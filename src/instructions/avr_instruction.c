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
  AVR_INC_OPCODE = 0x9403,
  AVR_LD_MASK = 0xfe0f,
  AVR_LD_OPCODE = 0x900c,
  AVR_ST_MASK = 0xfe0f,
  AVR_ST_OPCODE = 0x920c,
  AVR_IN_MASK = 0xf800,
  AVR_IN_OPCODE = 0xb000,
  AVR_OUT_MASK = 0xf800,
  AVR_OUT_OPCODE = 0xb800,
  AVR_RJMP_MASK = 0xf000,
  AVR_RJMP_OPCODE = 0xc000,
  AVR_BRNE_MASK = 0xfc07,
  AVR_BRNE_OPCODE = 0xf401,
  AVR_BREQ_MASK = 0xfc07,
  AVR_BREQ_OPCODE = 0xf001,
  AVR_DEC_MASK = 0xfe0f,
  AVR_DEC_OPCODE = 0x940a,
  AVR_AND_MASK = 0xfc00,
  AVR_AND_OPCODE = 0x2000,
  AVR_OR_MASK = 0xfc00,
  AVR_OR_OPCODE = 0x2800,
  AVR_EOR_MASK = 0xfc00,
  AVR_EOR_OPCODE = 0x2400,
  AVR_CP_MASK = 0xfc00,
  AVR_CP_OPCODE = 0x1400,
  AVR_CPI_MASK = 0xf000,
  AVR_CPI_OPCODE = 0x3000,
  AVR_SBI_MASK = 0xff00,
  AVR_SBI_OPCODE = 0x9a00,
  AVR_CBI_MASK = 0xff00,
  AVR_CBI_OPCODE = 0x9800
};

static bool valid_register(uint8_t register_index);
static bool read_x_pointer(const AvrMCU *mcu, uint16_t *address);
static bool valid_relative_offset(int16_t offset, int16_t minimum,
                                  int16_t maximum);
static uint16_t encode_relative_offset(int16_t offset, uint8_t bit_count);
static int16_t decode_relative_offset(uint16_t instruction_word,
                                      uint8_t bit_count);

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

static uint8_t decode_bit_index(uint16_t instruction_word)
{
  return (uint8_t)(instruction_word & UINT16_C(0x0007));
}

static uint8_t decode_bit_io_address(uint16_t instruction_word)
{
  return (uint8_t)((instruction_word >> 3) & UINT16_C(0x001f));
}

static uint8_t decode_st_source_register(uint16_t instruction_word)
{
  return (uint8_t)(((instruction_word >> 4) & UINT16_C(0x000f)) |
                   ((instruction_word >> 4) & UINT16_C(0x0010)));
}

