#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "avr_instruction.h"

static void execute(AvrCpu *cpu, AvrOperation operation, uint8_t destination,
                    uint8_t source)
{
  AvrInstruction instruction = {
    .operation = operation,
    .destination_register = destination,
    .source_register = source
  };

  assert(avr_execute_instruction(cpu, &instruction));
}

static void test_reset_clears_cpu(void)
{
  AvrCpu cpu = avr_cpu_create();

  for (uint8_t index = 0; index < AVR_REGISTER_COUNT; ++index)
  {
    assert(avr_cpu_write_register(&cpu, index, UINT8_C(0xa5)));
  }
  avr_cpu_write_pc(&cpu, UINT16_C(0xffff));
  avr_cpu_write_sreg(&cpu, UINT8_C(0xff));

  avr_cpu_reset(&cpu);

  for (uint8_t index = 0; index < AVR_REGISTER_COUNT; ++index)
  {
    uint8_t value = UINT8_C(0xff);
    assert(avr_cpu_read_register(&cpu, index, &value));
    assert(value == 0);
  }
  assert(avr_cpu_read_pc(&cpu) == 0);
  assert(avr_cpu_read_sreg(&cpu) == 0);
  printf("test_reset_clears_cpu passed!\n");
}

static void test_add_flags(void)
{
  AvrCpu cpu = avr_cpu_create();

  cpu.registers[16] = UINT8_C(0xff);
  cpu.registers[17] = UINT8_C(0x01);
  execute(&cpu, AVR_OPERATION_ADD, 16, 17);
  assert(cpu.registers[16] == 0);
  assert(cpu.sreg == (AVR_SREG_C | AVR_SREG_H | AVR_SREG_Z));

  cpu.registers[16] = UINT8_C(0x7f);
  cpu.registers[17] = UINT8_C(0x01);
  execute(&cpu, AVR_OPERATION_ADD, 16, 17);
  assert(cpu.registers[16] == UINT8_C(0x80));
  assert(cpu.sreg == (AVR_SREG_H | AVR_SREG_V | AVR_SREG_N));
}

static void test_sub_flags(void)
{
  AvrCpu cpu = avr_cpu_create();

  cpu.registers[16] = 0;
  cpu.registers[17] = UINT8_C(0x01);
  execute(&cpu, AVR_OPERATION_SUB, 16, 17);
  assert(cpu.registers[16] == UINT8_C(0xff));
  assert(cpu.sreg == (AVR_SREG_C | AVR_SREG_H | AVR_SREG_N | AVR_SREG_S));

  cpu.registers[16] = UINT8_C(0x80);
  cpu.registers[17] = UINT8_C(0x01);
  execute(&cpu, AVR_OPERATION_SUB, 16, 17);
  assert(cpu.registers[16] == UINT8_C(0x7f));
  assert(cpu.sreg == (AVR_SREG_H | AVR_SREG_V | AVR_SREG_S));
}

static void test_inc_flags_and_preservation(void)
{
  AvrCpu cpu = avr_cpu_create();

  cpu.sreg = AVR_SREG_I | AVR_SREG_T | AVR_SREG_C | AVR_SREG_H;
  cpu.registers[16] = UINT8_C(0x7f);
  execute(&cpu, AVR_OPERATION_INC, 16, 0);
  assert(cpu.registers[16] == UINT8_C(0x80));
  assert(cpu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C | AVR_SREG_H |
                      AVR_SREG_V | AVR_SREG_N));

  cpu.registers[16] = UINT8_C(0xff);
  execute(&cpu, AVR_OPERATION_INC, 16, 0);
  assert(cpu.registers[16] == 0);
  assert(cpu.sreg == (AVR_SREG_I | AVR_SREG_T | AVR_SREG_C | AVR_SREG_H |
                      AVR_SREG_Z));
}

static void test_arithmetic_preserves_interrupt_and_transfer_flags(void)
{
  AvrCpu cpu = avr_cpu_create();

  cpu.sreg = AVR_SREG_I | AVR_SREG_T;
  cpu.registers[16] = 1;
  cpu.registers[17] = 1;
  execute(&cpu, AVR_OPERATION_ADD, 16, 17);
  assert((cpu.sreg & (AVR_SREG_I | AVR_SREG_T)) ==
         (AVR_SREG_I | AVR_SREG_T));
}

int main(void)
{
  test_reset_clears_cpu();
  test_add_flags();
  test_sub_flags();
  test_inc_flags_and_preservation();
  test_arithmetic_preserves_interrupt_and_transfer_flags();
  printf("All tests passed!\n");
  return 0;
}
