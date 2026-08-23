#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "avr_instruction.h"

void test_phase5_instruction_encoding(void)
{
    uint16_t word = 0;

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_RJMP,
                                      .relative_offset = 0},
                                  &word));
    assert(word == UINT16_C(0xc000));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_RJMP,
                                      .relative_offset = -2048},
                                  &word));
    assert(word == UINT16_C(0xc800));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_RJMP,
                                      .relative_offset = 2047},
                                  &word));
    assert(word == UINT16_C(0xc7ff));

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_BRNE,
                                      .relative_offset = -1},
                                  &word));
    assert(word == UINT16_C(0xf7f9));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_BREQ,
                                      .relative_offset = 63},
                                  &word));
    assert(word == UINT16_C(0xf1f9));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_DEC,
                                      .destination_register = 31},
                                  &word));
    assert(word == UINT16_C(0x95fa));
}

void test_phase5_instruction_decoding_and_roundtrip(void)
{
    const AvrInstruction originals[] = {
        {.operation = AVR_OPERATION_RJMP, .relative_offset = -2048},
        {.operation = AVR_OPERATION_RJMP, .relative_offset = 2047},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = -64},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = 63},
        {.operation = AVR_OPERATION_BREQ, .relative_offset = -1},
        {.operation = AVR_OPERATION_DEC, .destination_register = 12}};

    for (size_t index = 0; index < sizeof(originals) / sizeof(originals[0]);
         ++index)
    {
        uint16_t word = 0;
        AvrInstruction decoded = {0};

        assert(avr_encode_instruction(&originals[index], &word));
        assert(avr_decode_instruction_word(word, &decoded));
        assert(decoded.operation == originals[index].operation);
        assert(decoded.relative_offset == originals[index].relative_offset);
        assert(decoded.destination_register == originals[index].destination_register);
    }

    {
        AvrInstruction instruction = {0};
        assert(avr_decode_instruction_word(UINT16_C(0xcfff), &instruction));
        assert(instruction.operation == AVR_OPERATION_RJMP);
        assert(instruction.relative_offset == -1);
        assert(avr_decode_instruction_word(UINT16_C(0xf001), &instruction));
        assert(instruction.operation == AVR_OPERATION_BREQ);
        assert(instruction.relative_offset == 0);
        assert(!avr_decode_instruction_word(UINT16_C(0xf000), &instruction));
    }
}

void test_dec_flags_and_preservation(void)
{
    AvrMCU mcu = avr_mcu_create();
    const uint8_t preserved = AVR_SREG_I | AVR_SREG_T | AVR_SREG_H | AVR_SREG_C;

    mcu.registers[16] = 0;
    mcu.sreg = preserved;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_DEC,
                                             .destination_register = 16}));
    assert(mcu.registers[16] == UINT8_C(0xff));
    assert(mcu.sreg == (preserved | AVR_SREG_N | AVR_SREG_S));

    mcu.registers[16] = UINT8_C(0x80);
    mcu.sreg = preserved;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_DEC,
                                             .destination_register = 16}));
    assert(mcu.registers[16] == UINT8_C(0x7f));
    assert(mcu.sreg == (preserved | AVR_SREG_V | AVR_SREG_S));

    mcu.registers[16] = 1;
    mcu.sreg = preserved;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_DEC,
                                             .destination_register = 16}));
    assert(mcu.registers[16] == 0);
    assert(mcu.sreg == (preserved | AVR_SREG_Z));
}

void test_relative_control_flow_execution(void)
{
    AvrMCU mcu = avr_mcu_create();

    mcu.pc = 10;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_RJMP,
                                             .relative_offset = -3}));
    assert(mcu.pc == 8);

    mcu.pc = 10;
    mcu.sreg = 0;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_BRNE,
                                             .relative_offset = 4}));
    assert(mcu.pc == 15);

    mcu.pc = 10;
    mcu.sreg = AVR_SREG_Z;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_BRNE,
                                             .relative_offset = 4}));
    assert(mcu.pc == 11);

    mcu.pc = 10;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_BREQ,
                                             .relative_offset = -4}));
    assert(mcu.pc == 7);
}

void test_phase5_machine_code_loop(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 3},
        {.operation = AVR_OPERATION_DEC, .destination_register = 16},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = -2}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];

    for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
    {
        assert(avr_encode_instruction(&program[index], &machine_code[index]));
    }
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    for (size_t step = 0; step < 7; ++step)
    {
        assert(avr_mcu_step(&mcu));
    }
    assert(mcu.registers[16] == 0);
    assert(mcu.pc == 3);
}

void test_phase5_failure_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint16_t word = UINT16_C(0xaaaa);

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_RJMP,
                                       .relative_offset = 2048},
                                   &word));
    assert(word == UINT16_C(0xaaaa));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_BRNE,
                                       .relative_offset = -65},
                                   &word));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_BREQ,
                                       .relative_offset = 64},
                                   &word));

    mcu.pc = 12;
    mcu.registers[16] = UINT8_C(0x55);
    mcu.sreg = AVR_SREG_C;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_RJMP,
                                              .relative_offset = 2048}));
    assert(mcu.pc == 12);
    assert(mcu.registers[16] == UINT8_C(0x55));
    assert(mcu.sreg == AVR_SREG_C);
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_BRNE,
                                              .relative_offset = -65}));
    assert(mcu.pc == 12);
}
