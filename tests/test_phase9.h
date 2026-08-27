#ifndef TEST_PHASE9_H
#define TEST_PHASE9_H

void test_cycle_counter_increments_on_success_only(void);
void test_breakpoint_api(void);
void test_run_stops_on_cycle_limit(void);
void test_run_stops_on_breakpoint(void);
void test_run_stops_on_invalid_instruction(void);
void test_run_equivalent_to_repeated_step(void);

#endif
