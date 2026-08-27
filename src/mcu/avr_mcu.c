#include <stddef.h>
#include <string.h>
#include "avr_instruction.h"
#include "avr_mcu.h"

AvrMCU avr_mcu_create(void)
{
  AvrMCU mcu = {0};

  mcu.sp = AVR_SRAM_SIZE;
  return mcu;
}

void avr_mcu_reset(AvrMCU *mcu)
{
  *mcu = avr_mcu_create();
}

bool avr_mcu_read_register(const AvrMCU *mcu, uint8_t register_index,
                           uint8_t *value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  *value = mcu->registers[register_index];
  return true;
}

bool avr_mcu_write_register(AvrMCU *mcu, uint8_t register_index,
                            uint8_t value)
{
  if (register_index >= AVR_REGISTER_COUNT)
  {
    return false;
  }

  mcu->registers[register_index] = value;
  return true;
}

uint16_t avr_mcu_read_pc(const AvrMCU *mcu)
{
  return mcu->pc;
}

void avr_mcu_write_pc(AvrMCU *mcu, uint16_t value)
{
  mcu->pc = value;
}

uint8_t avr_mcu_read_sreg(const AvrMCU *mcu)
{
  return mcu->sreg;
}

void avr_mcu_write_sreg(AvrMCU *mcu, uint8_t value)
{
  mcu->sreg = value;
}

uint16_t avr_mcu_read_sp(const AvrMCU *mcu)
{
  return mcu->sp;
}

void avr_mcu_write_sp(AvrMCU *mcu, uint16_t value)
{
  mcu->sp = value;
}

bool avr_mcu_read_flash(const AvrMCU *mcu, uint16_t address,
                        uint16_t *instruction)
{
  if (address >= AVR_FLASH_SIZE || mcu == NULL || instruction == NULL)
  {
    return false;
  }

  *instruction = mcu->flash[address];
  return true;
}

bool avr_mcu_write_flash(AvrMCU *mcu, uint16_t address,
                         uint16_t instruction)
{
  if (address >= AVR_FLASH_SIZE || mcu == NULL)
  {
    return false;
  }

  mcu->flash[address] = instruction;
  return true;
}

bool avr_mcu_read_data(const AvrMCU *mcu, uint16_t address, uint8_t *value)
{
  if (address >= AVR_SRAM_SIZE || mcu == NULL || value == NULL)
  {
    return false;
  }

  *value = mcu->sram[address];
  return true;
}

bool avr_mcu_write_data(AvrMCU *mcu, uint16_t address, uint8_t value)
{
  if (address >= AVR_SRAM_SIZE || mcu == NULL)
  {
    return false;
  }

  mcu->sram[address] = value;
  return true;
}

bool avr_mcu_read_io(const AvrMCU *mcu, uint8_t address, uint8_t *value)
{
  if (mcu == NULL || value == NULL)
  {
    return false;
  }

  if (address == AVR_IO_PINB)
  {
    *value = (uint8_t)((mcu->portb & mcu->ddrb) |
                       (mcu->external_input & (uint8_t)~mcu->ddrb));
    return true;
  }
  if (address == AVR_IO_DDRB)
  {
    *value = mcu->ddrb;
    return true;
  }
  if (address == AVR_IO_PORTB)
  {
    *value = mcu->portb;
    return true;
  }

  return false;
}

bool avr_mcu_write_io(AvrMCU *mcu, uint8_t address, uint8_t value)
{
  if (mcu == NULL)
  {
    return false;
  }

  if (address == AVR_IO_DDRB)
  {
    mcu->ddrb = value;
    return true;
  }
  if (address == AVR_IO_PORTB)
  {
    mcu->portb = value;
    return true;
  }

  return false;
}

bool avr_mcu_read_external_input(const AvrMCU *mcu, uint8_t *value)
{
  if (mcu == NULL || value == NULL)
  {
    return false;
  }

  *value = mcu->external_input;
  return true;
}

bool avr_mcu_write_external_input(AvrMCU *mcu, uint8_t value)
{
  if (mcu == NULL)
  {
    return false;
  }

  mcu->external_input = value;
  return true;
}

bool avr_mcu_load_program(AvrMCU *mcu, const uint16_t *program,
                          size_t instruction_count)
{
  if (mcu == NULL || program == NULL || instruction_count > AVR_FLASH_SIZE)
  {
    return false;
  }

  for (size_t index = 0; index < instruction_count; ++index)
  {
    mcu->flash[index] = program[index];
  }

  return true;
}

