#include <stdio.h>

#include "test_phase1.h"
#include "test_phase2.h"
#include "test_phase3.h"
#include "test_phase4.h"
#include "test_phase5.h"
#include "test_phase6.h"
#include "test_phase7.h"
#include "test_phase8.h"
#include "test_phase9.h"

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
  test_machine_code_execution();
  test_machine_code_execution_rejects_unknown_word();
  test_program_loader_bounds();
  test_sram_api();
  test_ld_st_instruction_encoding();
  test_ld_st_instruction_decoding_and_roundtrip();
  test_ld_st_direct_execution();
  test_ld_st_failure_semantics();
  test_phase3_machine_code_integration();
  test_gpio_io_api();
  test_gpio_pin_semantics();
  test_in_out_instruction_encoding();
  test_in_out_instruction_decoding_and_roundtrip();
  test_in_out_direct_execution();
  test_phase4_machine_code_integration();
  test_phase5_instruction_encoding();
  test_phase5_instruction_decoding_and_roundtrip();
  test_dec_flags_and_preservation();
  test_relative_control_flow_execution();
  test_phase5_machine_code_loop();
  test_phase5_failure_semantics();
  test_phase6_instruction_encoding();
  test_phase6_instruction_decoding_and_roundtrip();
  test_logical_instruction_flags();
  test_compare_instruction_flags_and_preservation();
  test_sbi_cbi_gpio_execution();
  test_phase6_machine_code_integration();
  test_phase6_failure_semantics();
  test_phase7_instruction_encoding();
  test_phase7_instruction_decoding_and_roundtrip();
  test_push_pop_execution_and_stack_bounds();
  test_call_ret_execution_and_stack_bounds();
  test_phase7_machine_code_nested_subroutines();
  test_phase7_failure_semantics();
  test_intel_hex_loader_matches_direct_program();
  test_intel_hex_loader_preserves_unwritten_flash();
  test_intel_hex_loader_failure_semantics();
  test_cycle_counter_increments_on_success_only();
  test_breakpoint_api();
  test_run_stops_on_cycle_limit();
  test_run_stops_on_breakpoint();
  test_run_stops_on_invalid_instruction();
  test_run_equivalent_to_repeated_step();
  printf("All tests passed!\n");
  return 0;
}
