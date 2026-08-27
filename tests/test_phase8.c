#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "avr_mcu.h"

void test_intel_hex_loader_matches_direct_program(void)
{
    const uint16_t machine_code[] = {UINT16_C(0xe005), UINT16_C(0xe013),
                                     UINT16_C(0x0f01)};
    const char *hex_text = ":0600000005E013E0010F12\n:00000001FF\n";
    AvrMCU direct_mcu = avr_mcu_create();
    AvrMCU hex_mcu = avr_mcu_create();

    assert(avr_mcu_load_program(&direct_mcu, machine_code,
                                sizeof(machine_code) / sizeof(machine_code[0])));
    assert(avr_mcu_load_intel_hex(&hex_mcu, hex_text));

    for (size_t index = 0; index < AVR_FLASH_SIZE; ++index)
    {
        assert(direct_mcu.flash[index] == hex_mcu.flash[index]);
    }
}

void test_intel_hex_loader_preserves_unwritten_flash(void)
{
    /* Data record targets word index 5 (byte address 0x000A) only. */
    const char *hex_text = ":02000A00EFBE47\n:00000001FF\n";
    AvrMCU mcu = avr_mcu_create();

    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0x1234)));
    assert(avr_mcu_load_intel_hex(&mcu, hex_text));

    assert(mcu.flash[0] == UINT16_C(0x1234));
    assert(mcu.flash[5] == UINT16_C(0xbeef));
}

void test_intel_hex_loader_failure_semantics(void)
{
    AvrMCU mcu;
    const char *bad_checksum = ":0600000005E013E0010F00\n:00000001FF\n";
    const char *unsupported_record_type = ":0600000205E013E0010F10\n";
    const char *truncated_data = ":0600000005E0";
    const char *data_after_eof = ":00000001FF\n:0600000005E013E0010F12\n";
    const char *missing_eof = ":0600000005E013E0010F12\n";
    const char *out_of_range_address = ":0207FF00AABB93";

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, bad_checksum));
    assert(mcu.flash[0] == UINT16_C(0xabcd));

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, unsupported_record_type));
    assert(mcu.flash[0] == UINT16_C(0xabcd));

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, truncated_data));
    assert(mcu.flash[0] == UINT16_C(0xabcd));

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, data_after_eof));
    assert(mcu.flash[0] == UINT16_C(0xabcd));

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, missing_eof));
    assert(mcu.flash[0] == UINT16_C(0xabcd));

    mcu = avr_mcu_create();
    assert(avr_mcu_write_flash(&mcu, 0, UINT16_C(0xabcd)));
    assert(!avr_mcu_load_intel_hex(&mcu, out_of_range_address));
    assert(mcu.flash[0] == UINT16_C(0xabcd));
    assert(mcu.flash[1023] == 0);

    assert(!avr_mcu_load_intel_hex(NULL, missing_eof));
    assert(!avr_mcu_load_intel_hex(&mcu, NULL));
}
