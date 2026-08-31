#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "avr_debug.h"

enum
{
    TUI_RUN_BATCH_SIZE = 1,
    TUI_CYCLE_DELAY_MS = 500,
    TUI_STATUS_SIZE = 80,
    /* Minimum size for the single-column, full-width panel layout below. */
    TUI_MIN_COLS = 100,
    TUI_MIN_ROWS = 40,
};

typedef struct
{
    AvrMCU mcu;
    AvrSnapshot snapshot;
    AvrSnapshot previous_snapshot;
    bool snapshot_valid;
    bool has_previous_snapshot;
    bool running;
    char status[TUI_STATUS_SIZE];
} TuiState;

static const AvrInstruction demo_program[] = {
    {.operation = AVR_OPERATION_LDI, .destination_register = 16, .immediate = UINT8_C(0x20)},
    {.operation = AVR_OPERATION_OUT, .source_register = 16, .immediate = AVR_IO_DDRB},
    {.operation = AVR_OPERATION_LDI, .destination_register = 18, .immediate = UINT8_C(0x01)},
    {.operation = AVR_OPERATION_IN, .destination_register = 17, .immediate = AVR_IO_PINB},
    {.operation = AVR_OPERATION_AND, .destination_register = 17, .source_register = 18},
    {.operation = AVR_OPERATION_BREQ, .relative_offset = 2},
    {.operation = AVR_OPERATION_SBI, .immediate = AVR_IO_PORTB, .bit_index = 5},
    {.operation = AVR_OPERATION_RJMP, .relative_offset = 1},
    {.operation = AVR_OPERATION_CBI, .immediate = AVR_IO_PORTB, .bit_index = 5},
    {.operation = AVR_OPERATION_RJMP, .relative_offset = -7},
};

static bool load_demo(AvrMCU *mcu)
{
    uint16_t words[sizeof(demo_program) / sizeof(demo_program[0])];
    size_t index;

    for (index = 0; index < sizeof(demo_program) / sizeof(demo_program[0]); ++index)
    {
        if (!avr_encode_instruction(&demo_program[index], &words[index]))
        {
            return false;
        }
    }

    return avr_mcu_load_program(mcu, words, sizeof(words) / sizeof(words[0]));
}

static void refresh_snapshot(TuiState *state)
{
    if (state->snapshot_valid)
    {
        state->previous_snapshot = state->snapshot;
        state->has_previous_snapshot = true;
    }
    else
    {
        state->has_previous_snapshot = false;
    }
    state->snapshot_valid = avr_debug_snapshot(&state->mcu, &state->snapshot);
}

static bool value_changed(const TuiState *state, uint8_t value, uint8_t previous)
{
    return state->has_previous_snapshot && value != previous;
}

static void draw_registers(const TuiState *state, int row, int column)
{
    uint8_t index;

    mvprintw(row, column, "Registers (R0-R31)");
    for (index = 0; index < AVR_REGISTER_COUNT; ++index)
    {
        int register_row = row + 1 + index / 8;
        int register_column = column + (index % 8) * 9;
        if (value_changed(state, state->snapshot.registers[index],
                          state->previous_snapshot.registers[index]))
        {
            attron(A_REVERSE);
        }
        mvprintw(register_row, register_column, "R%-2u:%02X", index,
                 state->snapshot.registers[index]);
        attroff(A_REVERSE);
    }
}

static void draw_sram(const TuiState *state, int row, int column)
{
    uint16_t address;

    mvprintw(row, column, "Memory (SRAM), bytes 0-63 (0x00-0x3F)");
    for (address = 0; address < 64; ++address)
    {
        int memory_row = row + 1 + address / 16;
        int memory_column = column + (address % 16) * 3;
        bool changed = value_changed(state, state->snapshot.sram[address],
                                     state->previous_snapshot.sram[address]);

        if (changed)
        {
            attron(A_REVERSE);
        }
        else if (state->snapshot.sram[address] == 0)
        {
            attron(A_DIM);
        }
        mvprintw(memory_row, memory_column, "%02X", state->snapshot.sram[address]);
        attroff(A_REVERSE | A_DIM);
    }
}