static uint8_t decode_io_address(uint16_t instruction_word)
{
  return (uint8_t)(((instruction_word >> 5) & UINT16_C(0x0030)) |
                   (instruction_word & UINT16_C(0x000f)));
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

  if (instruction->operation == AVR_OPERATION_LD)
  {
    *instruction_word = (uint16_t)(AVR_LD_OPCODE |
                                   ((uint16_t)instruction->destination_register
                                    << 4));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_ST)
  {
    if (!valid_register(instruction->source_register))
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_ST_OPCODE |
                                   ((uint16_t)(instruction->source_register &
                                               UINT8_C(0x10))
                                    << 4) |
                                   ((uint16_t)(instruction->source_register &
                                               UINT8_C(0x0f))
                                    << 4));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_IN)
  {
    if (instruction->immediate > UINT8_C(0x3f))
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_IN_OPCODE |
                                   ((uint16_t)(instruction->immediate &
                                               UINT8_C(0x30))
                                    << 5) |
                                   ((uint16_t)instruction->destination_register << 4) |
                                   (instruction->immediate & UINT8_C(0x0f)));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_OUT)
  {
    if (!valid_register(instruction->source_register) ||
        instruction->immediate > UINT8_C(0x3f))
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_OUT_OPCODE |
                                   ((uint16_t)(instruction->immediate &
                                               UINT8_C(0x30))
                                    << 5) |
                                   ((uint16_t)instruction->source_register << 4) |
                                   (instruction->immediate & UINT8_C(0x0f)));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_RJMP)
  {
    if (!valid_relative_offset(instruction->relative_offset, -2048, 2047))
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_RJMP_OPCODE |
                                   encode_relative_offset(instruction->relative_offset,
                                                          12));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_BRNE ||
      instruction->operation == AVR_OPERATION_BREQ)
  {
    if (!valid_relative_offset(instruction->relative_offset, -64, 63))
    {
      return false;
    }

    *instruction_word = (uint16_t)((instruction->operation == AVR_OPERATION_BRNE
                                        ? AVR_BRNE_OPCODE
                                        : AVR_BREQ_OPCODE) |
                                   (encode_relative_offset(instruction->relative_offset,
                                                           7)
                                    << 3));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_DEC)
  {
    *instruction_word = (uint16_t)(AVR_DEC_OPCODE |
                                   ((uint16_t)instruction->destination_register << 4));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_AND ||
      instruction->operation == AVR_OPERATION_OR ||
      instruction->operation == AVR_OPERATION_EOR ||
      instruction->operation == AVR_OPERATION_CP)
  {
    if (!valid_register(instruction->source_register))
    {
      return false;
    }

    uint16_t opcode = AVR_AND_OPCODE;
    if (instruction->operation == AVR_OPERATION_OR)
    {
      opcode = AVR_OR_OPCODE;
    }
    else if (instruction->operation == AVR_OPERATION_EOR)
    {
      opcode = AVR_EOR_OPCODE;
    }
    else if (instruction->operation == AVR_OPERATION_CP)
    {
      opcode = AVR_CP_OPCODE;
    }

    *instruction_word = encode_dual_register(opcode,
                                             instruction->destination_register,
                                             instruction->source_register);
    return true;
  }

  if (instruction->operation == AVR_OPERATION_CPI)
  {
    if (instruction->destination_register < 16 ||
        instruction->destination_register >= AVR_REGISTER_COUNT)
    {
      return false;
    }

    *instruction_word = (uint16_t)(AVR_CPI_OPCODE |
                                   ((uint16_t)(instruction->immediate & 0xf0) << 4) |
                                   ((uint16_t)(instruction->destination_register - 16)
                                    << 4) |
                                   (instruction->immediate & 0x0f));
    return true;
  }

  if (instruction->operation == AVR_OPERATION_SBI ||
      instruction->operation == AVR_OPERATION_CBI)
  {
    if (instruction->immediate > UINT8_C(0x1f) ||
        instruction->bit_index > UINT8_C(0x07))
    {
      return false;
    }

    *instruction_word = (uint16_t)((instruction->operation == AVR_OPERATION_SBI
                                        ? AVR_SBI_OPCODE
                                        : AVR_CBI_OPCODE) |
                                   ((uint16_t)instruction->immediate << 3) |
                                   instruction->bit_index);
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

  if ((instruction_word & AVR_LD_MASK) == AVR_LD_OPCODE)
  {
    instruction->operation = AVR_OPERATION_LD;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = 0;
    instruction->immediate = 0;
    return true;
  }

  if ((instruction_word & AVR_ST_MASK) == AVR_ST_OPCODE)
  {
    instruction->operation = AVR_OPERATION_ST;
    instruction->destination_register = 0;
    instruction->source_register = decode_st_source_register(instruction_word);
    instruction->immediate = 0;
    return true;
  }

  if ((instruction_word & AVR_IN_MASK) == AVR_IN_OPCODE)
  {
    instruction->operation = AVR_OPERATION_IN;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = 0;
    instruction->immediate = decode_io_address(instruction_word);
    return true;
  }

  if ((instruction_word & AVR_OUT_MASK) == AVR_OUT_OPCODE)
  {
    instruction->operation = AVR_OPERATION_OUT;
    instruction->destination_register = 0;
    instruction->source_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->immediate = decode_io_address(instruction_word);
    return true;
  }

  if ((instruction_word & AVR_RJMP_MASK) == AVR_RJMP_OPCODE)
  {
    instruction->operation = AVR_OPERATION_RJMP;
    instruction->destination_register = 0;
    instruction->source_register = 0;
    instruction->immediate = 0;
    instruction->relative_offset = decode_relative_offset(instruction_word, 12);
    return true;
  }

  if ((instruction_word & AVR_BRNE_MASK) == AVR_BRNE_OPCODE ||
      (instruction_word & AVR_BREQ_MASK) == AVR_BREQ_OPCODE)
  {
    instruction->operation = (instruction_word & AVR_BRNE_MASK) == AVR_BRNE_OPCODE
                                 ? AVR_OPERATION_BRNE
                                 : AVR_OPERATION_BREQ;
    instruction->destination_register = 0;
    instruction->source_register = 0;
    instruction->immediate = 0;
    instruction->relative_offset = decode_relative_offset((uint16_t)(instruction_word >> 3),
                                                          7);
    return true;
  }

  if ((instruction_word & AVR_DEC_MASK) == AVR_DEC_OPCODE)
  {
    instruction->operation = AVR_OPERATION_DEC;
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = 0;
    instruction->immediate = 0;
    instruction->relative_offset = 0;
    return true;
  }

  if ((instruction_word & AVR_AND_MASK) == AVR_AND_OPCODE ||
      (instruction_word & AVR_OR_MASK) == AVR_OR_OPCODE ||
      (instruction_word & AVR_EOR_MASK) == AVR_EOR_OPCODE ||
      (instruction_word & AVR_CP_MASK) == AVR_CP_OPCODE)
  {
    if ((instruction_word & AVR_AND_MASK) == AVR_AND_OPCODE)
    {
      instruction->operation = AVR_OPERATION_AND;
    }
    else if ((instruction_word & AVR_OR_MASK) == AVR_OR_OPCODE)
    {
      instruction->operation = AVR_OPERATION_OR;
    }
    else if ((instruction_word & AVR_EOR_MASK) == AVR_EOR_OPCODE)
    {
      instruction->operation = AVR_OPERATION_EOR;
    }
    else
    {
      instruction->operation = AVR_OPERATION_CP;
    }
    instruction->destination_register =
        (uint8_t)((instruction_word >> 4) & UINT16_C(0x001f));
    instruction->source_register = decode_source_register(instruction_word);
    instruction->immediate = 0;
    instruction->relative_offset = 0;
    return true;
  }

  if ((instruction_word & AVR_CPI_MASK) == AVR_CPI_OPCODE)
  {
    instruction->operation = AVR_OPERATION_CPI;
    instruction->destination_register =
        (uint8_t)(16 + ((instruction_word >> 4) & UINT16_C(0x000f)));
    instruction->source_register = 0;
    instruction->immediate = (uint8_t)((instruction_word & UINT16_C(0x000f)) |
                                       ((instruction_word >> 4) & UINT16_C(0x00f0)));
    instruction->relative_offset = 0;
    return true;
  }

  if ((instruction_word & AVR_SBI_MASK) == AVR_SBI_OPCODE ||
      (instruction_word & AVR_CBI_MASK) == AVR_CBI_OPCODE)
  {
    instruction->operation = (instruction_word & AVR_SBI_MASK) == AVR_SBI_OPCODE
                                 ? AVR_OPERATION_SBI
                                 : AVR_OPERATION_CBI;
    instruction->destination_register = 0;
    instruction->source_register = 0;
    instruction->immediate = decode_bit_io_address(instruction_word);
    instruction->bit_index = decode_bit_index(instruction_word);
    instruction->relative_offset = 0;
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

static uint8_t arithmetic_flags_dec(uint8_t result)
{
  uint8_t flags = 0;

  if (result == UINT8_C(0x7f))
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

static uint8_t arithmetic_flags_logical(uint8_t result)
{
  uint8_t flags = 0;

  if ((result & UINT8_C(0x80)) != 0)
  {
    flags |= AVR_SREG_N | AVR_SREG_S;
  }
  if (result == 0)
  {
    flags |= AVR_SREG_Z;
  }

  return flags;
}

static bool valid_register(uint8_t register_index)
{
  return register_index < AVR_REGISTER_COUNT;
}

static bool valid_relative_offset(int16_t offset, int16_t minimum,
                                  int16_t maximum)
{
  return offset >= minimum && offset <= maximum;
}

static uint16_t encode_relative_offset(int16_t offset, uint8_t bit_count)
{
  return (uint16_t)offset & (uint16_t)((UINT16_C(1) << bit_count) - 1);
}

static int16_t decode_relative_offset(uint16_t instruction_word,
                                      uint8_t bit_count)
{
  uint16_t encoded = instruction_word & (uint16_t)((UINT16_C(1) << bit_count) - 1);
  uint16_t sign_bit = UINT16_C(1) << (bit_count - 1);

  if ((encoded & sign_bit) != 0)
  {
    encoded |= (uint16_t)~((UINT16_C(1) << bit_count) - 1);
  }

  return (int16_t)encoded;
}

static bool read_x_pointer(const AvrMCU *mcu, uint16_t *address)
{
  uint8_t x_low;
  uint8_t x_high;

  if (mcu == NULL || address == NULL ||
      !avr_mcu_read_register(mcu, 26, &x_low) ||
      !avr_mcu_read_register(mcu, 27, &x_high))
  {
    return false;
  }

  *address = (uint16_t)(((uint16_t)x_high << 8) | x_low);
  return true;
}

bool avr_execute_instruction(AvrMCU *cpu, const AvrInstruction *instruction)
{
  uint8_t destination;
  uint8_t source;
  uint8_t result;
  uint8_t flags;
  uint16_t x_address;
  uint16_t next_pc;

  if (cpu == NULL || instruction == NULL ||
      !valid_register(instruction->destination_register))
  {
    return false;
  }

  next_pc = (uint16_t)(avr_mcu_read_pc(cpu) + 1);

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
  else if (instruction->operation == AVR_OPERATION_LD)
  {
    if (!read_x_pointer(cpu, &x_address) ||
        !avr_mcu_read_data(cpu, x_address, &source))
    {
      return false;
    }

    avr_mcu_write_register(cpu, instruction->destination_register, source);
  }
  else if (instruction->operation == AVR_OPERATION_ST)
  {
    if (!valid_register(instruction->source_register) ||
        !read_x_pointer(cpu, &x_address) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source) ||
        !avr_mcu_write_data(cpu, x_address, source))
    {
      return false;
    }
  }
  else if (instruction->operation == AVR_OPERATION_IN)
  {
    if (instruction->immediate > UINT8_C(0x3f) ||
        !avr_mcu_read_io(cpu, instruction->immediate, &source))
    {
      return false;
    }

    avr_mcu_write_register(cpu, instruction->destination_register, source);
  }
  else if (instruction->operation == AVR_OPERATION_OUT)
  {
    if (!valid_register(instruction->source_register) ||
        instruction->immediate > UINT8_C(0x3f) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source) ||
        !avr_mcu_write_io(cpu, instruction->immediate, source))
    {
      return false;
    }
  }
  else if (instruction->operation == AVR_OPERATION_RJMP)
  {
    if (!valid_relative_offset(instruction->relative_offset, -2048, 2047))
    {
      return false;
    }

    next_pc = (uint16_t)(avr_mcu_read_pc(cpu) + 1 + instruction->relative_offset);
  }
  else if (instruction->operation == AVR_OPERATION_BRNE ||
           instruction->operation == AVR_OPERATION_BREQ)
  {
    if (!valid_relative_offset(instruction->relative_offset, -64, 63))
    {
      return false;
    }

    if ((instruction->operation == AVR_OPERATION_BRNE &&
         (avr_mcu_read_sreg(cpu) & AVR_SREG_Z) == 0) ||
        (instruction->operation == AVR_OPERATION_BREQ &&
         (avr_mcu_read_sreg(cpu) & AVR_SREG_Z) != 0))
    {
      next_pc = (uint16_t)(avr_mcu_read_pc(cpu) + 1 +
                           instruction->relative_offset);
    }
  }
  else if (instruction->operation == AVR_OPERATION_DEC)
  {
    if (!avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination))
    {
      return false;
    }

    result = (uint8_t)(destination - 1);
    flags = arithmetic_flags_dec(result);
    avr_mcu_write_register(cpu, instruction->destination_register, result);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T | AVR_SREG_H |
                                        AVR_SREG_C)) |
                                      flags));
  }
  else if (instruction->operation == AVR_OPERATION_AND ||
           instruction->operation == AVR_OPERATION_OR ||
           instruction->operation == AVR_OPERATION_EOR)
  {
    if (!valid_register(instruction->source_register) ||
        !avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination) ||
        !avr_mcu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    if (instruction->operation == AVR_OPERATION_AND)
    {
      result = (uint8_t)(destination & source);
    }
    else if (instruction->operation == AVR_OPERATION_OR)
    {
      result = (uint8_t)(destination | source);
    }
    else
    {
      result = (uint8_t)(destination ^ source);
    }

    avr_mcu_write_register(cpu, instruction->destination_register, result);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T | AVR_SREG_H |
                                        AVR_SREG_C)) |
                                      arithmetic_flags_logical(result)));
  }
  else if (instruction->operation == AVR_OPERATION_CP ||
           instruction->operation == AVR_OPERATION_CPI)
  {
    if (instruction->operation == AVR_OPERATION_CPI &&
        (instruction->destination_register < 16 ||
         instruction->destination_register >= AVR_REGISTER_COUNT))
    {
      return false;
    }
    if (instruction->operation == AVR_OPERATION_CP &&
        !valid_register(instruction->source_register))
    {
      return false;
    }
    if (!avr_mcu_read_register(cpu, instruction->destination_register,
                               &destination))
    {
      return false;
    }

    source = instruction->operation == AVR_OPERATION_CPI
                 ? instruction->immediate
                 : instruction->source_register;
    if (instruction->operation == AVR_OPERATION_CP &&
        !avr_mcu_read_register(cpu, instruction->source_register, &source))
    {
      return false;
    }

    result = (uint8_t)(destination - source);
    avr_mcu_write_sreg(cpu, (uint8_t)((avr_mcu_read_sreg(cpu) &
                                       (AVR_SREG_I | AVR_SREG_T)) |
                                      arithmetic_flags_sub(destination, source,
                                                           result)));
  }
  else if (instruction->operation == AVR_OPERATION_SBI ||
           instruction->operation == AVR_OPERATION_CBI)
  {
    if (instruction->immediate > UINT8_C(0x1f) ||
        instruction->bit_index > UINT8_C(0x07) ||
        !avr_mcu_read_io(cpu, instruction->immediate, &source))
    {
      return false;
    }

    if (instruction->operation == AVR_OPERATION_SBI)
    {
      source |= (uint8_t)(UINT8_C(1) << instruction->bit_index);
    }
    else
    {
      source &= (uint8_t)~(UINT8_C(1) << instruction->bit_index);
    }

    if (!avr_mcu_write_io(cpu, instruction->immediate, source))
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  avr_mcu_write_pc(cpu, next_pc);
  return true;
}
