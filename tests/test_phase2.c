#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "avr_instruction.h"

void test_instruction_encoding(void)
{
  uint16_t instruction_word = 0;

  assert(avr_encode_instruction(&(AvrInstruction){
                                  .operation = AVR_OPERATION_LDI,
                                  .destination_register = 16,
                                  .immediate = UINT8_C(0x5a)
                                },
                                &instruction_word));
  assert(instruction_word == UINT16_C(0xe50a));

  assert(avr_encode_instruction(&(AvrInstruction){
                                  .operation = AVR_OPERATION_MOV,
                                  .destination_register = 18,
                                  .source_register = 16
                                },
                                &instruction_word));
  assert(instruction_word == UINT16_C(0x2f20));

  assert(avr_encode_instruction(&(AvrInstruction){
                                  .operation = AVR_OPERATION_ADD,
                                  .destination_register = 16,
                                  .source_register = 17
                                },
                                &instruction_word));
  assert(instruction_word == UINT16_C(0x0f01));

  assert(avr_encode_instruction(&(AvrInstruction){
                                  .operation = AVR_OPERATION_SUB,
                                  .destination_register = 16,
                                  .source_register = 17
                                },
                                &instruction_word));
  assert(instruction_word == UINT16_C(0x1b01));

  assert(avr_encode_instruction(&(AvrInstruction){
                                  .operation = AVR_OPERATION_INC,
                                  .destination_register = 31
                                },
                                &instruction_word));
  assert(instruction_word == UINT16_C(0x95f3));

  assert(!avr_encode_instruction(&(AvrInstruction){
                                   .operation = AVR_OPERATION_LDI,
                                   .destination_register = 15,
                                   .immediate = UINT8_C(0xaa)
                                 },
                                 &instruction_word));
}

void test_instruction_decoding(void)
{
  AvrInstruction instruction;

  assert(avr_decode_instruction_word(UINT16_C(0xe50a), &instruction));
  assert(instruction.operation == AVR_OPERATION_LDI);
  assert(instruction.destination_register == 16);
  assert(instruction.immediate == UINT8_C(0x5a));

  assert(avr_decode_instruction_word(UINT16_C(0x2f20), &instruction));
  assert(instruction.operation == AVR_OPERATION_MOV);
  assert(instruction.destination_register == 18);
  assert(instruction.source_register == 16);

  assert(avr_decode_instruction_word(UINT16_C(0x0f01), &instruction));
  assert(instruction.operation == AVR_OPERATION_ADD);
  assert(instruction.destination_register == 16);
  assert(instruction.source_register == 17);

  assert(avr_decode_instruction_word(UINT16_C(0x1b01), &instruction));
  assert(instruction.operation == AVR_OPERATION_SUB);
  assert(instruction.destination_register == 16);
  assert(instruction.source_register == 17);

  assert(avr_decode_instruction_word(UINT16_C(0x95f3), &instruction));
  assert(instruction.operation == AVR_OPERATION_INC);
  assert(instruction.destination_register == 31);

  assert(!avr_decode_instruction_word(UINT16_C(0xffff), &instruction));
}

void test_instruction_encode_decode_roundtrip(void)
{
  const AvrInstruction original[] = {
    {
      .operation = AVR_OPERATION_LDI,
      .destination_register = 27,
      .immediate = UINT8_C(0xc3)
    },
    {
      .operation = AVR_OPERATION_MOV,
      .destination_register = 3,
      .source_register = 29
    },
    {
      .operation = AVR_OPERATION_ADD,
      .destination_register = 20,
      .source_register = 6
    },
    {
      .operation = AVR_OPERATION_SUB,
      .destination_register = 31,
      .source_register = 0
    },
    {
      .operation = AVR_OPERATION_INC,
      .destination_register = 9
    }
  };

  for (size_t index = 0; index < sizeof(original) / sizeof(original[0]);
       ++index)
  {
    uint16_t instruction_word = 0;
    AvrInstruction decoded = {0};

    assert(avr_encode_instruction(&original[index], &instruction_word));
    assert(avr_decode_instruction_word(instruction_word, &decoded));
    assert(decoded.operation == original[index].operation);
    assert(decoded.destination_register == original[index].destination_register);

    if (decoded.operation == AVR_OPERATION_LDI)
    {
      assert(decoded.immediate == original[index].immediate);
    }
    else if (decoded.operation == AVR_OPERATION_INC)
    {
      assert(decoded.source_register == 0);
      assert(decoded.immediate == 0);
    }
    else
    {
      assert(decoded.source_register == original[index].source_register);
    }
  }
}
