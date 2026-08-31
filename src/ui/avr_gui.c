/* Optional SDL3 graphical frontend (master-plan Phase 12). Reuses the same
 * emulator/debug APIs as the TUI (src/ui/avr_tui.c); implements no CPU
 * semantics of its own. Requires SDL3 + SDL3_ttf ("make gui"). */
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "avr_debug.h"

enum
{
    GUI_WINDOW_WIDTH = 1200,
    GUI_WINDOW_HEIGHT = 860,
    GUI_MARGIN = 16,
    GUI_GUTTER = 16,
    GUI_TOOLBAR_HEIGHT = 56,
    GUI_STATUS_SIZE = 96,
    GUI_FONT_SIZE = 14,
    GUI_LINE_HEIGHT = 18,
    GUI_PROGRAM_LISTING_ROWS = 10,
    GUI_FRAME_DELAY_MS = 16
};

/* Panel layout, derived once from the constants above (see memory notes in
 * plan/master-plan Phase 12 section for the column math). */
static const SDL_FRect LAYOUT_TOOLBAR = {0, 0, GUI_WINDOW_WIDTH, GUI_TOOLBAR_HEIGHT};
static const SDL_FRect LAYOUT_PROGRAM_MEMORY = {16, 72, 300, 300};
static const SDL_FRect LAYOUT_REGISTERS = {332, 72, 420, 300};
static const SDL_FRect LAYOUT_GPIO = {768, 72, 416, 546};
static const SDL_FRect LAYOUT_INSTRUCTION_EXPLANATION = {16, 388, 300, 230};
static const SDL_FRect LAYOUT_STATUS_FLAGS = {332, 388, 200, 230};
static const SDL_FRect LAYOUT_CPU_INFO = {548, 388, 204, 230};
static const SDL_FRect LAYOUT_SRAM = {16, 634, 736, 210};
static const SDL_FRect LAYOUT_CHANGE_TRACKER = {768, 634, 416, 210};

static const SDL_FRect BUTTON_STEP = {16, 12, 90, 32};
static const SDL_FRect BUTTON_RUN = {118, 12, 90, 32};
static const SDL_FRect BUTTON_RESET = {220, 12, 90, 32};

static const SDL_Color COLOR_BACKGROUND = {18, 20, 26, 255};
static const SDL_Color COLOR_PANEL_BORDER = {70, 76, 90, 255};
static const SDL_Color COLOR_TEXT = {225, 228, 235, 255};
static const SDL_Color COLOR_DIM_TEXT = {140, 145, 155, 255};
static const SDL_Color COLOR_HEADER = {120, 190, 255, 255};
static const SDL_Color COLOR_ACCENT = {255, 180, 90, 255};
static const SDL_Color COLOR_HIGH = {70, 210, 110, 255};
static const SDL_Color COLOR_LOW = {90, 95, 105, 255};
static const SDL_Color COLOR_CHANGED = {230, 210, 60, 255};
static const SDL_Color COLOR_BREAKPOINT = {220, 60, 60, 255};
static const SDL_Color COLOR_BUTTON = {40, 44, 54, 255};
static const SDL_Color COLOR_BUTTON_ACTIVE = {60, 110, 70, 255};

typedef struct
{
    AvrMCU mcu;
    AvrSnapshot snapshot;
    AvrSnapshot previous_snapshot;
    bool snapshot_valid;
    bool has_previous_snapshot;
    AvrEventLog last_events;
    bool running;
    double speed_hz;
    Uint64 last_step_ticks;
    char status[GUI_STATUS_SIZE];
    bool quit;
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
} GuiState;

/* Same demo program as the TUI (src/ui/avr_tui.c): configures PB5 as an
 * output, then mirrors an external input bit onto it in a loop. */
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

static const char *find_font_path(void)
{
    static const char *candidates[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/Library/Fonts/Arial.ttf",
    };
    size_t index;

    for (index = 0; index < sizeof(candidates) / sizeof(candidates[0]); ++index)
    {
        FILE *file = fopen(candidates[index], "rb");
        if (file != NULL)
        {
            fclose(file);
            return candidates[index];
        }
    }
    return NULL;
}

