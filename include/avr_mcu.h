#ifndef AVR_CPU_H
#define AVR_CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  AVR_REGISTER_COUNT = 32,
  AVR_FLASH_SIZE = 1024,
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
} AvrMCU;

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

bool avr_mcu_read_flash(const AvrMCU *cpu, uint16_t address,
                        uint16_t *instruction);
bool avr_mcu_write_flash(AvrMCU *cpu, uint16_t address,
                         uint16_t instruction);
bool avr_mcu_load_program(AvrMCU *mcu, const uint16_t *program,
                          size_t instruction_count);

#endif