static bool hex_nibble_value(char digit, uint8_t *value)
{
  if (digit >= '0' && digit <= '9')
  {
    *value = (uint8_t)(digit - '0');
    return true;
  }
  if (digit >= 'A' && digit <= 'F')
  {
    *value = (uint8_t)(digit - 'A' + 10);
    return true;
  }
  if (digit >= 'a' && digit <= 'f')
  {
    *value = (uint8_t)(digit - 'a' + 10);
    return true;
  }

  return false;
}

static bool parse_hex_byte(const char **cursor, uint8_t *value)
{
  uint8_t high_nibble;
  uint8_t low_nibble;

  if (!hex_nibble_value((*cursor)[0], &high_nibble) ||
      !hex_nibble_value((*cursor)[1], &low_nibble))
  {
    return false;
  }

  *value = (uint8_t)((high_nibble << 4) | low_nibble);
  *cursor += 2;
  return true;
}

static bool parse_hex_u16(const char **cursor, uint16_t *value)
{
  uint8_t high_byte;
  uint8_t low_byte;

  if (!parse_hex_byte(cursor, &high_byte) || !parse_hex_byte(cursor, &low_byte))
  {
    return false;
  }

  *value = (uint16_t)(((uint16_t)high_byte << 8) | low_byte);
  return true;
}

bool avr_mcu_load_intel_hex(AvrMCU *mcu, const char *hex_text)
{
  enum
  {
    INTEL_HEX_RECORD_DATA = 0x00,
    INTEL_HEX_RECORD_EOF = 0x01,
    INTEL_HEX_MAX_DATA_BYTES = 255
  };
  uint16_t staged_flash[AVR_FLASH_SIZE];
  const char *cursor;
  bool end_of_file_seen = false;

  if (mcu == NULL || hex_text == NULL)
  {
    return false;
  }

  /* Validate every record into a staging copy first so a malformed image
   * never partially mutates the MCU's real Flash. */
  memcpy(staged_flash, mcu->flash, sizeof(staged_flash));
  cursor = hex_text;

  while (*cursor != '\0')
  {
    uint8_t byte_count;
    uint16_t address;
    uint8_t record_type;
    uint8_t checksum;
    uint8_t data[INTEL_HEX_MAX_DATA_BYTES];
    uint8_t file_checksum;

    if (*cursor == '\r' || *cursor == '\n' || *cursor == ' ' || *cursor == '\t')
    {
      ++cursor;
      continue;
    }

    if (end_of_file_seen || *cursor != ':')
    {
      return false;
    }
    ++cursor;

    if (!parse_hex_byte(&cursor, &byte_count) ||
        !parse_hex_u16(&cursor, &address) ||
        !parse_hex_byte(&cursor, &record_type))
    {
      return false;
    }

    checksum = (uint8_t)(byte_count + (address >> 8) + (address & 0xff) +
                         record_type);
    for (uint16_t index = 0; index < byte_count; ++index)
    {
      if (!parse_hex_byte(&cursor, &data[index]))
      {
        return false;
      }
      checksum = (uint8_t)(checksum + data[index]);
    }

    if (!parse_hex_byte(&cursor, &file_checksum))
    {
      return false;
    }
    if ((uint8_t)(checksum + file_checksum) != 0)
    {
      return false;
    }

    if (record_type == INTEL_HEX_RECORD_EOF)
    {
      end_of_file_seen = true;
      continue;
    }
    if (record_type != INTEL_HEX_RECORD_DATA)
    {
      /* Extended segment/linear address records are unsupported: Flash is
       * small enough that every byte address fits in 16 bits. */
      return false;
    }

    for (uint16_t index = 0; index < byte_count; ++index)
    {
      uint32_t byte_address = (uint32_t)address + index;
      uint16_t word_index;

      if (byte_address >= (uint32_t)AVR_FLASH_SIZE * 2)
      {
        return false;
      }

      word_index = (uint16_t)(byte_address / 2);
      if ((byte_address & 1) == 0)
      {
        staged_flash[word_index] = (uint16_t)((staged_flash[word_index] &
                                               0xff00u) |
                                              data[index]);
      }
      else
      {
        staged_flash[word_index] = (uint16_t)((staged_flash[word_index] &
                                               0x00ffu) |
                                              ((uint16_t)data[index] << 8));
      }
    }
  }

  if (!end_of_file_seen)
  {
    return false;
  }

  memcpy(mcu->flash, staged_flash, sizeof(staged_flash));
  return true;
}

bool avr_mcu_step(AvrMCU *mcu)
{
  uint16_t instruction_word;
  AvrInstruction instruction;

  if (mcu == NULL ||
      !avr_mcu_read_flash(mcu, avr_mcu_read_pc(mcu), &instruction_word) ||
      !avr_decode_instruction_word(instruction_word, &instruction))
  {
    return false;
  }

  return avr_execute_instruction(mcu, &instruction);
}