static bool point_in_rect(const SDL_FRect *rect, float x, float y)
{
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static void fill_rect(SDL_Renderer *renderer, const SDL_FRect *rect, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, rect);
}

static void outline_rect(SDL_Renderer *renderer, const SDL_FRect *rect, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(renderer, rect);
}

static void fill_circle(SDL_Renderer *renderer, float cx, float cy, float radius, SDL_Color color)
{
    float dx;
    float dy;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (dy = -radius; dy <= radius; dy += 1.0f)
    {
        for (dx = -radius; dx <= radius; dx += 1.0f)
        {
            if (dx * dx + dy * dy <= radius * radius)
            {
                SDL_RenderPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

/* Draws text at (x, y) and returns the pixel width consumed, or 0 on failure. */
static float draw_text(GuiState *state, float x, float y, SDL_Color color, const char *fmt, ...)
{
    char buffer[160];
    va_list args;
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_FRect dest;
    float width;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (buffer[0] == '\0' || state->font == NULL)
    {
        return 0;
    }

    surface = TTF_RenderText_Blended(state->font, buffer, strlen(buffer), color);
    if (surface == NULL)
    {
        return 0;
    }
    texture = SDL_CreateTextureFromSurface(state->renderer, surface);
    width = (float)surface->w;
    dest.x = x;
    dest.y = y;
    dest.w = (float)surface->w;
    dest.h = (float)surface->h;
    SDL_DestroySurface(surface);
    if (texture != NULL)
    {
        SDL_RenderTexture(state->renderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
    }
    return width;
}

static void draw_panel_frame(SDL_Renderer *renderer, const SDL_FRect *rect, const char *title,
                             TTF_Font *font)
{
    (void)title;
    (void)font;
    fill_rect(renderer, rect, (SDL_Color){26, 29, 37, 255});
    outline_rect(renderer, rect, COLOR_PANEL_BORDER);
}

static bool value_changed_u8(const GuiState *state, uint8_t value, uint8_t previous)
{
    return state->has_previous_snapshot && value != previous;
}

static void draw_toolbar(GuiState *state)
{
    SDL_Color step_color = COLOR_BUTTON;
    SDL_Color run_color = state->running ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON;

    fill_rect(state->renderer, &LAYOUT_TOOLBAR, (SDL_Color){12, 13, 18, 255});

    fill_rect(state->renderer, &BUTTON_STEP, step_color);
    outline_rect(state->renderer, &BUTTON_STEP, COLOR_PANEL_BORDER);
    draw_text(state, BUTTON_STEP.x + 14, BUTTON_STEP.y + 8, COLOR_TEXT, "Step (s)");

    fill_rect(state->renderer, &BUTTON_RUN, run_color);
    outline_rect(state->renderer, &BUTTON_RUN, COLOR_PANEL_BORDER);
    draw_text(state, BUTTON_RUN.x + 14, BUTTON_RUN.y + 8, COLOR_TEXT,
              state->running ? "Pause (r)" : "Run (r)");

    fill_rect(state->renderer, &BUTTON_RESET, COLOR_BUTTON);
    outline_rect(state->renderer, &BUTTON_RESET, COLOR_PANEL_BORDER);
    draw_text(state, BUTTON_RESET.x + 10, BUTTON_RESET.y + 8, COLOR_TEXT, "Reset (x)");

    draw_text(state, 330, 22, COLOR_DIM_TEXT, "Speed: %.2fx ([ / ])", state->speed_hz);

    draw_text(state, 560, 12, COLOR_DIM_TEXT, "State:");
    draw_text(state, 610, 12, state->running ? COLOR_HIGH : COLOR_ACCENT,
              state->running ? "RUNNING" : "PAUSED");
    draw_text(state, 560, 30, COLOR_DIM_TEXT, "Cycles: %u   PC: 0x%04x   SP: 0x%04x",
              state->snapshot.cycle_count, state->snapshot.pc, state->snapshot.sp);

    draw_text(state, 900, 12, COLOR_DIM_TEXT, "%s", state->status);
}

static void draw_program_memory(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_PROGRAM_MEMORY;
    int32_t start;
    int32_t address;
    int row = 0;

    draw_panel_frame(state->renderer, rect, "PROGRAM MEMORY", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "PROGRAM MEMORY");
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT, COLOR_DIM_TEXT,
              "Addr    Instruction                Bytes");

    start = (int32_t)state->snapshot.pc - 2;
    if (start < 0)
    {
        start = 0;
    }

    for (address = start; row < GUI_PROGRAM_LISTING_ROWS &&
                          address < (int32_t)AVR_FLASH_SIZE;
         ++address, ++row)
    {
        float row_y = rect->y + 8 + 2 * GUI_LINE_HEIGHT + (float)row * GUI_LINE_HEIGHT;
        uint16_t word;
        AvrInstruction instruction;
        char formatted[64] = "<invalid>";
        bool is_current = (uint16_t)address == state->snapshot.pc;
        bool has_breakpoint = avr_mcu_has_breakpoint(&state->mcu, (uint16_t)address);

        if (!avr_mcu_read_flash(&state->mcu, (uint16_t)address, &word))
        {
            break;
        }
        if (avr_decode_instruction_word(word, &instruction))
        {
            avr_debug_format_instruction(&instruction, (uint16_t)address, formatted,
                                         sizeof(formatted));
        }

        if (is_current)
        {
            SDL_FRect highlight = {rect->x + 6, row_y - 2, rect->w - 12, GUI_LINE_HEIGHT};
            outline_rect(state->renderer, &highlight, COLOR_ACCENT);
        }
        if (has_breakpoint)
        {
            fill_circle(state->renderer, rect->x + 14, row_y + GUI_LINE_HEIGHT / 2.0f - 2, 4,
                        COLOR_BREAKPOINT);
        }

        draw_text(state, rect->x + 30, row_y, is_current ? COLOR_ACCENT : COLOR_TEXT,
                  "0x%04x  %-20s %02x%02x", (unsigned)address, formatted,
                  (unsigned)(word & 0xff), (unsigned)(word >> 8));
    }
}

static void draw_instruction_explanation(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_INSTRUCTION_EXPLANATION;
    char mnemonic[64] = "<invalid instruction>";
    char explanation[128] = "";

    draw_panel_frame(state->renderer, rect, "INSTRUCTION EXPLANATION", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "INSTRUCTION EXPLANATION");

    if (state->snapshot.instruction_valid)
    {
        avr_debug_format_instruction(&state->snapshot.instruction, state->snapshot.pc, mnemonic,
                                     sizeof(mnemonic));
        avr_debug_explain_instruction(&state->snapshot.instruction, state->snapshot.pc,
                                      explanation, sizeof(explanation));
    }

    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 2, COLOR_ACCENT, "%s",
              mnemonic);
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 3, COLOR_TEXT, "%s",
              explanation);
}

static void draw_registers(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_REGISTERS;
    uint8_t index;
    uint8_t active_a = 0xff;
    uint8_t active_b = 0xff;

    draw_panel_frame(state->renderer, rect, "REGISTERS", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "REGISTERS");

    if (state->snapshot.instruction_valid)
    {
        active_a = state->snapshot.instruction.destination_register;
        active_b = state->snapshot.instruction.source_register;
    }

    for (index = 0; index < AVR_REGISTER_COUNT; ++index)
    {
        float column = rect->x + 10 + (index % 4) * 100.0f;
        float row = rect->y + 8 + GUI_LINE_HEIGHT + (index / 4) * GUI_LINE_HEIGHT;
        bool changed = value_changed_u8(state, state->snapshot.registers[index],
                                        state->previous_snapshot.registers[index]);
        bool active = (index == active_a || index == active_b);
        SDL_Color color = changed ? COLOR_CHANGED : (active ? COLOR_ACCENT : COLOR_TEXT);

        draw_text(state, column, row, color, "R%-2u:%02X", index, state->snapshot.registers[index]);
    }
}

static void draw_flag_cell(GuiState *state, float x, float y, uint8_t mask)
{
    bool on = (state->snapshot.sreg & mask) != 0;

    fill_circle(state->renderer, x + 6, y + 6, 6, on ? COLOR_HIGH : COLOR_LOW);
    draw_text(state, x + 20, y, on ? COLOR_TEXT : COLOR_DIM_TEXT, "%s: %s",
              avr_debug_flag_name(mask), on ? "ON" : "OFF");
}

static void draw_status_flags(GuiState *state)
{
    static const uint8_t flags[8] = {AVR_SREG_I, AVR_SREG_T, AVR_SREG_H, AVR_SREG_S,
                                     AVR_SREG_V, AVR_SREG_N, AVR_SREG_Z, AVR_SREG_C};
    const SDL_FRect *rect = &LAYOUT_STATUS_FLAGS;
    int index;

    draw_panel_frame(state->renderer, rect, "STATUS FLAGS (SREG)", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "STATUS FLAGS");

    for (index = 0; index < 8; ++index)
    {
        float row = rect->y + 8 + GUI_LINE_HEIGHT + (float)index * GUI_LINE_HEIGHT;
        draw_flag_cell(state, rect->x + 10, row, flags[index]);
    }
}

static void draw_cpu_info(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_CPU_INFO;

    draw_panel_frame(state->renderer, rect, "CPU INFO", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "CPU INFO");
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 2, COLOR_TEXT,
              "Cycles Executed: %u", state->snapshot.cycle_count);
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 3, COLOR_TEXT,
              "Interrupt Enable: %s", (state->snapshot.sreg & AVR_SREG_I) ? "ON" : "OFF");
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 4, COLOR_TEXT,
              "T-bit: %s", (state->snapshot.sreg & AVR_SREG_T) ? "ON" : "OFF");
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 5,
              state->running ? COLOR_HIGH : COLOR_ACCENT, "State: %s",
              state->running ? "RUNNING" : "PAUSED");
}

