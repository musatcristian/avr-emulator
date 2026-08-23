#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_instruction.h"

static void execute(AvrMCU *mcu, AvrOperation operation, uint8_t destination,
                    uint8_t source)
{
    assert(avr_execute_instruction(mcu, &(AvrInstruction){
                                            .operation = operation,
                                            .destination_register = destination,
                                            .source_register = source}));
}

void test_phase6_instruction_encoding(void)
{
    uint16_t word = 0;

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_AND,
                                      .destination_register = 18,
                                      .source_register = 16},
                                  &word));
    assert(word == UINT16_C(0x2320));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_OR,
                                      .destination_register = 16,
                                      .source_register = 17},
                                  &word));
    assert(word == UINT16_C(0x2b01));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_EOR,
                                      .destination_register = 31,
                                      .source_register = 0},
                                  &word));
    assert(word == UINT16_C(0x25f0));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_CP,
                                      .destination_register = 20,
                                      .source_register = 7},
                                  &word));
    assert(word == UINT16_C(0x1547));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_CPI,
                                      .destination_register = 18,
                                      .immediate = UINT8_C(0x5a)},
                                  &word));
    assert(word == UINT16_C(0x352a));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_SBI,
                                      .immediate = AVR_IO_PORTB,
                                      .bit_index = 5},
                                  &word));
    assert(word == UINT16_C(0x9a15));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_CBI,
                                      .immediate = AVR_IO_DDRB,
                                      .bit_index = 0},
                                  &word));
    assert(word == UINT16_C(0x9808));
}

void test_phase6_instruction_decoding_and_roundtrip(void)
{
    const AvrInstruction originals[] = {
        {.operation = AVR_OPERATION_AND, .destination_register = 3, .source_register = 29},
        {.operation = AVR_OPERATION_OR, .destination_register = 31, .source_register = 0},
        {.operation = AVR_OPERATION_EOR, .destination_register = 16, .source_register = 17},
        {.operation = AVR_OPERATION_CP, .destination_register = 20, .source_register = 7},
        {.operation = AVR_OPERATION_CPI, .destination_register = 31, .immediate = 0xa5},
        {.operation = AVR_OPERATION_SBI, .immediate = 17, .bit_index = 7},
        {.operation = AVR_OPERATION_CBI, .immediate = 3, .bit_index = 2}};

    for (size_t index = 0; index < sizeof(originals) / sizeof(originals[0]); ++index)
    {
        uint16_t word = 0;
        AvrInstruction decoded = {0};

        assert(avr_encode_instruction(&originals[index], &word));
        assert(avr_decode_instruction_word(word, &decoded));
        assert(decoded.operation == originals[index].operation);
        assert(decoded.destination_register == originals[index].destination_register);
        assert(decoded.source_register == originals[index].source_register);
        assert(decoded.immediate == originals[index].immediate);
        assert(decoded.bit_index == originals[index].bit_index);
    }

    assert(!avr_decode_instruction_word(UINT16_C(0x9b00), &(AvrInstruction){0}));
}

void test_logical_instruction_flags(void)
{
    AvrMCU mcu = avr_mcu_create();
    const uint8_t preserved = AVR_SREG_I | AVR_SREG_T | AVR_SREG_H |
                              AVR_SREG_C;

    mcu.registers[16] = UINT8_C(0xf0);
    mcu.registers[17] = UINT8_C(0x0f);
    mcu.sreg = preserved | AVR_SREG_V;
    execute(&mcu, AVR_OPERATION_AND, 16, 17);
    assert(mcu.registers[16] == 0);
    assert(mcu.sreg == (preserved | AVR_SREG_Z));

    mcu.registers[16] = UINT8_C(0x80);
    mcu.registers[17] = UINT8_C(0xff);
    mcu.sreg = preserved | AVR_SREG_V;
    execute(&mcu, AVR_OPERATION_OR, 16, 17);
    assert(mcu.registers[16] == UINT8_C(0xff));
    assert(mcu.sreg == (preserved | AVR_SREG_N | AVR_SREG_S));

    mcu.registers[16] = UINT8_C(0xaa);
    mcu.registers[17] = UINT8_C(0xff);
    mcu.sreg = preserved | AVR_SREG_V;
    execute(&mcu, AVR_OPERATION_EOR, 16, 17);
    assert(mcu.registers[16] == UINT8_C(0x55));
    assert(mcu.sreg == preserved);
}

