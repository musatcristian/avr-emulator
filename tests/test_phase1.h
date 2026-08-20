#ifndef TEST_PHASE1_H
#define TEST_PHASE1_H

void test_reset_clears_cpu(void);
void test_ldi_and_mov(void);
void test_add_flags(void);
void test_sub_flags(void);
void test_inc_flags_and_preservation(void);
void test_arithmetic_preserves_interrupt_and_transfer_flags(void);
void test_invalid_instruction_does_not_change_cpu(void);

#endif
