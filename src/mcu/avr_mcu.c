#include <stddef.h>
#include "avr_mcu.h"



AvrMCU avr_mcu_create(void)
{
  return (AvrMCU){0};
}

void avr_mcu_reset(AvrMCU *mcu)
{
  *mcu = avr_mcu_create();
}

bool avr_mcu_read_register(const AvrMCU *mcu, uint8_t register_index,
                           uint8_t *value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  *value = mcu->registers[register_index];
  return true;
}

bool avr_mcu_write_register(AvrMCU *mcu, uint8_t register_index,
                            uint8_t value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  mcu->registers[register_index] = value;
  return true;
}

uint16_t avr_mcu_read_pc(const AvrMCU *mcu)
{
  return mcu->pc;
}

void avr_mcu_write_pc(AvrMCU *mcu, uint16_t value)
{
  mcu->pc = value;
}

uint8_t avr_mcu_read_sreg(const AvrMCU *mcu)
{
  return mcu->sreg;
}

void avr_mcu_write_sreg(AvrMCU *mcu, uint8_t value)
{
  mcu->sreg = value;
}

bool avr_mcu_read_flash(const AvrMCU *mcu, uint16_t address,
                        uint16_t *instruction)
{
  if (address >= AVR_FLASH_SIZE || mcu == NULL || instruction == NULL)
  {
    return false;
  }

  *instruction = mcu->flash[address];
  return true;
}

bool avr_mcu_write_flash(AvrMCU *mcu, uint16_t address,
                         uint16_t instruction)
{
  if (address >= AVR_FLASH_SIZE || mcu == NULL)
  {
    return false;
  }

  mcu->flash[address] = instruction;
  return true;
}
