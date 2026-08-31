#ifndef TEST_PHASE10_H
#define TEST_PHASE10_H

void test_snapshot_reflects_mcu_state(void);
void test_snapshot_is_read_only(void);
void test_snapshot_reports_invalid_instruction(void);
void test_format_instruction_examples(void);
void test_format_instruction_rejects_short_buffer(void);
void test_explain_instruction_examples(void);
void test_explain_instruction_rejects_short_buffer(void);
void test_flag_name_examples(void);
void test_step_with_events_reports_changes(void);
void test_step_with_events_failure_semantics(void);
void test_step_with_events_matches_plain_step(void);

#endif
