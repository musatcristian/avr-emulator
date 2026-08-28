#include "avr_debug.h"

#include <stdio.h>
#include <string.h>

static const char *io_name(uint8_t io_id, char *fallback, size_t fallback_size)
{
    switch (io_id)
    {
    case AVR_IO_PINB:
        return "PINB";
    case AVR_IO_DDRB:
        return "DDRB";
    case AVR_IO_PORTB:
        return "PORTB";
    default:
        snprintf(fallback, fallback_size, "IO%u", io_id);
        return fallback;
    }
}

static void push_event(AvrEventLog *events, AvrEventType type,
                       uint16_t address, uint16_t old_value,
                       uint16_t new_value)
{
    if (events == NULL || events->count >= AVR_EVENT_LOG_CAPACITY)
    {
        return;
    }

    events->events[events->count].type = type;
    events->events[events->count].address = address;
    events->events[events->count].old_value = old_value;
    events->events[events->count].new_value = new_value;
    events->count++;
}

bool avr_debug_snapshot(const AvrMCU *mcu, AvrSnapshot *out)
{
    uint16_t instruction_word;

    if (mcu == NULL || out == NULL)
    {
        return false;
    }

    out->pc = avr_mcu_read_pc(mcu);
    memcpy(out->registers, mcu->registers, sizeof(out->registers));
    out->sreg = avr_mcu_read_sreg(mcu);
    memcpy(out->sram, mcu->sram, sizeof(out->sram));
    out->sp = avr_mcu_read_sp(mcu);
    out->cycle_count = avr_mcu_read_cycle_count(mcu);
    out->breakpoint_at_pc = avr_mcu_has_breakpoint(mcu, out->pc);

    avr_mcu_read_io(mcu, AVR_IO_DDRB, &out->ddrb);
    avr_mcu_read_io(mcu, AVR_IO_PORTB, &out->portb);
    avr_mcu_read_io(mcu, AVR_IO_PINB, &out->pinb);
    avr_mcu_read_external_input(mcu, &out->external_input);

    if (avr_mcu_read_flash(mcu, out->pc, &instruction_word))
    {
        out->instruction_word = instruction_word;
        out->instruction_valid =
            avr_decode_instruction_word(instruction_word, &out->instruction);
    }
    else
    {
        out->instruction_word = 0;
        out->instruction_valid = false;
        memset(&out->instruction, 0, sizeof(out->instruction));
    }

    return true;
}

