#include <stddef.h>
#include "avr_cpu.h"



AvrMCU avr_cpu_create(void)
{
  return (AvrMCU){0};
}

void avr_cpu_reset(AvrMCU *cpu)
{
  *cpu = avr_cpu_create();
}

bool avr_cpu_read_register(const AvrMCU *cpu, uint8_t register_index,
                           uint8_t *value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  *value = cpu->registers[register_index];
  return true;
}

bool avr_cpu_write_register(AvrMCU *cpu, uint8_t register_index,
                            uint8_t value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  cpu->registers[register_index] = value;
  return true;
}

uint16_t avr_cpu_read_pc(const AvrMCU *cpu)
{
  return cpu->pc;
}

void avr_cpu_write_pc(AvrMCU *cpu, uint16_t value)
{
  cpu->pc = value;
}

uint8_t avr_cpu_read_sreg(const AvrMCU *cpu)
{
  return cpu->sreg;
}

void avr_cpu_write_sreg(AvrMCU *cpu, uint8_t value)
{
  cpu->sreg = value;
}

bool avr_cpu_read_flash(const AvrMCU *cpu, uint16_t address,
                        uint16_t *instruction)
{
  if (address >= AVR_FLASH_SIZE || cpu == NULL || instruction == NULL)
  {
    return false;
  }

  *instruction = cpu->flash[address];
  return true;
}

bool avr_cpu_write_flash(AvrMCU *cpu, uint16_t address,
                         uint16_t instruction)
{
  if (address >= AVR_FLASH_SIZE || cpu == NULL)
  {
    return false;
  }

  cpu->flash[address] = instruction;
  return true;
}