/* Shared by drawing and mouse hit-testing so both agree on pin row bounds. */
static SDL_FRect gpio_pin_row_rect(int bit)
{
    SDL_FRect rect = {LAYOUT_GPIO.x + 10,
                      LAYOUT_GPIO.y + 8 + 2 * GUI_LINE_HEIGHT +
                          (float)(7 - bit) * GUI_LINE_HEIGHT,
                      LAYOUT_GPIO.w - 20, GUI_LINE_HEIGHT};
    return rect;
}

static void draw_gpio_table(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_GPIO;
    int bit;
    uint8_t highlighted_bit = 0xff;

    draw_panel_frame(state->renderer, rect, "GPIO - PORT B", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "GPIO - PORT B");
    draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT, COLOR_DIM_TEXT,
              "Pin  Dir  Latch  Read  Level");

    if (state->snapshot.instruction_valid &&
        (state->snapshot.instruction.operation == AVR_OPERATION_SBI ||
         state->snapshot.instruction.operation == AVR_OPERATION_CBI) &&
        state->snapshot.instruction.immediate == AVR_IO_PORTB)
    {
        highlighted_bit = state->snapshot.instruction.bit_index;
    }

    for (bit = 7; bit >= 0; --bit)
    {
        SDL_FRect row_rect = gpio_pin_row_rect(bit);
        bool is_output = (state->snapshot.ddrb & (UINT8_C(1) << bit)) != 0;
        bool is_high = (state->snapshot.pinb & (UINT8_C(1) << bit)) != 0;
        SDL_Color led_color = is_high ? COLOR_HIGH : COLOR_LOW;

        if (bit == highlighted_bit)
        {
            outline_rect(state->renderer, &row_rect, COLOR_ACCENT);
        }

        draw_text(state, row_rect.x, row_rect.y, COLOR_TEXT, "PB%-2d  %-4s %-6d %-5d",
                  bit, is_output ? "OUT" : "IN",
                  (state->snapshot.portb >> bit) & 1, (state->snapshot.pinb >> bit) & 1);
        fill_circle(state->renderer, row_rect.x + 220, row_rect.y + GUI_LINE_HEIGHT / 2.0f, 6,
                    led_color);
        draw_text(state, row_rect.x + 235, row_rect.y, COLOR_DIM_TEXT, is_high ? "High" : "Low");
    }

    draw_text(state, rect->x + 10, rect->y + 8 + 11 * GUI_LINE_HEIGHT, COLOR_DIM_TEXT,
              "DDRB:0x%02X PORTB:0x%02X PINB:0x%02X INPUT:0x%02X", state->snapshot.ddrb,
              state->snapshot.portb, state->snapshot.pinb, state->snapshot.external_input);
    draw_text(state, rect->x + 10, rect->y + 8 + 12 * GUI_LINE_HEIGHT, COLOR_DIM_TEXT,
              "Click an input pin's level to toggle it, or press 0-7.");
}

