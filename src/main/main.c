#include <stdbool.h>
#include <stdio.h>

#include "avr_instruction.h"

static void print_cpu(const AvrMCU *cpu)
{
  printf("PC:   %04x\n", (unsigned int)avr_cpu_read_pc(cpu));
  printf("R16:  %02x\n", (unsigned int)cpu->registers[16]);
  printf("R17:  %02x\n", (unsigned int)cpu->registers[17]);
  printf("SREG: %02x\n", (unsigned int)avr_cpu_read_sreg(cpu));
}

static bool execute(const char *name, AvrMCU *cpu, AvrInstruction instruction)
{
  printf("Execute %s\n", name);
  if (!avr_execute_instruction(cpu, &instruction))
  {
    fprintf(stderr, "Failed to execute %s\n", name);
    return false;
  }

  print_cpu(cpu);
  putchar('\n');
  return true;
}

int main(void)
{
  AvrMCU cpu = avr_cpu_create();

  printf("CPU reset\n\n");
  print_cpu(&cpu);
  putchar('\n');

  if (!execute("LDI R16, 0x05", &cpu, (AvrInstruction){
                 .operation = AVR_OPERATION_LDI,
                 .destination_register = 16,
                 .immediate = 0x05
               }) ||
      !execute("LDI R17, 0x03", &cpu, (AvrInstruction){
                 .operation = AVR_OPERATION_LDI,
                 .destination_register = 17,
                 .immediate = 0x03
               }) ||
      !execute("ADD R16, R17", &cpu, (AvrInstruction){
                 .operation = AVR_OPERATION_ADD,
                 .destination_register = 16,
                 .source_register = 17
               }))
  {
    return 1;
  }

  return 0;
}
