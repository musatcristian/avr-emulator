#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "avr_debug.h"
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

void test_snapshot_reflects_mcu_state(void)
{
    AvrMCU mcu = avr_mcu_create();
    AvrSnapshot snapshot;
    uint16_t inc_word;

    assert(avr_mcu_write_register(&mcu, 3, 42));
    assert(avr_mcu_write_data(&mcu, 10, 0x55));
    assert(avr_mcu_write_io(&mcu, AVR_IO_DDRB, 0x0f));
    assert(avr_mcu_write_io(&mcu, AVR_IO_PORTB, 0x0a));
    assert(avr_mcu_write_external_input(&mcu, 0xf0));
    avr_mcu_write_sp(&mcu, 100);
    assert(avr_mcu_set_breakpoint(&mcu, 0));

    assert(avr_encode_instruction(&(AvrInstruction){
                                      .operation = AVR_OPERATION_INC,
                                      .destination_register = 16},
                                  &inc_word));
    assert(avr_mcu_write_flash(&mcu, 0, inc_word));
    assert(avr_mcu_step(&mcu));
    /* Step already advanced pc/cycle_count/sreg/register 16; rewind pc so
     * the snapshot is taken while flash[0] (the INC word) is still current. */
    avr_mcu_write_pc(&mcu, 0);
    avr_mcu_write_sreg(&mcu, AVR_SREG_Z);

    assert(avr_debug_snapshot(&mcu, &snapshot));

    assert(snapshot.pc == 0);
    assert(snapshot.instruction_word == inc_word);
    assert(snapshot.instruction_valid);
    assert(snapshot.instruction.operation == AVR_OPERATION_INC);
    assert(snapshot.instruction.destination_register == 16);
    assert(snapshot.registers[3] == 42);
    assert(snapshot.sreg == AVR_SREG_Z);
    assert(snapshot.sram[10] == 0x55);
    assert(snapshot.ddrb == 0x0f);
    assert(snapshot.portb == 0x0a);
    /* PINB reads output bits from PORTB and input bits from external_input. */
    assert(snapshot.pinb == (uint8_t)((0x0a & 0x0f) | (0xf0 & ~0x0f)));
    assert(snapshot.external_input == 0xf0);
    assert(snapshot.sp == 100);
    assert(snapshot.cycle_count == 1);
    assert(snapshot.breakpoint_at_pc);
}

void test_snapshot_is_read_only(void)
{
    AvrMCU mcu = avr_mcu_create();
    AvrMCU before;
    AvrSnapshot snapshot;

    assert(avr_mcu_write_register(&mcu, 5, 7));
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xffff)));
    before = mcu;

    assert(avr_debug_snapshot(&mcu, &snapshot));
    assert(memcmp(&before, &mcu, sizeof(mcu)) == 0);
}

void test_snapshot_reports_invalid_instruction(void)
{
    AvrMCU mcu = avr_mcu_create();
    AvrSnapshot snapshot;

    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xffff)));

    assert(avr_debug_snapshot(&mcu, &snapshot));
    assert(snapshot.instruction_word == UINT16_C(0xffff));
    assert(!snapshot.instruction_valid);
}

void test_format_instruction_examples(void)
{
    char buffer[64];

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_LDI,
                                            .destination_register = 16,
                                            .immediate = 5},
                                        0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "LDI R16, 5") == 0);

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_MOV,
                                            .destination_register = 1,
                                            .source_register = 2},
                                        0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "MOV R1, R2") == 0);

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_OUT,
                                            .source_register = 16,
                                            .immediate = AVR_IO_DDRB},
                                        0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "OUT DDRB, R16") == 0);

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_RJMP,
                                            .relative_offset = -2},
                                        5, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "RJMP -2 (0x0004)") == 0);

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_SBI,
                                            .immediate = AVR_IO_PORTB,
                                            .bit_index = 5},
                                        0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "SBI PORTB, 5") == 0);

    assert(avr_debug_format_instruction(&(AvrInstruction){
                                            .operation = AVR_OPERATION_CALL,
                                            .target_address = 0x10},
                                        0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "CALL 0x0010") == 0);

    assert(avr_debug_format_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_RET}, 0, buffer,
        sizeof(buffer)));
    assert(strcmp(buffer, "RET") == 0);
}

void test_format_instruction_rejects_short_buffer(void)
{
    char buffer[3];

    assert(!avr_debug_format_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_RET}, 0, buffer,
        sizeof(buffer)));
    assert(!avr_debug_format_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_LDI,
                          .destination_register = 16,
                          .immediate = 5},
        0, NULL, 0));
}

void test_explain_instruction_examples(void)
{
    char buffer[96];

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_ADD,
                                             .destination_register = 1,
                                             .source_register = 2},
                                         0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "Add R2 to R1 and store the result in R1.") == 0);

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_LDI,
                                             .destination_register = 16,
                                             .immediate = 5},
                                         0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "Load the number 5 into register R16.") == 0);

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_OUT,
                                             .source_register = 16,
                                             .immediate = AVR_IO_DDRB},
                                         0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "Copy R16 into io-register DDRB.") == 0);

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_BREQ,
                                             .relative_offset = 2},
                                         5, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "If the last result was zero, jump to line 8; "
                          "otherwise continue to the next line.") == 0);

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_SBI,
                                             .immediate = AVR_IO_PORTB,
                                             .bit_index = 5},
                                         0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "Turn on bit 5 of io-register PORTB (e.g. drive a GPIO "
                          "pin high or mark it an output).") == 0);

    assert(avr_debug_explain_instruction(&(AvrInstruction){
                                             .operation = AVR_OPERATION_CALL,
                                             .target_address = 0x10},
                                         0, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "Jump to line 16 and remember where to come back to.") == 0);

    assert(avr_debug_explain_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_RET}, 0, buffer,
        sizeof(buffer)));
    assert(strcmp(buffer, "Return to the line right after the last CALL.") == 0);
}