static void draw_sram(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_SRAM;
    uint16_t address;

    draw_panel_frame(state->renderer, rect, "SRAM DATA MEMORY", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "SRAM DATA MEMORY (0x00-0x3F)");

    for (address = 0; address < 64; ++address)
    {
        float row_y = rect->y + 8 + GUI_LINE_HEIGHT + (address / 16) * GUI_LINE_HEIGHT;
        float column_x = rect->x + 60 + (address % 16) * 32.0f;
        bool changed = value_changed_u8(state, state->snapshot.sram[address],
                                        state->previous_snapshot.sram[address]);
        SDL_Color color = changed ? COLOR_CHANGED
                                  : (state->snapshot.sram[address] == 0 ? COLOR_DIM_TEXT
                                                                        : COLOR_TEXT);

        if (address % 16 == 0)
        {
            draw_text(state, rect->x + 10, row_y, COLOR_DIM_TEXT, "0x%02X", address);
        }
        draw_text(state, column_x, row_y, color, "%02X", state->snapshot.sram[address]);
    }
}

static const char *event_type_label(AvrEventType type)
{
    switch (type)
    {
    case AVR_EVENT_REGISTER_CHANGED:
        return "Register";
    case AVR_EVENT_SREG_CHANGED:
        return "Flags";
    case AVR_EVENT_SRAM_WRITTEN:
        return "SRAM";
    case AVR_EVENT_IO_CHANGED:
        return "I/O Register";
    case AVR_EVENT_PIN_CHANGED:
        return "Pin";
    case AVR_EVENT_PC_CHANGED:
    default:
        return "PC";
    }
}

