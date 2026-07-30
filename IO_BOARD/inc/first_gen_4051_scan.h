#ifndef FIRST_GEN_4051_SCAN_H
#define FIRST_GEN_4051_SCAN_H

#include <stdint.h>

#define FIRST_GEN_4051_POINT_COUNT 96U

#define FIRST_GEN_PRINT_STATE_IDLE                 0U
#define FIRST_GEN_PRINT_STATE_WAIT_HALL            1U
#define FIRST_GEN_PRINT_STATE_WAIT_WIFI_ACK        2U
#define FIRST_GEN_PRINT_STATE_WAIT_WIFI_DONE       3U
#define FIRST_GEN_PRINT_STATE_WAIT_REMOVE          4U

extern volatile uint16_t g_first_gen_last_adc1;
extern volatile uint16_t g_first_gen_last_adc2;
extern volatile uint16_t g_first_gen_adc_threshold;
extern volatile uint16_t g_first_gen_dac_code;
extern volatile uint16_t g_first_gen_first_fail_out;
extern volatile uint16_t g_first_gen_first_fail_in;
extern volatile uint16_t g_first_gen_current_out;
extern volatile uint16_t g_first_gen_current_problem_in;
extern volatile uint32_t g_first_gen_scan_counter;
extern volatile uint32_t g_first_gen_missing_counter;
extern volatile uint32_t g_first_gen_unexpected_counter;
extern volatile uint32_t g_first_gen_learn_counter;
extern volatile uint32_t g_first_gen_print_request_counter;
extern volatile uint32_t g_first_gen_print_ack_counter;
extern volatile uint32_t g_first_gen_print_done_counter;
extern volatile uint32_t g_first_gen_print_error_counter;
extern volatile uint32_t g_first_gen_print_blocked_counter;
extern volatile uint8_t g_first_gen_recipe_valid;
extern volatile uint8_t g_first_gen_learn_status;
extern volatile uint8_t g_first_gen_last_pass;
extern volatile uint8_t g_first_gen_print_ready;
extern volatile uint8_t g_first_gen_print_waiting_for_wifi;
extern volatile uint8_t g_first_gen_panel_mode;
extern volatile uint8_t g_first_gen_last_panel_key;
extern volatile uint8_t g_first_gen_pass_hold_active;
extern volatile uint8_t g_first_gen_print_done;
extern volatile uint8_t g_first_gen_hall_active;
extern volatile uint8_t g_first_gen_print_state;
extern volatile uint32_t g_first_gen_last_connected_pairs;
extern volatile uint32_t g_first_gen_learn_connected_pairs;
extern volatile uint16_t g_first_gen_learn_out_count;
extern volatile uint16_t g_first_gen_learn_in_count;
extern volatile uint8_t g_first_gen_learn_pending;

void first_gen_4051_scan_init(void);
void first_gen_4051_scan_service(void);
uint8_t first_gen_4051_scan_once(void);
uint8_t first_gen_4051_learn_current_harness(void);

#endif
