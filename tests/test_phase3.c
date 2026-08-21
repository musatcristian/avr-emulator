#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_instruction.h"

void test_sram_api(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = UINT8_C(0xff);

    assert(avr_mcu_read_data(&mcu, 0, &value));
    assert(value == 0);
    assert(avr_mcu_read_data(&mcu, AVR_SRAM_SIZE - 1, &value));
    assert(value == 0);

    assert(avr_mcu_write_data(&mcu, 0, UINT8_C(0x55)));
    assert(avr_mcu_read_data(&mcu, 0, &value));
    assert(value == UINT8_C(0x55));

    assert(avr_mcu_write_data(&mcu, AVR_SRAM_SIZE - 1, UINT8_C(0xff)));
    assert(avr_mcu_read_data(&mcu, AVR_SRAM_SIZE - 1, &value));
    assert(value == UINT8_C(0xff));

    assert(avr_mcu_write_data(&mcu, 1, UINT8_C(0x01)));
    assert(avr_mcu_read_data(&mcu, 1, &value));
    assert(value == UINT8_C(0x01));

    assert(!avr_mcu_write_data(&mcu, AVR_SRAM_SIZE, UINT8_C(0xaa)));
    assert(!avr_mcu_write_data(&mcu, UINT16_C(0xffff), UINT8_C(0xbb)));
    assert(avr_mcu_read_data(&mcu, 1, &value));
    assert(value == UINT8_C(0x01));

    value = UINT8_C(0x55);
    assert(!avr_mcu_read_data(&mcu, AVR_SRAM_SIZE, &value));
    assert(value == UINT8_C(0x55));
    assert(!avr_mcu_read_data(&mcu, UINT16_C(0xffff), &value));
    assert(value == UINT8_C(0x55));

    assert(!avr_mcu_read_data(NULL, 0, &value));
    assert(!avr_mcu_read_data(&mcu, 0, NULL));
    assert(!avr_mcu_write_data(NULL, 0, UINT8_C(0x00)));

    avr_mcu_reset(&mcu);
    assert(avr_mcu_read_data(&mcu, 0, &value));
    assert(value == 0);
    assert(avr_mcu_read_data(&mcu, AVR_SRAM_SIZE - 1, &value));
    assert(value == 0);
}

void test_ld_st_instruction_encoding(void)
{
    uint16_t instruction_word = 0;

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_LD,
                                      .destination_register = 0},
                                  &instruction_word));
    assert(instruction_word == UINT16_C(0x900c));

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_LD,
                                      .destination_register = 31},
                                  &instruction_word));
    assert(instruction_word == UINT16_C(0x91fc));

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_ST,
                                      .destination_register = 0,
                                      .source_register = 0},
                                  &instruction_word));
    assert(instruction_word == UINT16_C(0x920c));

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_ST,
                                      .destination_register = 0,
                                      .source_register = 31},
                                  &instruction_word));
    assert(instruction_word == UINT16_C(0x93fc));

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_LD,
                                       .destination_register = AVR_REGISTER_COUNT},
                                   &instruction_word));

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_ST,
                                       .destination_register = 0,
                                       .source_register = AVR_REGISTER_COUNT},
                                   &instruction_word));
}

void test_ld_st_instruction_decoding_and_roundtrip(void)
{
    AvrInstruction instruction = {0};
    const AvrInstruction originals[] = {
        {.operation = AVR_OPERATION_LD,
         .destination_register = 5},
        {.operation = AVR_OPERATION_LD,
         .destination_register = 30},
        {.operation = AVR_OPERATION_ST,
         .destination_register = 0,
         .source_register = 12},
        {.operation = AVR_OPERATION_ST,
         .destination_register = 0,
         .source_register = 31}};

    assert(avr_decode_instruction_word(UINT16_C(0x900c), &instruction));
    assert(instruction.operation == AVR_OPERATION_LD);
    assert(instruction.destination_register == 0);
    assert(instruction.source_register == 0);

    assert(avr_decode_instruction_word(UINT16_C(0x93fc), &instruction));
    assert(instruction.operation == AVR_OPERATION_ST);
    assert(instruction.source_register == 31);

    assert(!avr_decode_instruction_word(UINT16_C(0x900d), &instruction));

    for (size_t index = 0; index < sizeof(originals) / sizeof(originals[0]);
         ++index)
    {
        uint16_t instruction_word = 0;
        AvrInstruction decoded = {0};

        assert(avr_encode_instruction(&originals[index], &instruction_word));
        assert(avr_decode_instruction_word(instruction_word, &decoded));
        assert(decoded.operation == originals[index].operation);

        if (decoded.operation == AVR_OPERATION_LD)
        {
            assert(decoded.destination_register == originals[index].destination_register);
            assert(decoded.source_register == 0);
        }
        else
        {
            assert(decoded.source_register == originals[index].source_register);
            assert(decoded.destination_register == 0);
        }
    }
}