static void draw_change_tracker(GuiState *state)
{
    const SDL_FRect *rect = &LAYOUT_CHANGE_TRACKER;
    size_t index;
    int row = 0;
    const int max_rows = 9;

    draw_panel_frame(state->renderer, rect, "CHANGE TRACKER (THIS STEP)", state->font);
    draw_text(state, rect->x + 10, rect->y + 8, COLOR_HEADER, "CHANGE TRACKER (THIS STEP)");

    if (state->snapshot.instruction_valid)
    {
        char mnemonic[64] = "<invalid>";
        avr_debug_format_instruction(&state->previous_snapshot.instruction,
                                     state->previous_snapshot.pc, mnemonic, sizeof(mnemonic));
        draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT, COLOR_DIM_TEXT,
                  "Last instruction: %s", mnemonic);
    }

    for (index = 0; index < state->last_events.count && row < max_rows; ++index)
    {
        const AvrEvent *event = &state->last_events.events[index];
        float row_y = rect->y + 8 + GUI_LINE_HEIGHT * 2 + (float)row * GUI_LINE_HEIGHT;

        if (event->type == AVR_EVENT_PC_CHANGED)
        {
            continue;
        }

        if (event->type == AVR_EVENT_PIN_CHANGED)
        {
            int bit;
            for (bit = 0; bit < 8 && row < max_rows; ++bit)
            {
                bool old_high = (event->old_value & (1 << bit)) != 0;
                bool new_high = (event->new_value & (1 << bit)) != 0;
                if (old_high == new_high)
                {
                    continue;
                }
                row_y = rect->y + 8 + GUI_LINE_HEIGHT * 2 + (float)row * GUI_LINE_HEIGHT;
                draw_text(state, rect->x + 10, row_y, COLOR_TEXT, "Pin PB%d: %s -> %s", bit,
                          old_high ? "High" : "Low", new_high ? "High" : "Low");
                ++row;
            }
            continue;
        }

        draw_text(state, rect->x + 10, row_y, COLOR_TEXT, "%s [%u]: 0x%02X -> 0x%02X",
                  event_type_label(event->type), event->address,
                  (unsigned)event->old_value, (unsigned)event->new_value);
        ++row;
    }

    if (row == 0)
    {
        draw_text(state, rect->x + 10, rect->y + 8 + GUI_LINE_HEIGHT * 2, COLOR_DIM_TEXT,
                  "No changes this step.");
    }
}

