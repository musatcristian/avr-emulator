#include "avr_cpu.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
  AvrCpu cpu = avr_cpu_create();
  
  // Test register write/read
  assert(avr_cpu_write_register(&cpu, 5, 42) == true);
  uint8_t value;
  assert(avr_cpu_read_register(&cpu, 5, &value) == true);
  assert(value == 42);
  
  // Test invalid register
  assert(avr_cpu_write_register(&cpu, 32, 100) == false);
  
  // Test PC
  avr_cpu_write_pc(&cpu, 0x1234);
  assert(avr_cpu_read_pc(&cpu) == 0x1234);
  
  printf("All tests passed!\n");
  return 0;
}