static void draw_current_instruction(const TuiState *state, int row, int column)
{
    char instruction[64] = "<invalid instruction>";
    char explanation[96] = "";

    if (state->snapshot.instruction_valid)
    {
        avr_debug_format_instruction(&state->snapshot.instruction,
                                     state->snapshot.pc, instruction,
                                     sizeof(instruction));
        avr_debug_explain_instruction(&state->snapshot.instruction,
                                      state->snapshot.pc, explanation,
                                      sizeof(explanation));
    }

    attron(A_REVERSE);
    mvprintw(row, column, "Line %u (0x%04x): %s", state->snapshot.pc,
             state->snapshot.pc, instruction);
    attroff(A_REVERSE);

    if (explanation[0] != '\0')
    {
        mvprintw(row + 1, column, "%s", explanation);
    }

    attron(A_DIM);
    mvprintw(row + 2, column, "Machine word: 0x%04x%s",
             state->snapshot.instruction_word,
             state->snapshot.breakpoint_at_pc
                 ? "   Breakpoint is set on this line"
                 : "");
    attroff(A_DIM);
}

static void draw_flag_cell(int row, int column, uint8_t sreg, uint8_t mask)
{
    bool on = (sreg & mask) != 0;

    if (on)
    {
        attron(A_BOLD | COLOR_PAIR(1));
    }
    mvprintw(row, column, "%-16s: %-3s", avr_debug_flag_name(mask), on ? "ON" : "OFF");
    attroff(A_BOLD | COLOR_PAIR(1));
}

static void draw_processor_state(const TuiState *state, int row, int column)
{
    static const uint8_t top_row_flags[4] = {AVR_SREG_I, AVR_SREG_T, AVR_SREG_H, AVR_SREG_S};
    static const uint8_t bottom_row_flags[4] = {AVR_SREG_V, AVR_SREG_N, AVR_SREG_Z, AVR_SREG_C};
    int index;

    mvprintw(row, column, "Stack pointer: %u (0x%04x)      Cycles executed: %u",
             state->snapshot.sp, state->snapshot.sp, state->snapshot.cycle_count);

    for (index = 0; index < 4; ++index)
    {
        draw_flag_cell(row + 1, column + index * 24, state->snapshot.sreg,
                       top_row_flags[index]);
        draw_flag_cell(row + 2, column + index * 24, state->snapshot.sreg,
                       bottom_row_flags[index]);
    }
}

static void draw_gpio_table(const TuiState *state, int row, int column)
{
    int bit;
    int table_row = row;

    mvprintw(table_row++, column, "GPIO Port B");
    mvprintw(table_row++, column, "Pin   Direction  Level");
    for (bit = 7; bit >= 0; --bit)
    {
        bool is_output = (state->snapshot.ddrb & (UINT8_C(1) << bit)) != 0;
        bool is_high = (state->snapshot.pinb & (UINT8_C(1) << bit)) != 0;

        mvprintw(table_row, column, "PB%-2d  %-9s  ", bit, is_output ? "Output" : "Input");
        if (is_high)
        {
            attron(COLOR_PAIR(1));
        }
        if (is_output)
        {
            attron(A_BOLD);
        }
        printw("%s", is_high ? "High" : "Low");
        attroff(COLOR_PAIR(1) | A_BOLD);
        ++table_row;
    }

    attron(A_DIM);
    mvprintw(table_row++, column, "Raw: DDRB=0x%02X PORTB=0x%02X PINB=0x%02X INPUT=0x%02X",
             state->snapshot.ddrb, state->snapshot.portb, state->snapshot.pinb,
             state->snapshot.external_input);
    mvprintw(table_row, column, "Legend: green/bold = pin is HIGH and configured as an output");
    attroff(A_DIM);
}