static void set_status(GuiState *state, const char *message)
{
    snprintf(state->status, sizeof(state->status), "%s", message);
}

static void refresh_snapshot(GuiState *state)
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

static void step_once(GuiState *state)
{
    if (!avr_debug_step_with_events(&state->mcu, &state->last_events))
    {
        state->running = false;
        set_status(state, "Stopped: invalid instruction");
    }
    else
    {
        snprintf(state->status, sizeof(state->status), "Stepped: %zu change(s)",
                 state->last_events.count);
    }
    refresh_snapshot(state);
}

static void reset_demo(GuiState *state)
{
    avr_mcu_reset(&state->mcu);
    load_demo(&state->mcu);
    state->running = false;
    memset(&state->last_events, 0, sizeof(state->last_events));
    set_status(state, "Demo reset");
    refresh_snapshot(state);
}

static void handle_key_down(GuiState *state, SDL_Keycode key)
{
    if (key == SDLK_Q || key == SDLK_ESCAPE)
    {
        state->quit = true;
    }
    else if (key == SDLK_S)
    {
        state->running = false;
        step_once(state);
    }
    else if (key == SDLK_R)
    {
        state->running = !state->running;
        set_status(state, state->running ? "Running" : "Paused");
    }
    else if (key == SDLK_X)
    {
        reset_demo(state);
    }
    else if (key == SDLK_B)
    {
        if (avr_mcu_has_breakpoint(&state->mcu, state->snapshot.pc))
        {
            avr_mcu_clear_breakpoint(&state->mcu, state->snapshot.pc);
            set_status(state, "Breakpoint cleared");
        }
        else
        {
            avr_mcu_set_breakpoint(&state->mcu, state->snapshot.pc);
            set_status(state, "Breakpoint set");
        }
    }
    else if (key == SDLK_LEFTBRACKET)
    {
        state->speed_hz = state->speed_hz > 0.5 ? state->speed_hz / 2.0 : 0.25;
    }
    else if (key == SDLK_RIGHTBRACKET)
    {
        state->speed_hz = state->speed_hz < 8.0 ? state->speed_hz * 2.0 : 16.0;
    }
    else if (key >= SDLK_0 && key <= SDLK_7)
    {
        uint8_t input = state->snapshot.external_input;
        int bit = (int)(key - SDLK_0);
        input ^= (uint8_t)(UINT8_C(1) << bit);
        avr_mcu_write_external_input(&state->mcu, input);
        snprintf(state->status, sizeof(state->status), "Input PB%d toggled", bit);
        refresh_snapshot(state);
    }
}

