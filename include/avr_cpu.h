#ifndef AVR_CPU_H
#define AVR_CPU_H

#include <stdbool.h>
#include <stdint.h>

enum
{
  AVR_REGISTER_COUNT = 32,
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
} AvrCpu;

AvrCpu avr_cpu_create(void);
void avr_cpu_reset(AvrCpu *cpu);

bool avr_cpu_read_register(const AvrCpu *cpu, uint8_t register_index,
                           uint8_t *value);
bool avr_cpu_write_register(AvrCpu *cpu, uint8_t register_index,
                            uint8_t value);

uint16_t avr_cpu_read_pc(const AvrCpu *cpu);
void avr_cpu_write_pc(AvrCpu *cpu, uint16_t value);

uint8_t avr_cpu_read_sreg(const AvrCpu *cpu);
void avr_cpu_write_sreg(AvrCpu *cpu, uint8_t value);

#endif
