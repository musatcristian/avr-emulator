#ifndef TEST_PHASE6_H
#define TEST_PHASE6_H

void test_phase6_instruction_encoding(void);
void test_phase6_instruction_decoding_and_roundtrip(void);
void test_logical_instruction_flags(void);
void test_compare_instruction_flags_and_preservation(void);
void test_sbi_cbi_gpio_execution(void);
void test_phase6_machine_code_integration(void);
void test_phase6_failure_semantics(void);

#endif
