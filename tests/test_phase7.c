#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_instruction.h"

void test_phase7_instruction_encoding(void)
{
    uint16_t word = 0;

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_PUSH,
                                      .source_register = 18},
                                  &word));
    assert(word == UINT16_C(0x932f));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_POP,
                                      .destination_register = 5},
                                  &word));
    assert(word == UINT16_C(0x905f));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_CALL,
                                      .target_address = 0},
                                  &word));
    assert(word == UINT16_C(0x9c00));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_CALL,
                                      .target_address = AVR_FLASH_SIZE - 1},
                                  &word));
    assert(word == UINT16_C(0x9fff));
    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_RET},
                                  &word));
    assert(word == UINT16_C(0x9508));
}

void test_phase7_instruction_decoding_and_roundtrip(void)
{
    const AvrInstruction originals[] = {
        {.operation = AVR_OPERATION_PUSH, .source_register = 31},
        {.operation = AVR_OPERATION_POP, .destination_register = 0},
        {.operation = AVR_OPERATION_CALL, .target_address = 1023},
        {.operation = AVR_OPERATION_CALL, .target_address = 0},
        {.operation = AVR_OPERATION_RET}};

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
        assert(decoded.target_address == originals[index].target_address);
    }
}

void test_push_pop_execution_and_stack_bounds(void)
{
    AvrMCU mcu = avr_mcu_create();

    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);

    mcu.registers[16] = UINT8_C(0xab);
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_PUSH,
                                             .source_register = 16}));
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE - 1);
    assert(mcu.sram[AVR_SRAM_SIZE - 1] == UINT8_C(0xab));

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_POP,
                                             .destination_register = 17}));
    assert(mcu.registers[17] == UINT8_C(0xab));
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);

    /* Overflow: no room left below the stack pointer to push into. */
    avr_mcu_write_sp(&mcu, 0);
    mcu.pc = 4;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_PUSH,
                                              .source_register = 16}));
    assert(avr_mcu_read_sp(&mcu) == 0);
    assert(mcu.pc == 4);

    /* Underflow: nothing has been pushed yet. */
    avr_mcu_write_sp(&mcu, AVR_SRAM_SIZE);
    mcu.registers[18] = UINT8_C(0x11);
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_POP,
                                              .destination_register = 18}));
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);
    assert(mcu.registers[18] == UINT8_C(0x11));
    assert(mcu.pc == 4);
}

void test_call_ret_execution_and_stack_bounds(void)
{
    AvrMCU mcu = avr_mcu_create();

    mcu.pc = 10;
    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_CALL,
                                             .target_address = 50}));
    assert(mcu.pc == 50);
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE - 2);
    assert(mcu.sram[AVR_SRAM_SIZE - 2] == UINT8_C(11));
    assert(mcu.sram[AVR_SRAM_SIZE - 1] == 0);

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_RET}));
    assert(mcu.pc == 11);
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);

    /* Overflow: fewer than two free bytes remain for the return address. */
    avr_mcu_write_sp(&mcu, 1);
    mcu.pc = 20;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_CALL,
                                              .target_address = 5}));
    assert(mcu.pc == 20);
    assert(avr_mcu_read_sp(&mcu) == 1);

    /* Underflow: fewer than two stored bytes remain to reconstruct a PC. */
    avr_mcu_write_sp(&mcu, AVR_SRAM_SIZE - 1);
    mcu.pc = 20;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_RET}));
    assert(mcu.pc == 20);
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE - 1);

    /* An out-of-range target is rejected without touching PC or the stack. */
    avr_mcu_write_sp(&mcu, AVR_SRAM_SIZE);
    mcu.pc = 20;
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_CALL,
                                              .target_address = AVR_FLASH_SIZE}));
    assert(mcu.pc == 20);
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);
}

void test_phase7_machine_code_nested_subroutines(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI, .destination_register = 16, .immediate = 1},
        {.operation = AVR_OPERATION_CALL, .target_address = 4},
        {.operation = AVR_OPERATION_INC, .destination_register = 16},
        {.operation = AVR_OPERATION_RJMP, .relative_offset = -1},
        {.operation = AVR_OPERATION_CALL, .target_address = 6},
        {.operation = AVR_OPERATION_RET},
        {.operation = AVR_OPERATION_INC, .destination_register = 16},
        {.operation = AVR_OPERATION_RET}};
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

    assert(mcu.registers[16] == 3);
    assert(mcu.pc == 3);
    assert(avr_mcu_read_sp(&mcu) == AVR_SRAM_SIZE);
}

void test_phase7_failure_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    uint16_t word = UINT16_C(0xaaaa);

    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_PUSH,
                                       .source_register = 32},
                                   &word));
    assert(word == UINT16_C(0xaaaa));
    assert(!avr_encode_instruction(&(AvrInstruction){
                                       .operation = AVR_OPERATION_CALL,
                                       .target_address = AVR_FLASH_SIZE},
                                   &word));
    assert(word == UINT16_C(0xaaaa));

    mcu.pc = 9;
    mcu.sreg = AVR_SREG_C;
    avr_mcu_write_sp(&mcu, 0);
    assert(!avr_execute_instruction(&mcu, &(AvrInstruction){
                                              .operation = AVR_OPERATION_PUSH,
                                              .source_register = 16}));
    assert(mcu.pc == 9);
    assert(mcu.sreg == AVR_SREG_C);
    assert(avr_mcu_read_sp(&mcu) == 0);
}
