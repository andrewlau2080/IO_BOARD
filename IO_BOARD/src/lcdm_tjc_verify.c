#include "lcdm_tjc_verify.h"

#include "at32f45x_board.h"
#include "lcdm_motor_ui.h"
#include "lcdm_tjc.h"

#include <stdio.h>

#ifndef LCDM_VERIFY_REFRESH_MS
#define LCDM_VERIFY_REFRESH_MS 100U
#endif

volatile uint32_t g_lcdm_verify_tx_cmd_count;
volatile uint32_t g_lcdm_verify_rx_byte_count;
volatile uint32_t g_lcdm_verify_rx_packet_count;
volatile uint32_t g_lcdm_verify_rx_overflow_count;
volatile uint8_t g_lcdm_verify_last_packet_len;
volatile uint8_t g_lcdm_verify_last_packet0;
volatile uint8_t g_lcdm_verify_last_packet1;
volatile uint8_t g_lcdm_verify_last_packet2;
volatile uint8_t g_lcdm_verify_page;
volatile uint8_t g_lcdm_verify_component;
volatile uint8_t g_lcdm_verify_touch_event;

static void sync_lcdm_debug_counters(void)
{
  g_lcdm_verify_tx_cmd_count = g_lcdm_tjc_tx_cmd_count;
  g_lcdm_verify_rx_byte_count = g_lcdm_tjc_rx_byte_count;
  g_lcdm_verify_rx_packet_count = g_lcdm_tjc_rx_packet_count;
  g_lcdm_verify_rx_overflow_count = g_lcdm_tjc_rx_overflow_count;
  g_lcdm_verify_last_packet_len = g_lcdm_tjc_last_packet_len;
  g_lcdm_verify_last_packet0 = g_lcdm_tjc_last_packet0;
  g_lcdm_verify_last_packet1 = g_lcdm_tjc_last_packet1;
  g_lcdm_verify_last_packet2 = g_lcdm_tjc_last_packet2;
  g_lcdm_verify_page = g_lcdm_motor_page;
  g_lcdm_verify_component = g_lcdm_motor_selected;
  g_lcdm_verify_touch_event = g_lcdm_motor_last_event;
}

void lcdm_tjc_verify_init(void)
{
  lcdm_motor_ui_init();
}

void lcdm_tjc_verify_service(void)
{
  lcdm_motor_ui_service();
  sync_lcdm_debug_counters();
}
