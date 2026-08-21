#include <stdbool.h>
#include <stdio.h>

#include "avr_instruction.h"

static uint16_t read_x(const AvrMCU *mcu)
{
  return (uint16_t)(((uint16_t)mcu->registers[27] << 8) | mcu->registers[26]);
}

static void print_mcu(const AvrMCU *mcu)
{
  uint16_t x = read_x(mcu);
  uint8_t data = 0;
  bool data_ok = avr_mcu_read_data(mcu, x, &data);

  printf("PC:   %04x\n", (unsigned int)avr_mcu_read_pc(mcu));
  printf("R26:  %02x\n", (unsigned int)mcu->registers[26]);
  printf("R27:  %02x\n", (unsigned int)mcu->registers[27]);
  printf("X:    %04x\n", (unsigned int)x);
  printf("R16:  %02x\n", (unsigned int)mcu->registers[16]);
  printf("R17:  %02x\n", (unsigned int)mcu->registers[17]);
  if (data_ok)
  {
    printf("SRAM[X]: %02x\n", (unsigned int)data);
  }
  else
  {
    printf("SRAM[X]: --\n");
  }
  printf("SREG: %02x\n", (unsigned int)avr_mcu_read_sreg(mcu));
}

static bool run_step(const char *name, AvrMCU *mcu)
{
  printf("Execute %s\n", name);
  if (!avr_mcu_step(mcu))
  {
    fprintf(stderr, "Failed to execute %s\n", name);
    return false;
  }

  print_mcu(mcu);
  putchar('\n');
  return true;
}

const AvrInstruction program[] = {
    {.operation = AVR_OPERATION_LDI,
     .destination_register = 26,
     .immediate = 0x22},
    {.operation = AVR_OPERATION_LDI,
     .destination_register = 27,
     .immediate = 0x00},
    {.operation = AVR_OPERATION_LDI,
     .destination_register = 16,
     .immediate = 0x55},
    {.operation = AVR_OPERATION_ST,
     .destination_register = 0,
     .source_register = 16},
    {.operation = AVR_OPERATION_LD,
     .destination_register = 17},
    {.operation = AVR_OPERATION_INC,
     .destination_register = 17}};

const char *step_names[] = {
    "LDI R26, 0x22",
    "LDI R27, 0x00",
    "LDI R16, 0x55",
    "ST X, R16",
    "LD R17, X",
    "INC R17"};

int main(void)
{
  AvrMCU mcu = avr_mcu_create();
  uint16_t machine_code[sizeof(program) / sizeof(program[0])];

  for (size_t index = 0; index < sizeof(program) / sizeof(program[0]); ++index)
  {
    if (!avr_encode_instruction(&program[index], &machine_code[index]))
    {
      fprintf(stderr, "Failed to encode instruction %zu\n", index);
      return 1;
    }
  }

  if (!avr_mcu_load_program(&mcu, machine_code,
                            sizeof(machine_code) / sizeof(machine_code[0])))
  {
    fprintf(stderr, "Failed to load machine code into flash\n");
    return 1;
  }

  printf("mcu reset\n\n");
  print_mcu(&mcu);
  putchar('\n');

  for (size_t index = 0; index < sizeof(step_names) / sizeof(step_names[0]);
       ++index)
  {
    if (!run_step(step_names[index], &mcu))
    {
      return 1;
    }
  }

  return 0;
}
