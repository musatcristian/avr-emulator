#ifndef AVR_INSTRUCTION_H
#define AVR_INSTRUCTION_H

#include <stdbool.h>
#include <stdint.h>

#include "avr_mcu.h"

typedef enum
{
  AVR_OPERATION_LDI,
  AVR_OPERATION_MOV,
  AVR_OPERATION_ADD,
  AVR_OPERATION_SUB,
  AVR_OPERATION_INC,
  AVR_OPERATION_LD,
  AVR_OPERATION_ST,
  AVR_OPERATION_IN,
  AVR_OPERATION_OUT,
  AVR_OPERATION_RJMP,
  AVR_OPERATION_BRNE,
  AVR_OPERATION_BREQ,
  AVR_OPERATION_DEC,
  AVR_OPERATION_AND,
  AVR_OPERATION_OR,
  AVR_OPERATION_EOR,
  AVR_OPERATION_CP,
  AVR_OPERATION_CPI,
  AVR_OPERATION_SBI,
  AVR_OPERATION_CBI,
  AVR_OPERATION_PUSH,
  AVR_OPERATION_POP,
  AVR_OPERATION_CALL,
  AVR_OPERATION_RET
} AvrOperation;

typedef struct
{
  AvrOperation operation;
  uint8_t destination_register;
  uint8_t source_register;
  uint8_t immediate;
  uint8_t bit_index;
  int16_t relative_offset;
  /* Absolute flash word address for CALL; fits AVR_FLASH_SIZE in one word. */
  uint16_t target_address;
} AvrInstruction;

bool avr_encode_instruction(const AvrInstruction *instruction,
                            uint16_t *instruction_word);
bool avr_decode_instruction_word(uint16_t instruction_word,
                                 AvrInstruction *instruction);
bool avr_execute_instruction(AvrMCU *mcu, const AvrInstruction *instruction);

#endif
