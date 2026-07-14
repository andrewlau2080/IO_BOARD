#ifndef LCDM_TJC_VERIFY_H
#define LCDM_TJC_VERIFY_H

#include <stdint.h>

extern volatile uint32_t g_lcdm_verify_tx_cmd_count;
extern volatile uint32_t g_lcdm_verify_rx_byte_count;
extern volatile uint32_t g_lcdm_verify_rx_packet_count;
extern volatile uint32_t g_lcdm_verify_rx_overflow_count;
extern volatile uint8_t g_lcdm_verify_last_packet_len;
extern volatile uint8_t g_lcdm_verify_last_packet0;
extern volatile uint8_t g_lcdm_verify_last_packet1;
extern volatile uint8_t g_lcdm_verify_last_packet2;
extern volatile uint8_t g_lcdm_verify_page;
extern volatile uint8_t g_lcdm_verify_component;
extern volatile uint8_t g_lcdm_verify_touch_event;

void lcdm_tjc_verify_init(void);
void lcdm_tjc_verify_service(void);

#endif
