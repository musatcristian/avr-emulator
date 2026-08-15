#include "avr_cpu.h"

AvrCpu avr_cpu_create(void)
{
  return (AvrCpu){0};
}

void avr_cpu_reset(AvrCpu *cpu)
{
  *cpu = avr_cpu_create();
}

bool avr_cpu_read_register(const AvrCpu *cpu, uint8_t register_index,
                           uint8_t *value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  *value = cpu->registers[register_index];
  return true;
}

bool avr_cpu_write_register(AvrCpu *cpu, uint8_t register_index,
                            uint8_t value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  cpu->registers[register_index] = value;
  return true;
}

uint16_t avr_cpu_read_pc(const AvrCpu *cpu)
{
  return cpu->pc;
}

void avr_cpu_write_pc(AvrCpu *cpu, uint16_t value)
{
  cpu->pc = value;
}

uint8_t avr_cpu_read_sreg(const AvrCpu *cpu)
{
  return cpu->sreg;
}

void avr_cpu_write_sreg(AvrCpu *cpu, uint8_t value)
{
  cpu->sreg = value;
}