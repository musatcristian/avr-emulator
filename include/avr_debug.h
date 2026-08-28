#ifndef AVR_DEBUG_H
#define AVR_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_instruction.h"
#include "avr_mcu.h"

/* Read-only view of everything a frontend needs to render one MCU state,
 * without reaching into AvrMCU's internal fields directly. */
typedef struct
{
    uint16_t pc;
    uint16_t instruction_word;
    AvrInstruction instruction;
    /* False if flash[pc] does not decode to a supported instruction. */
    bool instruction_valid;
    uint8_t registers[AVR_REGISTER_COUNT];
    uint8_t sreg;
    uint8_t sram[AVR_SRAM_SIZE];
    uint8_t ddrb;
    uint8_t portb;
    uint8_t pinb;
    uint8_t external_input;
    uint16_t sp;
    uint32_t cycle_count;
    bool breakpoint_at_pc;
} AvrSnapshot;

typedef enum
{
    AVR_EVENT_PC_CHANGED,
    AVR_EVENT_REGISTER_CHANGED,
    AVR_EVENT_SREG_CHANGED,
    AVR_EVENT_SRAM_WRITTEN,
    AVR_EVENT_IO_CHANGED,
    AVR_EVENT_PIN_CHANGED
} AvrEventType;

typedef struct
{
    AvrEventType type;
    /* Register index, SRAM address, or symbolic I/O id; unused for PC/SREG. */
    uint16_t address;
    uint16_t old_value;
    uint16_t new_value;
} AvrEvent;

enum
{
    AVR_EVENT_LOG_CAPACITY = 64
};

typedef struct
{
    AvrEvent events[AVR_EVENT_LOG_CAPACITY];
    size_t count;
} AvrEventLog;

bool avr_debug_snapshot(const AvrMCU *mcu, AvrSnapshot *out);

/* Formats a decoded instruction (register/I/O names, resolved branch
 * targets) into buffer; returns false if it does not fit. */
bool avr_debug_format_instruction(const AvrInstruction *instruction,
                                  uint16_t pc, char *buffer,
                                  size_t buffer_size);

/* Runs one avr_mcu_step and reports what changed by diffing snapshots taken
 * before and after, rather than hooking into instruction execution. Clears
 * events (if non-NULL) unconditionally, then repopulates it only if the
 * step succeeds. */
bool avr_debug_step_with_events(AvrMCU *mcu, AvrEventLog *events);

#endif