void test_explain_instruction_rejects_short_buffer(void)
{
    char buffer[3];

    assert(!avr_debug_explain_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_RET}, 0, buffer,
        sizeof(buffer)));
    assert(!avr_debug_explain_instruction(
        &(AvrInstruction){.operation = AVR_OPERATION_LDI,
                          .destination_register = 16,
                          .immediate = 5},
        0, NULL, 0));
}

void test_flag_name_examples(void)
{
    assert(strcmp(avr_debug_flag_name(AVR_SREG_I), "Interrupt Enable") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_T), "T-bit") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_H), "Half-Carry") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_S), "Sign") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_V), "Overflow") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_N), "Negative") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_Z), "Zero") == 0);
    assert(strcmp(avr_debug_flag_name(AVR_SREG_C), "Carry") == 0);
    assert(strcmp(avr_debug_flag_name(0), "Unknown") == 0);
}

void test_step_with_events_reports_changes(void)
{
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 0xff},
        {.operation = AVR_OPERATION_INC, .destination_register = 16},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 16,
         .immediate = AVR_IO_DDRB},
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 17,
         .immediate = 0xaa},
        {.operation = AVR_OPERATION_OUT,
         .source_register = 17,
         .immediate = AVR_IO_PORTB}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrMCU mcu = avr_mcu_create();
    AvrEventLog events;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    /* LDI R16, 0xff: pc and register 16 change, sreg is untouched. */
    assert(avr_debug_step_with_events(&mcu, &events));
    assert(events.count == 2);
    assert(events.events[0].type == AVR_EVENT_PC_CHANGED);
    assert(events.events[0].old_value == 0 && events.events[0].new_value == 1);
    assert(events.events[1].type == AVR_EVENT_REGISTER_CHANGED);
    assert(events.events[1].address == 16);
    assert(events.events[1].old_value == 0 && events.events[1].new_value == 0xff);

    /* INC R16 wraps 0xff to 0x00: sreg's Z flag flips on too. */
    assert(avr_debug_step_with_events(&mcu, &events));
    {
        bool saw_sreg_change = false;
        bool saw_register_change = false;

        for (size_t index = 0; index < events.count; ++index)
        {
            if (events.events[index].type == AVR_EVENT_SREG_CHANGED)
            {
                saw_sreg_change = true;
            }
            if (events.events[index].type == AVR_EVENT_REGISTER_CHANGED &&
                events.events[index].address == 16)
            {
                saw_register_change = true;
                assert(events.events[index].new_value == 0);
            }
        }
        assert(saw_sreg_change);
        assert(saw_register_change);
    }

    /* OUT DDRB, R16 configures all 8 pins as outputs. */
    assert(avr_mcu_write_register(&mcu, 16, 0xff));
    assert(avr_debug_step_with_events(&mcu, &events));
    {
        bool saw_io_change = false;

        for (size_t index = 0; index < events.count; ++index)
        {
            if (events.events[index].type == AVR_EVENT_IO_CHANGED &&
                events.events[index].address == AVR_IO_DDRB)
            {
                saw_io_change = true;
                assert(events.events[index].old_value == 0);
                assert(events.events[index].new_value == 0xff);
            }
        }
        assert(saw_io_change);
    }

    /* LDI R17, 0xaa: only register 17 changes. */
    assert(avr_debug_step_with_events(&mcu, &events));

    /* OUT PORTB, R17: PORTB and (since DDRB is all-output) PINB both change. */
    assert(avr_debug_step_with_events(&mcu, &events));
    {
        bool saw_portb_change = false;
        bool saw_pin_change = false;

        for (size_t index = 0; index < events.count; ++index)
        {
            if (events.events[index].type == AVR_EVENT_IO_CHANGED &&
                events.events[index].address == AVR_IO_PORTB)
            {
                saw_portb_change = true;
                assert(events.events[index].new_value == 0xaa);
            }
            if (events.events[index].type == AVR_EVENT_PIN_CHANGED)
            {
                saw_pin_change = true;
                assert(events.events[index].new_value == 0xaa);
            }
        }
        assert(saw_portb_change);
        assert(saw_pin_change);
    }
}

void test_step_with_events_failure_semantics(void)
{
    AvrMCU mcu = avr_mcu_create();
    AvrMCU before;
    AvrEventLog events;

    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xffff)));
    before = mcu;
    events.count = 123;

    assert(!avr_debug_step_with_events(&mcu, &events));
    assert(memcmp(&before, &mcu, sizeof(mcu)) == 0);
    assert(events.count == 0);

    assert(!avr_debug_step_with_events(NULL, &events));
}

void test_step_with_events_matches_plain_step(void)
{
    const AvrInstruction program[] = {
        {.operation = AVR_OPERATION_LDI,
         .destination_register = 16,
         .immediate = 3},
        {.operation = AVR_OPERATION_DEC, .destination_register = 16},
        {.operation = AVR_OPERATION_BRNE, .relative_offset = -2}};
    uint16_t machine_code[sizeof(program) / sizeof(program[0])];
    AvrMCU stepped = avr_mcu_create();
    AvrMCU observed = avr_mcu_create();
    AvrEventLog events;

    encode_program(program, sizeof(program) / sizeof(program[0]), machine_code);
    assert(avr_mcu_load_program(&stepped, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));
    assert(avr_mcu_load_program(&observed, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));

    for (size_t step = 0; step < 7; ++step)
    {
        assert(avr_mcu_step(&stepped));
        assert(avr_debug_step_with_events(&observed, &events));
    }

    assert(memcmp(&stepped, &observed, sizeof(stepped)) == 0);
}
