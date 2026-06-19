#ifndef FIRST_GEN_4051_SCAN_H
#define FIRST_GEN_4051_SCAN_H

#include <stdint.h>

#define FIRST_GEN_4051_POINT_COUNT 96U

extern volatile uint16_t g_first_gen_last_adc1;
extern volatile uint16_t g_first_gen_last_adc2;
extern volatile uint16_t g_first_gen_adc_threshold;
extern volatile uint16_t g_first_gen_dac_code;
extern volatile uint16_t g_first_gen_first_fail_out;
extern volatile uint16_t g_first_gen_first_fail_in;
extern volatile uint32_t g_first_gen_scan_counter;
extern volatile uint32_t g_first_gen_missing_counter;
extern volatile uint32_t g_first_gen_unexpected_counter;
extern volatile uint8_t g_first_gen_last_pass;

void first_gen_4051_scan_init(void);
void first_gen_4051_scan_service(void);
uint8_t first_gen_4051_scan_once(void);

#endif