static void handle_mouse_down(GuiState *state, float x, float y)
{
    int bit;

    if (point_in_rect(&BUTTON_STEP, x, y))
    {
        state->running = false;
        step_once(state);
        return;
    }
    if (point_in_rect(&BUTTON_RUN, x, y))
    {
        state->running = !state->running;
        set_status(state, state->running ? "Running" : "Paused");
        return;
    }
    if (point_in_rect(&BUTTON_RESET, x, y))
    {
        reset_demo(state);
        return;
    }

    for (bit = 0; bit < 8; ++bit)
    {
        SDL_FRect row_rect = gpio_pin_row_rect(bit);
        bool is_output = (state->snapshot.ddrb & (UINT8_C(1) << bit)) != 0;

        if (!is_output && point_in_rect(&row_rect, x, y))
        {
            uint8_t input = state->snapshot.external_input;
            input ^= (uint8_t)(UINT8_C(1) << bit);
            avr_mcu_write_external_input(&state->mcu, input);
            snprintf(state->status, sizeof(state->status), "Input PB%d toggled", bit);
            refresh_snapshot(state);
            return;
        }
    }
}

static void handle_event(GuiState *state, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        state->quit = true;
    }
    else if (event->type == SDL_EVENT_KEY_DOWN)
    {
        handle_key_down(state, event->key.key);
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        handle_mouse_down(state, event->button.x, event->button.y);
    }
}

static void render_frame(GuiState *state)
{
    fill_rect(state->renderer, &(SDL_FRect){0, 0, GUI_WINDOW_WIDTH, GUI_WINDOW_HEIGHT},
              COLOR_BACKGROUND);

    draw_toolbar(state);
    draw_program_memory(state);
    draw_registers(state);
    draw_gpio_table(state);
    draw_instruction_explanation(state);
    draw_status_flags(state);
    draw_cpu_info(state);
    draw_sram(state);
    draw_change_tracker(state);

    SDL_RenderPresent(state->renderer);
}

static bool gui_init(GuiState *state)
{
    const char *font_path;

    memset(state, 0, sizeof(*state));
    state->mcu = avr_mcu_create();
    state->speed_hz = 2.0;

    if (!load_demo(&state->mcu))
    {
        fprintf(stderr, "Failed to load GUI demo program\n");
        return false;
    }
    set_status(state, "Demo loaded");
    refresh_snapshot(state);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    if (!TTF_Init())
    {
        fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        return false;
    }

    state->window = SDL_CreateWindow("AVR-Visualizer (MVP)", GUI_WINDOW_WIDTH,
                                     GUI_WINDOW_HEIGHT, 0);
    if (state->window == NULL)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    state->renderer = SDL_CreateRenderer(state->window, NULL);
    if (state->renderer == NULL)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    font_path = find_font_path();
    if (font_path == NULL)
    {
        fprintf(stderr, "No usable font found for the GUI frontend\n");
        return false;
    }
    state->font = TTF_OpenFont(font_path, GUI_FONT_SIZE);
    if (state->font == NULL)
    {
        fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

static void gui_shutdown(GuiState *state)
{
    if (state->font != NULL)
    {
        TTF_CloseFont(state->font);
    }
    if (state->renderer != NULL)
    {
        SDL_DestroyRenderer(state->renderer);
    }
    if (state->window != NULL)
    {
        SDL_DestroyWindow(state->window);
    }
    TTF_Quit();
    SDL_Quit();
}

int main(void)
{
    GuiState state;

    if (!gui_init(&state))
    {
        gui_shutdown(&state);
        return 1;
    }

    state.last_step_ticks = SDL_GetTicks();

    while (!state.quit)
    {
        SDL_Event event;
        Uint64 now;
        double interval_ms;

        while (SDL_PollEvent(&event))
        {
            handle_event(&state, &event);
        }

        if (state.running)
        {
            now = SDL_GetTicks();
            interval_ms = 1000.0 / state.speed_hz;
            if ((double)(now - state.last_step_ticks) >= interval_ms)
            {
                step_once(&state);
                state.last_step_ticks = now;
            }
        }

        render_frame(&state);
        SDL_Delay(GUI_FRAME_DELAY_MS);
    }

    gui_shutdown(&state);
    return 0;
}