void test_ld_st_direct_execution(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = 0;

    mcu.sreg = AVR_SREG_I | AVR_SREG_T | AVR_SREG_C;
    mcu.registers[26] = UINT8_C(0x04);
    mcu.registers[27] = UINT8_C(0x00);
    mcu.registers[16] = UINT8_C(0x55);

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_ST,
                                             .destination_register = 0,
                                             .source_register = 16}));
    assert(avr_mcu_read_data(&mcu, 4, &value));
    assert(value == UINT8_C(0x55));
    assert(mcu.registers[16] == UINT8_C(0x55));
    assert(mcu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C));
    assert(mcu.pc == 1);

    mcu.registers[26] = UINT8_C(0xff);
    mcu.registers[27] = UINT8_C(0x00);
    mcu.registers[17] = 0;

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_ST,
                                             .destination_register = 0,
                                             .source_register = 16}));
    assert(avr_mcu_read_data(&mcu, AVR_SRAM_SIZE - 1, &value));
    assert(value == UINT8_C(0x55));
    assert(mcu.pc == 2);

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_LD,
                                             .destination_register = 17}));
    assert(mcu.registers[17] == UINT8_C(0x55));
    assert(mcu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C));
    assert(mcu.pc == 3);
}

void test_ld_st_failure_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = 0;

    assert(avr_mcu_write_data(&mcu, 0, UINT8_C(0x01)));

    mcu.registers[26] = UINT8_C(0x00);
    mcu.registers[27] = UINT8_C(0x01);
    mcu.registers[18] = UINT8_C(0xaa);
    mcu.registers[19] = UINT8_C(0x55);
    mcu.sreg = AVR_SREG_H | AVR_SREG_C;
    mcu.pc = 9;

    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_LD,
                                              .destination_register = 18}));
    assert(mcu.registers[18] == UINT8_C(0xaa));
    assert(mcu.sreg == (AVR_SREG_H | AVR_SREG_C));
    assert(mcu.pc == 9);

    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_ST,
                                              .destination_register = 0,
                                              .source_register = 19}));
    assert(avr_mcu_read_data(&mcu, 0, &value));
    assert(value == UINT8_C(0x01));
    assert(mcu.registers[19] == UINT8_C(0x55));
    assert(mcu.sreg == (AVR_SREG_H | AVR_SREG_C));
    assert(mcu.pc == 9);
}

void test_phase3_machine_code_integration(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = 0;
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 26,
         .immediate = UINT8_C(0x22)},
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 27,
         .immediate = UINT8_C(0x00)},
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = UINT8_C(0x55)},
        {.operation = AVR_OPERATION_ST,
         .destination_register = 0,
         .source_register = 16},
        {.operation = AVR_OPERATION_LD,
         .destination_register = 17},
        {.operation = AVR_OPERATION_INC,
         .destination_register = 17}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];

    avr_mcu_reset(&mcu);
    for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
    {
        assert(avr_encode_instruction(&program[index], &machine_code[index]));
    }
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    assert(avr_mcu_step(&mcu));
    assert(mcu.registers[26] == UINT8_C(0x22));
    assert(mcu.pc == 1);

    assert(avr_mcu_step(&mcu));
    assert(mcu.registers[27] == UINT8_C(0x00));
    assert(mcu.pc == 2);

    assert(avr_mcu_step(&mcu));
    assert(mcu.registers[16] == UINT8_C(0x55));
    assert(mcu.pc == 3);

    assert(avr_mcu_step(&mcu));
    assert(avr_mcu_read_data(&mcu, UINT16_C(0x22), &value));
    assert(value == UINT8_C(0x55));
    assert(mcu.pc == 4);

    assert(avr_mcu_step(&mcu));
    assert(mcu.registers[17] == UINT8_C(0x55));
    assert(mcu.pc == 5);

    assert(avr_mcu_step(&mcu));
    assert(mcu.registers[17] == UINT8_C(0x56));
    assert(mcu.sreg == 0);
    assert(mcu.pc == 6);
}