void test_compare_instruction_flags_and_preservation(void)
{
    AvrMCU mcu = avr_mcu_create();
    const uint8_t preserved = AVR_SREG_I | AVR_SREG_T;

    mcu.registers[16] = 5;
    mcu.registers[17] = 5;
    mcu.sreg = preserved;
    execute(&mcu, AVR_OPERATION_CP, 16, 17);
    assert(mcu.registers[16] == 5);
    assert(mcu.sreg == (preserved | AVR_SREG_Z));

    mcu.registers[16] = 0;
    mcu.sreg = preserved;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_CPI,
                                             .destination_register = 16,
                                             .immediate = 1}));
    assert(mcu.registers[16] == 0);
    assert(mcu.sreg == (preserved | AVR_SREG_C | AVR_SREG_H | AVR_SREG_N |
                        AVR_SREG_S));
}

void test_sbi_cbi_gpio_execution(void)
{
    AvrMCU mcu = avr_mcu_create();
    mcu.sreg = AVR_SREG_I | AVR_SREG_T | AVR_SREG_C;

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_SBI,
                                             .immediate = AVR_IO_DDRB,
                                             .bit_index = 5}));
    assert((mcu.ddrb & UINT8_C(0x20)) != 0);
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_SBI,
                                             .immediate = AVR_IO_PORTB,
                                             .bit_index = 5}));
    assert(mcu.portb == UINT8_C(0x20));
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_CBI,
                                             .immediate = AVR_IO_PORTB,
                                             .bit_index = 5}));
    assert(mcu.portb == 0);
    assert(mcu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C));
}

void test_phase6_machine_code_integration(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_IN, .destination_register = 16, .immediate = AVR_IO_PINB},
        {.operation = AVR_OPERATION_AND, .destination_register = 16, .source_register = 17},
        {.operation = AVR_OPERATION_CPI, .destination_register = 16, .immediate = 0x20},
        {.operation = AVR_OPERATION_BREQ, .relative_offset = 1},
        {.operation = AVR_OPERATION_CBI, .immediate = AVR_IO_PORTB, .bit_index = 5},
        {.operation = AVR_OPERATION_RJMP, .relative_offset = 0},
        {.operation = AVR_OPERATION_SBI, .immediate = AVR_IO_PORTB, .bit_index = 5}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];

    mcu.registers[17] = UINT8_C(0x20);
    mcu.ddrb = 0;
    mcu.external_input = UINT8_C(0x20);
    for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
    {
        assert(avr_encode_instruction(&program[index], &machine_code[index]));
    }
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    for (size_t step = 0; step < 6; ++step)
    {
        assert(avr_mcu_step(&mcu));
    }
    assert(mcu.registers[16] == UINT8_C(0x20));
    assert(mcu.portb == UINT8_C(0x20));
    assert(mcu.pc == 7);
}

void test_phase6_failure_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint16_t word = UINT16_C(0xaaaa);

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_AND,
                                       .destination_register = 32,
                                       .source_register = 0},
                                   &word));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_CPI,
                                       .destination_register = 15,
                                       .immediate = 1},
                                   &word));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_SBI,
                                       .immediate = 32,
                                       .bit_index = 0},
                                   &word));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_CBI,
                                       .immediate = 0,
                                       .bit_index = 8},
                                   &word));

    mcu.pc = 9;
    mcu.sreg = AVR_SREG_C;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_SBI,
                                              .immediate = AVR_IO_PINB,
                                              .bit_index = 0}));
    assert(mcu.pc == 9);
    assert(mcu.sreg == AVR_SREG_C);
}
