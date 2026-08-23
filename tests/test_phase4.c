#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_instruction.h"

void test_gpio_io_api(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = UINT8_C(0xff);

    assert(avr_mcu_read_io(&mcu, AVR_IO_DDRB, &value));
    assert(value == 0);
    assert(avr_mcu_write_io(&mcu, AVR_IO_DDRB, UINT8_C(0xaa)));
    assert(avr_mcu_write_io(&mcu, AVR_IO_PORTB, UINT8_C(0x55)));
    assert(avr_mcu_write_external_input(&mcu, UINT8_C(0xf0)));
    assert(avr_mcu_read_io(&mcu, AVR_IO_DDRB, &value));
    assert(value == UINT8_C(0xaa));
    assert(avr_mcu_read_io(&mcu, AVR_IO_PORTB, &value));
    assert(value == UINT8_C(0x55));
    assert(avr_mcu_read_external_input(&mcu, &value));
    assert(value == UINT8_C(0xf0));

    value = UINT8_C(0x33);
    assert(!avr_mcu_read_io(&mcu, UINT8_C(0x3f), &value));
    assert(value == UINT8_C(0x33));
    assert(!avr_mcu_read_io(NULL, AVR_IO_PINB, &value));
    assert(!avr_mcu_read_io(&mcu, AVR_IO_PINB, NULL));
    assert(!avr_mcu_write_io(&mcu, AVR_IO_PINB, UINT8_C(0xff)));
    assert(!avr_mcu_write_io(&mcu, UINT8_C(0x3f), UINT8_C(0xff)));
    assert(!avr_mcu_write_external_input(NULL, UINT8_C(0xff)));

    avr_mcu_reset(&mcu);
    assert(avr_mcu_read_io(&mcu, AVR_IO_PINB, &value));
    assert(value == 0);
    assert(avr_mcu_read_external_input(&mcu, &value));
    assert(value == 0);
}

void test_gpio_pin_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint8_t value = 0;

    assert(avr_mcu_write_external_input(&mcu, UINT8_C(0xf0)));
    assert(avr_mcu_read_io(&mcu, AVR_IO_PINB, &value));
    assert(value == UINT8_C(0xf0));

    assert(avr_mcu_write_io(&mcu, AVR_IO_DDRB, UINT8_C(0xff)));
    assert(avr_mcu_write_io(&mcu, AVR_IO_PORTB, UINT8_C(0x3c)));
    assert(avr_mcu_read_io(&mcu, AVR_IO_PINB, &value));
    assert(value == UINT8_C(0x3c));

    assert(avr_mcu_write_io(&mcu, AVR_IO_DDRB, UINT8_C(0xaa)));
    assert(avr_mcu_write_io(&mcu, AVR_IO_PORTB, UINT8_C(0x55)));
    assert(avr_mcu_write_external_input(&mcu, UINT8_C(0xf0)));
    assert(avr_mcu_read_io(&mcu, AVR_IO_PINB, &value));
    assert(value == UINT8_C(0x50));
}

void test_in_out_instruction_encoding(void)
{
    uint16_t word = 0;

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_IN,
                                      .destination_register = 0,
                                      .immediate = AVR_IO_PINB},
                                  &word));
    assert(word == UINT16_C(0xb000));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_IN,
                                      .destination_register = 31,
                                      .immediate = UINT8_C(0x3f)},
                                  &word));
    assert(word == UINT16_C(0xb7ff));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_OUT,
                                      .source_register = 0,
                                      .immediate = AVR_IO_PINB},
                                  &word));
    assert(word == UINT16_C(0xb800));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_OUT,
                                      .source_register = 31,
                                      .immediate = UINT8_C(0x3f)},
                                  &word));
    assert(word == UINT16_C(0xbfff));

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_IN,
                                       .destination_register = AVR_REGISTER_COUNT,
                                       .immediate = AVR_IO_PINB},
                                   &word));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_OUT,
                                       .source_register = AVR_REGISTER_COUNT,
                                       .immediate = AVR_IO_PORTB},
                                   &word));
}

void test_in_out_instruction_decoding_and_roundtrip(void)
{
    AvrInstruction instruction = {0};
    const AvrInstruction originals[] = {
        {.operation = AVR_OPERATION_IN,
         .destination_register = 4,
         .immediate = AVR_IO_DDRB},
        {.operation = AVR_OPERATION_IN,
         .destination_register = 31,
         .immediate = UINT8_C(0x3f)},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 12,
         .immediate = AVR_IO_PORTB},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 0,
         .immediate = AVR_IO_PINB}};

    assert(avr_decode_instruction_word(UINT16_C(0xb121), &instruction));
    assert(instruction.operation == AVR_OPERATION_IN);
    assert(instruction.destination_register == 18);
    assert(instruction.immediate == 1);
    assert(avr_decode_instruction_word(UINT16_C(0xbfff), &instruction));
    assert(instruction.operation == AVR_OPERATION_OUT);
    assert(instruction.source_register == 31);
    assert(instruction.immediate == 63);
    assert(!avr_decode_instruction_word(UINT16_C(0xa000), &instruction));

    for (size_t index = 0; index < sizeof(originals) / sizeof(originals[0]);
         ++index)
    {
        uint16_t word = 0;
        AvrInstruction decoded = {0};
        assert(avr_encode_instruction(&originals[index], &word));
        assert(avr_decode_instruction_word(word, &decoded));
        assert(decoded.operation == originals[index].operation);
        assert(decoded.destination_register == originals[index].destination_register);
        assert(decoded.source_register == originals[index].source_register);
        assert(decoded.immediate == originals[index].immediate);
    }
}

void test_in_out_direct_execution(void)
{
    AvrMCU mcu = avr_mcu_create();
    mcu.sreg = AVR_SREG_I | AVR_SREG_T | AVR_SREG_C;
    mcu.registers[16] = UINT8_C(0xaa);
    mcu.registers[17] = UINT8_C(0xff);

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_OUT,
                                             .source_register = 16,
                                             .immediate = AVR_IO_DDRB}));
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_OUT,
                                             .source_register = 16,
                                             .immediate = AVR_IO_PORTB}));
    assert(avr_mcu_write_external_input(&mcu, UINT8_C(0x0f)));
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_IN,
                                             .destination_register = 17,
                                             .immediate = AVR_IO_PINB}));
    assert(mcu.registers[17] == UINT8_C(0xaf));
    assert(mcu.registers[16] == UINT8_C(0xaa));
    assert(mcu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C));
    assert(mcu.pc == 3);

    mcu.pc = 9;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_OUT,
                                              .source_register = 16,
                                              .immediate = AVR_IO_PINB}));
    assert(mcu.pc == 9);
    assert(mcu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C));
}

void test_phase4_machine_code_integration(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = UINT8_C(0xaa)},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 16,
         .immediate = AVR_IO_DDRB},
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = UINT8_C(0xf0)},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 16,
         .immediate = AVR_IO_PORTB},
        {.operation = AVR_OPERATION_IN,
         .destination_register = 17,
         .immediate = AVR_IO_PINB}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];

    assert(avr_mcu_write_external_input(&mcu, UINT8_C(0x0f)));
    for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
    {
        assert(avr_encode_instruction(&program[index], &machine_code[index]));
    }
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
    {
        assert(avr_mcu_step(&mcu));
        assert(mcu.pc == index + 1);
    }

    assert(mcu.ddrb == UINT8_C(0xaa));
    assert(mcu.portb == UINT8_C(0xf0));
    assert(mcu.registers[17] == UINT8_C(0xa5));
    assert(mcu.sreg == 0);
}
