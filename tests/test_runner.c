#include <stdio.h>

#include "test_phase1.h"
#include "test_phase2.h"

int main(void)
{
  test_reset_clears_cpu();
  test_ldi_and_mov();
  test_add_flags();
  test_sub_flags();
  test_inc_flags_and_preservation();
  test_arithmetic_preserves_interrupt_and_transfer_flags();
  test_invalid_instruction_does_not_change_cpu();
  test_instruction_encoding();
  test_instruction_decoding();
  test_instruction_encode_decode_roundtrip();
  printf("All tests passed!\n");
  return 0;
}