bool avr_debug_format_instruction(const AvrInstruction *instruction,
                                  uint16_t pc, char *buffer,
                                  size_t buffer_size)
{
    char io_buffer[16];
    int written;

    if (instruction == NULL || buffer == NULL || buffer_size == 0)
    {
        return false;
    }

    switch (instruction->operation)
    {
    case AVR_OPERATION_LDI:
        written = snprintf(buffer, buffer_size, "LDI R%u, %u",
                           instruction->destination_register,
                           instruction->immediate);
        break;
    case AVR_OPERATION_MOV:
        written = snprintf(buffer, buffer_size, "MOV R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_ADD:
        written = snprintf(buffer, buffer_size, "ADD R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_SUB:
        written = snprintf(buffer, buffer_size, "SUB R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_INC:
        written = snprintf(buffer, buffer_size, "INC R%u",
                           instruction->destination_register);
        break;
    case AVR_OPERATION_DEC:
        written = snprintf(buffer, buffer_size, "DEC R%u",
                           instruction->destination_register);
        break;
    case AVR_OPERATION_LD:
        written = snprintf(buffer, buffer_size, "LD R%u, X",
                           instruction->destination_register);
        break;
    case AVR_OPERATION_ST:
        written = snprintf(buffer, buffer_size, "ST X, R%u",
                           instruction->source_register);
        break;
    case AVR_OPERATION_IN:
        written = snprintf(buffer, buffer_size, "IN R%u, %s",
                           instruction->destination_register,
                           io_name(instruction->immediate, io_buffer,
                                   sizeof(io_buffer)));
        break;
    case AVR_OPERATION_OUT:
        written = snprintf(buffer, buffer_size, "OUT %s, R%u",
                           io_name(instruction->immediate, io_buffer,
                                   sizeof(io_buffer)),
                           instruction->source_register);
        break;
    case AVR_OPERATION_RJMP:
        written = snprintf(buffer, buffer_size, "RJMP %+d (0x%04x)",
                           instruction->relative_offset,
                           (unsigned)(uint16_t)(pc + 1 + instruction->relative_offset));
        break;
    case AVR_OPERATION_BRNE:
        written = snprintf(buffer, buffer_size, "BRNE %+d (0x%04x)",
                           instruction->relative_offset,
                           (unsigned)(uint16_t)(pc + 1 + instruction->relative_offset));
        break;
    case AVR_OPERATION_BREQ:
        written = snprintf(buffer, buffer_size, "BREQ %+d (0x%04x)",
                           instruction->relative_offset,
                           (unsigned)(uint16_t)(pc + 1 + instruction->relative_offset));
        break;
    case AVR_OPERATION_AND:
        written = snprintf(buffer, buffer_size, "AND R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_OR:
        written = snprintf(buffer, buffer_size, "OR R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_EOR:
        written = snprintf(buffer, buffer_size, "EOR R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_CP:
        written = snprintf(buffer, buffer_size, "CP R%u, R%u",
                           instruction->destination_register,
                           instruction->source_register);
        break;
    case AVR_OPERATION_CPI:
        written = snprintf(buffer, buffer_size, "CPI R%u, %u",
                           instruction->destination_register,
                           instruction->immediate);
        break;
    case AVR_OPERATION_SBI:
        written = snprintf(buffer, buffer_size, "SBI %s, %u",
                           io_name(instruction->immediate, io_buffer,
                                   sizeof(io_buffer)),
                           instruction->bit_index);
        break;
    case AVR_OPERATION_CBI:
        written = snprintf(buffer, buffer_size, "CBI %s, %u",
                           io_name(instruction->immediate, io_buffer,
                                   sizeof(io_buffer)),
                           instruction->bit_index);
        break;
    case AVR_OPERATION_PUSH:
        written = snprintf(buffer, buffer_size, "PUSH R%u",
                           instruction->source_register);
        break;
    case AVR_OPERATION_POP:
        written = snprintf(buffer, buffer_size, "POP R%u",
                           instruction->destination_register);
        break;
    case AVR_OPERATION_CALL:
        written = snprintf(buffer, buffer_size, "CALL 0x%04x",
                           instruction->target_address);
        break;
    case AVR_OPERATION_RET:
        written = snprintf(buffer, buffer_size, "RET");
        break;
    default:
        return false;
    }

    return written > 0 && (size_t)written < buffer_size;
}

bool avr_debug_step_with_events(AvrMCU *mcu, AvrEventLog *events)
{
    AvrSnapshot before;
    AvrSnapshot after;
    uint16_t index;

    if (events != NULL)
    {
        events->count = 0;
    }

    if (mcu == NULL || !avr_debug_snapshot(mcu, &before))
    {
        return false;
    }

    if (!avr_mcu_step(mcu))
    {
        return false;
    }

    if (events == NULL || !avr_debug_snapshot(mcu, &after))
    {
        return true;
    }

    if (before.pc != after.pc)
    {
        push_event(events, AVR_EVENT_PC_CHANGED, 0, before.pc, after.pc);
    }
    if (before.sreg != after.sreg)
    {
        push_event(events, AVR_EVENT_SREG_CHANGED, 0, before.sreg, after.sreg);
    }
    for (index = 0; index < AVR_REGISTER_COUNT; ++index)
    {
        if (before.registers[index] != after.registers[index])
        {
            push_event(events, AVR_EVENT_REGISTER_CHANGED, index,
                       before.registers[index], after.registers[index]);
        }
    }
    for (index = 0; index < AVR_SRAM_SIZE; ++index)
    {
        if (before.sram[index] != after.sram[index])
        {
            push_event(events, AVR_EVENT_SRAM_WRITTEN, index, before.sram[index],
                       after.sram[index]);
        }
    }
    if (before.ddrb != after.ddrb)
    {
        push_event(events, AVR_EVENT_IO_CHANGED, AVR_IO_DDRB, before.ddrb,
                   after.ddrb);
    }
    if (before.portb != after.portb)
    {
        push_event(events, AVR_EVENT_IO_CHANGED, AVR_IO_PORTB, before.portb,
                   after.portb);
    }
    if (before.pinb != after.pinb)
    {
        push_event(events, AVR_EVENT_PIN_CHANGED, AVR_IO_PINB, before.pinb,
                   after.pinb);
    }

    return true;
}
