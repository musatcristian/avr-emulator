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
  AVR_OPERATION_INC
} AvrOperation;

typedef struct
{
  AvrOperation operation;
  uint8_t destination_register;
  uint8_t source_register;
  uint8_t immediate;
} AvrInstruction;

bool avr_encode_instruction(const AvrInstruction *instruction,
                            uint16_t *instruction_word);
bool avr_decode_instruction_word(uint16_t instruction_word,
                                 AvrInstruction *instruction);

bool avr_execute_instruction(AvrMCU *mcu, const AvrInstruction *instruction);

#endif
