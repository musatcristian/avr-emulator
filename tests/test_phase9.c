#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "avr_instruction.h"
#include "avr_mcu.h"

static void encode_program(const AvrInstruction *program, size_t count,
                           uint16_t *machine_code)
{
    for (size_t index = 0; index < count; ++index)
    {
        assert(avr_encode_instruction(&program[index], &machine_code[index]));
    }
}

void test_cycle_counter_increments_on_success_only(void)
{
    AvrMCU mcu = avr_mcu_create();

    assert(avr_mcu_read_cycle_count(&mcu) == 0);

    assert(avr_execute_instruction(&mcu, &(AvrInstruction){
                                             .operation = AVR_OPERATION_LDI,
                                             .destination_register = 16,
                                             .immediate = 5}));
    /* Direct avr_execute_instruction calls bypass avr_mcu_step's fetch/decode,
     * so the abstract cycle counter only advances through avr_mcu_step; it
     * does advance pc like any normal instruction, so rewind it for the rest
     * of this test. */
    assert(avr_mcu_read_cycle_count(&mcu) == 0);
    avr_mcu_write_pc(&mcu, 0);

    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xffff)));
    assert(!avr_mcu_step(&mcu));
    assert(avr_mcu_read_cycle_count(&mcu) == 0);
    assert(avr_mcu_read_pc(&mcu) == 0);

    {
        AvrInstruction inc = {.operation = AVR_OPERATION_INC,
                              .destination_register = 16};
        uint16_t word = 0;

        assert(avr_encode_instruction(&inc, &word));
        assert(avr_mcu_write_flash(&mcu, 0, word));
    }
    assert(avr_mcu_step(&mcu));
    assert(avr_mcu_read_cycle_count(&mcu) == 1);
    assert(mcu.registers[16] == 6);
}

void test_breakpoint_api(void)
{
    AvrMCU mcu = avr_mcu_create();

    assert(!avr_mcu_has_breakpoint(&mcu, 0));
    assert(avr_mcu_set_breakpoint(&mcu, 0));
    assert(avr_mcu_has_breakpoint(&mcu, 0));
    assert(avr_mcu_clear_breakpoint(&mcu, 0));
    assert(!avr_mcu_has_breakpoint(&mcu, 0));

    assert(!avr_mcu_set_breakpoint(&mcu, AVR_FLASH_SIZE));
    assert(!avr_mcu_clear_breakpoint(&mcu, AVR_FLASH_SIZE));
    assert(!avr_mcu_has_breakpoint(&mcu, AVR_FLASH_SIZE));
    assert(!avr_mcu_set_breakpoint(NULL, 0));
}

void test_run_stops_on_cycle_limit(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_RJMP, .relative_offset = -1}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrRunResult result;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    result = avr_mcu_run(&mcu, 5);
    assert(result.reason == AVR_RUN_STOP_CYCLE_LIMIT);
    assert(result.cycles_executed == 5);
    assert(avr_mcu_read_cycle_count(&mcu) == 5);
    assert(mcu.pc == 0);
}

void test_run_stops_on_breakpoint(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 3},
        {.operation = AVR_OPERATION_DEC, .destination_register = 16},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = -2},
        {.operation = AVR_OPERATION_INC, .destination_register = 17}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrRunResult result;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));
    /* The loop body (addresses 1-2) is not where the breakpoint sits, so the
     * loop runs to completion and only the trailing INC trips it. */
    assert(avr_mcu_set_breakpoint(&mcu, 3));

    result = avr_mcu_run(&mcu, 100);
    assert(result.reason == AVR_RUN_STOP_BREAKPOINT);
    assert(result.cycles_executed == 7);
    assert(mcu.pc == 3);
    assert(mcu.registers[16] == 0);
    assert(mcu.registers[17] == 0);
    assert(avr_mcu_read_cycle_count(&mcu) == 7);

    /* Resuming from the breakpoint's own address must not immediately
     * re-trigger it. */
    result = avr_mcu_run(&mcu, 1);
    assert(result.reason == AVR_RUN_STOP_CYCLE_LIMIT);
    assert(result.cycles_executed == 1);
    assert(mcu.registers[17] == 1);
}

void test_run_stops_on_invalid_instruction(void)
{
    AvrMCU mcu = avr_mcu_create();
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 1}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrRunResult result;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));
    assert(avr_mcu_write_flash(&mcu, 1, UINT16_C(0xffff)));

    result = avr_mcu_run(&mcu, 10);
    assert(result.reason == AVR_RUN_STOP_INVALID_INSTRUCTION);
    assert(result.cycles_executed == 1);
    assert(avr_mcu_read_cycle_count(&mcu) == 1);
    /* The failing fetch/decode must leave pc parked on the bad instruction. */
    assert(mcu.pc == 1);

    result = avr_mcu_run(NULL, 10);
    assert(result.reason == AVR_RUN_STOP_INVALID_INSTRUCTION);
    assert(result.cycles_executed == 0);
}

void test_run_equivalent_to_repeated_step(void)
{
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 3},
        {.operation = AVR_OPERATION_DEC, .destination_register = 16},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = -2}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrMCU stepped = avr_mcu_create();
    AvrMCU ran = avr_mcu_create();
    AvrRunResult result;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&stepped, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));
    assert(avr_mcu_load_program(&ran, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    for (size_t step = 0; step < 7; ++step)
    {
        assert(avr_mcu_step(&stepped));
    }
    result = avr_mcu_run(&ran, 7);

    assert(result.reason == AVR_RUN_STOP_CYCLE_LIMIT);
    assert(result.cycles_executed == 7);
    assert(memcmp(stepped.registers, ran.registers, sizeof(stepped.registers)) == 0);
    assert(stepped.pc == ran.pc);
    assert(stepped.sreg == ran.sreg);
    assert(stepped.sp == ran.sp);
    assert(stepped.cycle_count == ran.cycle_count);
    assert(memcmp(stepped.sram, ran.sram, sizeof(stepped.sram)) == 0);
}
