#ifndef TEST_PHASE7_H
#define TEST_PHASE7_H

void test_phase7_instruction_encoding(void);
void test_phase7_instruction_decoding_and_roundtrip(void);
void test_push_pop_execution_and_stack_bounds(void);
void test_call_ret_execution_and_stack_bounds(void);
void test_phase7_machine_code_nested_subroutines(void);
void test_phase7_failure_semantics(void);

#endif
