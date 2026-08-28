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

    mvprintw(row, column, "REGISTERS");
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

static void draw_bits(int row, int column, const char *label, uint8_t value,
                      uint8_t outputs)
{
    int bit;

    mvprintw(row, column, "%s %02X ", label, value);
    for (bit = 7; bit >= 0; --bit)
    {
        bool high = (value & (UINT8_C(1) << bit)) != 0;
        bool output = (outputs & (UINT8_C(1) << bit)) != 0;
        if (high)
        {
            attron(COLOR_PAIR(1));
        }
        if (output)
        {
            attron(A_BOLD);
        }
        printw("%c", high ? '1' : '0');
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
}

static void draw_sram(const TuiState *state, int row, int column)
{
    uint16_t address;

    mvprintw(row, column, "SRAM 00-3F");
    for (address = 0; address < 64; ++address)
    {
        int memory_row = row + 1 + address / 16;
        int memory_column = column + (address % 16) * 3;
        if (value_changed(state, state->snapshot.sram[address],
                          state->previous_snapshot.sram[address]))
        {
            attron(A_REVERSE);
        }
        mvprintw(memory_row, memory_column, "%02X", state->snapshot.sram[address]);
        attroff(A_REVERSE);
    }
}

static void draw_ui(const TuiState *state)
{
    char instruction[64] = "<invalid instruction>";
    int bit;

    erase();
    attron(A_BOLD);
    mvprintw(0, 0, "AVR Emulator TUI");
    attroff(A_BOLD);
    mvprintw(0, 28, "[%s] %s", state->running ? "RUN" : "PAUSED", state->status);

    if (state->snapshot.instruction_valid)
    {
        avr_debug_format_instruction(&state->snapshot.instruction,
                                     state->snapshot.pc, instruction,
                                     sizeof(instruction));
    }
    attron(A_REVERSE);
    mvprintw(2, 0, "PC %04X  WORD %04X  %s%s", state->snapshot.pc,
             state->snapshot.instruction_word, instruction,
             state->snapshot.breakpoint_at_pc ? "  BREAKPOINT" : "");
    attroff(A_REVERSE);
    mvprintw(3, 0, "SP %04X  CYCLES %u  SREG %02X [I T H S V N Z C] ",
             state->snapshot.sp, state->snapshot.cycle_count, state->snapshot.sreg);
    for (bit = 7; bit >= 0; --bit)
    {
        printw("%c", (state->snapshot.sreg & (UINT8_C(1) << bit)) ? '1' : '0');
    }

    draw_registers(state, 5, 0);
    draw_sram(state, 11, 0);
    draw_bits(17, 0, "DDRB", state->snapshot.ddrb, UINT8_C(0));
    draw_bits(18, 0, "PORTB", state->snapshot.portb, state->snapshot.ddrb);
    draw_bits(19, 0, "PINB", state->snapshot.pinb, state->snapshot.ddrb);
    draw_bits(20, 0, "INPUT", state->snapshot.external_input, UINT8_C(0));
    mvprintw(22, 0, "PB7 PB6 PB5 PB4 PB3 PB2 PB1 PB0 \n");
    for (bit = 7; bit >= 0; --bit)
    {
        bool high = (state->snapshot.pinb & (UINT8_C(1) << bit)) != 0;
        bool output = (state->snapshot.ddrb & (UINT8_C(1) << bit)) != 0;
        if (high)
        {
            attron(COLOR_PAIR(1));
        }
        if (output)
        {
            attron(A_BOLD);
        }
        printw(" %c  ", high ? '^' : 'v');
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
    mvprintw(24, 0, "\ns step  r run/pause  x reset  b breakpoint  0-7 toggle input  q quit");
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
