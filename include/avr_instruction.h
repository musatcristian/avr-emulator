#ifndef AVR_INSTRUCTION_H
#define AVR_INSTRUCTION_H

#include <stdint.h>

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

#endif
