#include "avr_instruction.h"

#include <stddef.h>

enum
{
  AVR_LDI_MASK = 0xf000,
  AVR_LDI_OPCODE = 0xe000,
  AVR_MOV_MASK = 0xfc00,
  AVR_MOV_OPCODE = 0x2c00,
  AVR_ADD_MASK = 0xfc00,
  AVR_ADD_OPCODE = 0x0c00,
  AVR_SUB_MASK = 0xfc00,
  AVR_SUB_OPCODE = 0x1800,
  AVR_INC_MASK = 0xfe0f,
  AVR_INC_OPCODE = 0x9403
};

static bool valid_register(uint8_t register_index);

static uint16_t encode_dual_register(uint16_t opcode, uint8_t destination,
                                     uint8_t source)
{
  return (uint16_t)(opcode | ((uint16_t)(source & UINT8_C(0x10)) << 5) |
                    ((uint16_t)destination << 4) | (source & UINT8_C(0x0f)));
}

static uint8_t decode_source_register(uint16_t instruction_word)
{
  return (uint8_t)((instruction_word & UINT16_C(0x000f)) |
                   ((instruction_word >> 5) & UINT16_C(0x0010)));
}

bool avr_encode_instruction(const AvrInstruction *instruction,
                            uint16_t *instruction_word)
{
  if (instruction == NULL || instruction_word == NULL)
  {
    return false;
  }

  if (instruction->operation == AVR_OPERATION_LDI)
  {
    if (instruction->destination_register < 16 ||
        instruction->destination_register >= AVR_REGISTER_COUNT)
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_LDI_OPCODE |
                                   ((uint16_t)(instruction->immediate &
                                               UINT8_C(0xf0))
                                    << 4) |
                                   (((uint16_t)instruction->destination_register - 16)
                                    << 4) |
                                   (instruction->immediate & UINT8_C(0x0f)));
    return true;
  }

  if (!valid_register(instruction->destination_register))
  {
    return false;
  }

  if (instruction->operation == AVR_OPERATION_MOV)
  {
    if (!valid_register(instruction->source_register))
    {
      return false;
    }

    *instruction_word = encode_dual_register(AVR_MOV_OPCODE,
                                             instruction->destination_register,
                                             instruction->source_register);
    return true;
  }

  if (instruction->operation == AVR_OPERATION_ADD)
  {
    if (!valid_register(instruction->source_register))
    {
      return false;
    }

    *instruction_word = encode_dual_register(AVR_ADD_OPCODE,
                                             instruction->destination_register,
                                             instruction->source_register);
    return true;
  }

  if (instruction->operation == AVR_OPERATION_SUB)
  {
    if (!valid_register(instruction->source_register))
    {
      return false;
    }

    *instruction_word = encode_dual_register(AVR_SUB_OPCODE,
                                             instruction->destination_register,
                                             instruction->source_register);
    return true;
  }

  if (instruction->operation == AVR_OPERATION_INC)
  {
    *instruction_word = (uint16_t)(AVR_INC_OPCODE |
                                   ((uint16_t)instruction->destination_register
                                    << 4));
    return true;
  }

  return false;
}

bool avr_decode_instruction_word(uint16_t instruction_word,
                                 AvrInstruction *instruction)
{
  if (instruction == NULL)
  {
    return false;
  }

  if ((instruction_word & AVR_LDI_MASK) == AVR_LDI_OPCODE)
  {
    instruction->operation = AVR_OPERATION_LDI;
    instruction->destination_register =
        (uint8_t)(16 + ((instruction_word >> 4) & UINT16_C(0x000f)));
    instruction->source_register = 0;
    instruction->immediate =
        (uint8_t)((instruction_word & UINT16_C(0x000f)) |
                  ((instruction_word >> 4) & UINT16_C(0x00f0)));
    return true;
  }

  if ((instruction_word & AVR_MOV_MASK) == AVR_MOV_OPCODE)
  {
    instruction->operation = AVR_OPERATION_MOV;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = decode_source_register(instruction_word);
    instruction->immediate = 0;
    return true;
  }

  if ((instruction_word & AVR_ADD_MASK) == AVR_ADD_OPCODE)
  {
    instruction->operation = AVR_OPERATION_ADD;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = decode_source_register(instruction_word);
    instruction->immediate = 0;
    return true;
  }

  if ((instruction_word & AVR_SUB_MASK) == AVR_SUB_OPCODE)
  {
    instruction->operation = AVR_OPERATION_SUB;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = decode_source_register(instruction_word);
    instruction->immediate = 0;
    return true;
  }

  if ((instruction_word & AVR_INC_MASK) == AVR_INC_OPCODE)
  {
    instruction->operation = AVR_OPERATION_INC;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = 0;
    instruction->immediate = 0;
    return true;
  }

  return false;
}

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
       (result & (uint8_t)~left)) &
      UINT8_C(0x80))
  {
    flags |= AVR_SREG_C;
  }
  if (((uint8_t)(~left & right) | (right & result) |
       (result & (uint8_t)~left)) &
      UINT8_C(0x08))
  {
    flags |= AVR_SREG_H;
  }
  if ((((left & (uint8_t)~right & (uint8_t)~result) |
        ((uint8_t)~left & right & result)) &
       UINT8_C(0x80)) != 0)
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

    avr_mcu_write_register(cpu, instruction->destination_register,
                           instruction->immediate);
  }
  else if (instruction->operation == AVR_OPERATION_MOV)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    avr_mcu_write_register(cpu, instruction->destination_register, source);
  }
  else if (instruction->operation == AVR_OPERATION_ADD)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    result = (uint8_t)(destination + source);
    flags = arithmetic_flags_add(destination, source, result);
    avr_mcu_write_register(cpu, instruction->destination_register, result);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T)) |
                                      flags));
  }
  else if (instruction->operation == AVR_OPERATION_SUB)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    result = (uint8_t)(destination - source);
    flags = arithmetic_flags_sub(destination, source, result);
    avr_mcu_write_register(cpu, instruction->destination_register, result);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T)) |
                                      flags));
  }
  else if (instruction->operation == AVR_OPERATION_INC)
  {
    if (!avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination))
    {
      return false;
    }

    result = (uint8_t)(destination + 1);
    flags = arithmetic_flags_inc(result);
    avr_mcu_write_register(cpu, instruction->destination_register, result);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T | AVR_SREG_H |
                                        AVR_SREG_C)) |
                                      flags));
  }
  else
  {
    return false;
  }

  avr_mcu_write_pc(cpu, (uint16_t)(avr_mcu_read_pc(cpu) + 1));
  return true;
}
