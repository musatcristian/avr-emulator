#include <stdbool.h>
#include <stdio.h>

#include "avr_instruction.h"

static void print_mcu(const AvrMCU *mcu)
{
  printf("PC:   %04x\n", (unsigned int)avr_mcu_read_pc(mcu));
  printf("R16:  %02x\n", (unsigned int)mcu->registers[16]);
  printf("R17:  %02x\n", (unsigned int)mcu->registers[17]);
  printf("SREG: %02x\n", (unsigned int)avr_mcu_read_sreg(mcu));
}

static bool execute(const char *name, AvrMCU *mcu, AvrInstruction instruction)
{
  printf("Execute %s\n", name);
  if (!avr_execute_instruction(mcu, &instruction))
  {
    fprintf(stderr, "Failed to execute %s\n", name);
    return false;
  }

  print_mcu(mcu);
  putchar('\n');
  return true;
}

int main(void)
{
  AvrMCU mcu = avr_mcu_create();

  printf("mcu reset\n\n");
  print_mcu(&mcu);
  putchar('\n');

  if (!execute("LDI R16, 0x05", &mcu, (AvrInstruction){.operation = AVR_OPERATION_LDI, .destination_register = 16, .immediate = 0x05}) ||
      !execute("LDI R17, 0x03", &mcu, (AvrInstruction){.operation = AVR_OPERATION_LDI, .destination_register = 17, .immediate = 0x03}) ||
      !execute("ADD R16, R17", &mcu, (AvrInstruction){.operation = AVR_OPERATION_ADD, .destination_register = 16, .source_register = 17}))
  {
    return 1;
  }

  return 0;
}
