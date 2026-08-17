#include "avr_instruction.h"

#include <stddef.h>

static uint8_t arithmetic_flags_add(uint8_t left, uint8_t right,
                                    uint8_t result)
{
  uint8_t flags = 0;
  uint16_t sum = (uint16_t)left + right;

  if ((sum & UINT16_C(0x100)) != 0)
  {
    flags |= AVR_SREG_C;
  }
  if ((((uint16_t)(left & 0x0f) + (right & 0x0f)) & UINT16_C(0x10)) != 0)
  {
    flags |= AVR_SREG_H;
  }
  if (((uint8_t)(~(left ^ right) & (left ^ result)) & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_V;
  }
  if ((result & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_N;
  }
  if (result == 0)
  {
    flags |= AVR_SREG_Z;
  }
  if (((flags & AVR_SREG_N) != 0) != ((flags & AVR_SREG_V) != 0))
  {
    flags |= AVR_SREG_S;
  }

  return flags;
}

static uint8_t arithmetic_flags_sub(uint8_t left, uint8_t right,
                                    uint8_t result)
{
  uint8_t flags = 0;

  if (((uint8_t)(~left & right) | (right & result) |
       (result & (uint8_t)~left)) & UINT8_C(0x80))
  {
    flags |= AVR_SREG_C;
  }
  if (((uint8_t)(~left & right) | (right & result) |
       (result & (uint8_t)~left)) & UINT8_C(0x08))
  {
    flags |= AVR_SREG_H;
  }
  if ((((left & (uint8_t)~right & (uint8_t)~result) |
        ((uint8_t)~left & right & result)) & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_V;
  }
  if ((result & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_N;
  }
  if (result == 0)
  {
    flags |= AVR_SREG_Z;
  }
  if (((flags & AVR_SREG_N) != 0) != ((flags & AVR_SREG_V) != 0))
  {
    flags |= AVR_SREG_S;
  }

  return flags;
}

static uint8_t arithmetic_flags_inc(uint8_t result)
{
  uint8_t flags = 0;

  if (result == UINT8_C(0x80))
  {
    flags |= AVR_SREG_V;
  }
  if ((result & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_N;
  }
  if (result == 0)
  {
    flags |= AVR_SREG_Z;
  }
  if (((flags & AVR_SREG_N) != 0) != ((flags & AVR_SREG_V) != 0))
  {
    flags |= AVR_SREG_S;
  }

  return flags;
}

static bool valid_register(uint8_t register_index)
{
  return register_index < AVR_REGISTER_COUNT;
}

bool avr_execute_instruction(AvrMCU *cpu, const AvrInstruction *instruction)
{
  uint8_t destination;
  uint8_t source;
  uint8_t result;
  uint8_t flags;

  if (cpu == NULL || instruction == NULL ||
      !valid_register(instruction->destination_register))
  {
    return false;
  }

  if (instruction->operation == AVR_OPERATION_LDI)
  {
    if (instruction->destination_register < 16)
    {
      return false;
    }

    avr_cpu_write_register(cpu, instruction->destination_register,
                           instruction->immediate);
  }
  else if (instruction->operation == AVR_OPERATION_MOV)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_cpu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    avr_cpu_write_register(cpu, instruction->destination_register, source);
  }
  else if (instruction->operation == AVR_OPERATION_ADD)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_cpu_read_register(cpu, instruction->destination_register,
                               &destination) ||
        !avr_cpu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    result = (uint8_t)(destination + source);
    flags = arithmetic_flags_add(destination, source, result);
    avr_cpu_write_register(cpu, instruction->destination_register, result);
    avr_cpu_write_sreg(cpu, (uint8_t)((avr_cpu_read_sreg(cpu) &
                       (AVR_SREG_I | AVR_SREG_T)) | flags));
  }
  else if (instruction->operation == AVR_OPERATION_SUB)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_cpu_read_register(cpu, instruction->destination_register,
                               &destination) ||
        !avr_cpu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    result = (uint8_t)(destination - source);
    flags = arithmetic_flags_sub(destination, source, result);
    avr_cpu_write_register(cpu, instruction->destination_register, result);
    avr_cpu_write_sreg(cpu, (uint8_t)((avr_cpu_read_sreg(cpu) &
                       (AVR_SREG_I | AVR_SREG_T)) | flags));
  }
  else if (instruction->operation == AVR_OPERATION_INC)
  {
    if (!avr_cpu_read_register(cpu, instruction->destination_register,
                               &destination))
    {
      return false;
    }

    result = (uint8_t)(destination + 1);
    flags = arithmetic_flags_inc(result);
    avr_cpu_write_register(cpu, instruction->destination_register, result);
    avr_cpu_write_sreg(cpu, (uint8_t)((avr_cpu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T | AVR_SREG_H |
                                        AVR_SREG_C)) | flags));
  }
  else
  {
    return false;
  }

  avr_cpu_write_pc(cpu, (uint16_t)(avr_cpu_read_pc(cpu) + 1));
  return true;
}