static void draw_ui(const TuiState *state)
{
    int term_rows;
    int term_cols;

    getmaxyx(stdscr, term_rows, term_cols);

    erase();

    if (term_cols < TUI_MIN_COLS || term_rows < TUI_MIN_ROWS)
    {
        int message_row = term_rows > 0 ? term_rows / 2 : 0;

        mvprintw(message_row, 0,
                 "Terminal too small: resize to at least %dx%d (currently %dx%d).",
                 TUI_MIN_COLS, TUI_MIN_ROWS, term_cols, term_rows);
        refresh();
        return;
    }

    attron(A_BOLD);
    mvprintw(0, 0, "AVR Emulator TUI");
    attroff(A_BOLD);
    mvprintw(0, 28, "[%s] %s", state->running ? "RUN" : "PAUSED", state->status);

    draw_current_instruction(state, 2, 0);
    draw_processor_state(state, 6, 0);
    draw_registers(state, 10, 0);
    draw_sram(state, 16, 0);
    draw_gpio_table(state, 22, 0);

    mvprintw(35, 0, "s = Step   r = Run/Pause   x = Reset & reload demo   b = Toggle breakpoint");
    mvprintw(36, 0, "0-7 = Toggle input pin   q = Quit");
    refresh();
}

static void set_status(TuiState *state, const char *message)
{
    snprintf(state->status, sizeof(state->status), "%s", message);
}

static void step_once(TuiState *state)
{
    AvrEventLog events;

    if (!avr_debug_step_with_events(&state->mcu, &events))
    {
        state->running = false;
        set_status(state, "Stopped: invalid instruction");
    }
    else
    {
        snprintf(state->status, sizeof(state->status), "Stepped: %zu change(s)", events.count);
    }
    refresh_snapshot(state);
}

static void run_batch(TuiState *state)
{
    AvrRunResult result = avr_mcu_run(&state->mcu, TUI_RUN_BATCH_SIZE);

    if (result.reason == AVR_RUN_STOP_BREAKPOINT)
    {
        state->running = false;
        set_status(state, "Stopped: breakpoint");
    }
    else if (result.reason == AVR_RUN_STOP_INVALID_INSTRUCTION)
    {
        state->running = false;
        set_status(state, "Stopped: invalid instruction");
    }
    refresh_snapshot(state);
}

static void reset_demo(TuiState *state)
{
    avr_mcu_reset(&state->mcu);
    load_demo(&state->mcu);
    state->running = false;
    set_status(state, "Demo reset");
    refresh_snapshot(state);
}

int main(void)
{
    TuiState state;
    int key;

    memset(&state, 0, sizeof(state));
    state.mcu = avr_mcu_create();
    if (!load_demo(&state.mcu))
    {
        fprintf(stderr, "Failed to load TUI demo program\n");
        return 1;
    }
    set_status(&state, "Demo loaded");
    refresh_snapshot(&state);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    nodelay(stdscr, true);
    curs_set(0);
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
    }

    for (;;)
    {
        if (state.running)
        {
            run_batch(&state);
        }
        draw_ui(&state);
        if (state.running)
        {
            napms(TUI_CYCLE_DELAY_MS);
        }
        key = getch();
        if (key == ERR)
        {
            continue;
        }
        if (key == 'q')
        {
            break;
        }
        if (key == 's')
        {
            state.running = false;
            step_once(&state);
        }
        else if (key == 'r')
        {
            state.running = !state.running;
            set_status(&state, state.running ? "Running" : "Paused");
        }
        else if (key == 'x')
        {
            reset_demo(&state);
        }
        else if (key == 'b')
        {
            if (avr_mcu_has_breakpoint(&state.mcu, state.snapshot.pc))
            {
                avr_mcu_clear_breakpoint(&state.mcu, state.snapshot.pc);
                set_status(&state, "Breakpoint cleared");
            }
            else
            {
                avr_mcu_set_breakpoint(&state.mcu, state.snapshot.pc);
                set_status(&state, "Breakpoint set");
            }
            refresh_snapshot(&state);
        }
        else if (key >= '0' && key <= '7')
        {
            uint8_t input = state.snapshot.external_input;
            input ^= (uint8_t)(UINT8_C(1) << (key - '0'));
            avr_mcu_write_external_input(&state.mcu, input);
            snprintf(state.status, sizeof(state.status), "Input PB%d toggled", key - '0');
            refresh_snapshot(&state);
        }
    }

    endwin();
    return 0;
}
