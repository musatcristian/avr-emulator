#ifndef AVR_CPU_H
#define AVR_CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  AVR_REGISTER_COUNT = 32,
  AVR_FLASH_SIZE = 1024,
  AVR_SRAM_SIZE = 256,
  AVR_IO_PINB = 0,
  AVR_IO_DDRB = 1,
  AVR_IO_PORTB = 2,
  AVR_SREG_C = UINT8_C(1) << 0,
  AVR_SREG_Z = UINT8_C(1) << 1,
  AVR_SREG_N = UINT8_C(1) << 2,
  AVR_SREG_V = UINT8_C(1) << 3,
  AVR_SREG_S = UINT8_C(1) << 4,
  AVR_SREG_H = UINT8_C(1) << 5,
  AVR_SREG_T = UINT8_C(1) << 6,
  AVR_SREG_I = UINT8_C(1) << 7
};

typedef struct
{
  uint8_t registers[AVR_REGISTER_COUNT];
  /* AVR program counters address instruction words, not bytes. */
  uint16_t pc;
  uint8_t sreg;
  /* Simulated Flash memory: array of 16-bit instruction words. */
  uint16_t flash[AVR_FLASH_SIZE];
  /* Simulated SRAM memory: byte-addressable zero-based array. */
  uint8_t sram[AVR_SRAM_SIZE];
  uint8_t ddrb;
  uint8_t portb;
  uint8_t external_input;
  /* Stack pointer: address of the next free SRAM byte below the stack top;
   * reset points one past the last valid SRAM address (empty stack). */
  uint16_t sp;
  /* Abstract cycle counter: incremented by one per successfully executed
   * instruction; never advanced by wall-clock time. */
  uint32_t cycle_count;
  bool breakpoints[AVR_FLASH_SIZE];
} AvrMCU;

typedef enum
{
  AVR_RUN_STOP_CYCLE_LIMIT,
  AVR_RUN_STOP_BREAKPOINT,
  AVR_RUN_STOP_INVALID_INSTRUCTION
} AvrRunStopReason;

typedef struct
{
  AvrRunStopReason reason;
  uint32_t cycles_executed;
} AvrRunResult;

AvrMCU avr_mcu_create(void);
void avr_mcu_reset(AvrMCU *cpu);

bool avr_mcu_read_register(const AvrMCU *cpu, uint8_t register_index,
                           uint8_t *value);
bool avr_mcu_write_register(AvrMCU *cpu, uint8_t register_index,
                            uint8_t value);

uint16_t avr_mcu_read_pc(const AvrMCU *cpu);
void avr_mcu_write_pc(AvrMCU *cpu, uint16_t value);

uint8_t avr_mcu_read_sreg(const AvrMCU *cpu);
void avr_mcu_write_sreg(AvrMCU *cpu, uint8_t value);

uint16_t avr_mcu_read_sp(const AvrMCU *cpu);
void avr_mcu_write_sp(AvrMCU *cpu, uint16_t value);

bool avr_mcu_read_flash(const AvrMCU *cpu, uint16_t address,
                        uint16_t *instruction);
bool avr_mcu_write_flash(AvrMCU *cpu, uint16_t address,
                         uint16_t instruction);
bool avr_mcu_read_data(const AvrMCU *mcu, uint16_t address, uint8_t *value);
bool avr_mcu_write_data(AvrMCU *mcu, uint16_t address, uint8_t value);
bool avr_mcu_read_io(const AvrMCU *mcu, uint8_t address, uint8_t *value);
bool avr_mcu_write_io(AvrMCU *mcu, uint8_t address, uint8_t value);
bool avr_mcu_read_external_input(const AvrMCU *mcu, uint8_t *value);
bool avr_mcu_write_external_input(AvrMCU *mcu, uint8_t value);
bool avr_mcu_load_program(AvrMCU *mcu, const uint16_t *program,
                          size_t instruction_count);
/* Parses an Intel HEX (record type 00/01 only) image into Flash; rejects the
 * whole image, leaving Flash untouched, if any record is malformed. */
bool avr_mcu_load_intel_hex(AvrMCU *mcu, const char *hex_text);
bool avr_mcu_step(AvrMCU *mcu);

uint32_t avr_mcu_read_cycle_count(const AvrMCU *mcu);

bool avr_mcu_set_breakpoint(AvrMCU *mcu, uint16_t address);
bool avr_mcu_clear_breakpoint(AvrMCU *mcu, uint16_t address);
bool avr_mcu_has_breakpoint(const AvrMCU *mcu, uint16_t address);

/* Executes up to max_cycles instructions, stopping early on a breakpoint
 * (other than one already at the starting pc) or an invalid instruction.
 * Equivalent to calling avr_mcu_step that many times when neither occurs. */
AvrRunResult avr_mcu_run(AvrMCU *mcu, uint32_t max_cycles);

#endif
