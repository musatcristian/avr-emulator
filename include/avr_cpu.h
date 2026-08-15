#ifndef AVR_CPU_H
#define AVR_CPU_H

#include <stdint.h>

enum
{
  AVR_REGISTER_COUNT = 32
};

typedef struct
{
  uint8_t registers[AVR_REGISTER_COUNT];
  uint16_t pc;
  uint8_t sreg;
} AvrCpu;

#endif
