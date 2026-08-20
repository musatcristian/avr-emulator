#ifndef TEST_PHASE2_H
#define TEST_PHASE2_H

void test_instruction_encoding(void);
void test_instruction_decoding(void);
void test_instruction_encode_decode_roundtrip(void);
void test_machine_code_execution(void);
void test_machine_code_execution_rejects_unknown_word(void);
void test_program_loader_bounds(void);

#endif
