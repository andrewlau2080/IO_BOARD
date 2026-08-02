#include "tester_wifi_link_diag.h"

#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "first_gen_display.h"
#include "tester_wifi_print.h"

#include <stdio.h>

#define WIFI_LINK_DIAG_STEP_MS           1U
#define WIFI_LINK_DIAG_REPLY_TIMEOUT_MS 1000U
#define WIFI_LINK_DIAG_RETRY_MS          1500U

static uint32_t wifi_link_sequence;
static uint16_t wifi_link_wait_ms;
static uint16_t wifi_link_retry_ms;
static uint8_t wifi_link_waiting;

void tester_wifi_clock_config(void)
{
  uint32_t timeout;

  /* PB9 receives the ESP32-C3's 115200-baud boot/AT stream.  At the normal
   * 8 MHz HICK, one bit is only 69 CPU cycles and the edge-buffer receiver
   * can be delayed by LCDM transfers.  All WiFi software-UART images use the
   * proven internal-HICK PLL route, not the unproven external crystal. */
  crm_reset();
  flash_psr_set(FLASH_WAIT_CYCLE_5);
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, TRUE);
  pwc_ldo_output_voltage_set(PWC_LDO_OUTPUT_1V3);

  crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);
  timeout = 0x10000U;
  while(crm_flag_get(CRM_HICK_STABLE_FLAG) != SET && timeout != 0U) {
    timeout--;
  }
  if(timeout == 0U) {
    system_core_clock_update();
    return;
  }

  crm_pll_config(CRM_PLL_SOURCE_HICK, 96U, 1U, CRM_PLL_FP_4);
  crm_pllu_div_set(CRM_PLL_FU_16);
  crm_pllu_output_set(TRUE);
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  timeout = 0x10000U;
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) != SET && timeout != 0U) {
    timeout--;
  }
  if(timeout == 0U) {
    system_core_clock_update();
    return;
  }

  crm_ahb_div_set(CRM_AHB_DIV_1);
  crm_apb3_div_set(CRM_APB3_DIV_4);
  crm_apb2_div_set(CRM_APB2_DIV_1);
  crm_apb1_div_set(CRM_APB1_DIV_1);
  crm_auto_step_mode_enable(TRUE);
  crm_sysclk_switch(CRM_SCLK_PLL);

  timeout = 0x10000U;
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL && timeout != 0U) {
    timeout--;
  }
  crm_auto_step_mode_enable(FALSE);
  system_core_clock_update();
}

static void wifi_link_diag_draw(uint16_t status_color,
                                uint16_t tx_bg,
                                uint16_t tx_fg,
                                const char *tx_text,
                                const char *rx_text,
                                const char *detail)
{
  /* A distinct temporary page token makes the shared LCDM backend stop the
   * normal idle "WIRE TESTER" marquee once, without changing that marquee
   * for the regular tester firmware. */
  first_gen_display_show_page("WIFI-DIAG",
                              "WIFI LINK TEST",
                              tx_text,
                              rx_text,
                              detail,
                              status_color,
                              tx_bg,
                              tx_fg);
}

static void wifi_link_diag_start_next(void)
{
  char tx_text[32];

  wifi_link_sequence++;
  if(wifi_link_sequence == 0U) {
    wifi_link_sequence = 1U;
  }

  (void)snprintf(tx_text, sizeof(tx_text), "TX: AT #%lu",
                 (unsigned long)wifi_link_sequence);
  if(tester_wifi_print_link_test_request(wifi_link_sequence) == 0U) {
    wifi_link_waiting = 0U;
    wifi_link_retry_ms = WIFI_LINK_DIAG_RETRY_MS;
    wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_RED,
                        FIRST_GEN_DISPLAY_COLOR_WHITE,
                        FIRST_GEN_DISPLAY_COLOR_RED,
                        "TX: NOT READY",
                        "RX: --",
                        "CHECK PC3 / WIFI POWER");
    return;
  }

  wifi_link_waiting = 1U;
  wifi_link_wait_ms = 0U;
  wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_BLUE,
                      FIRST_GEN_DISPLAY_COLOR_PALE_CYAN,
                      FIRST_GEN_DISPLAY_COLOR_BLUE,
                      tx_text,
                      "RX: WAITING",
                      "TX/RX: 115.2K BAUD 8N1");
}

void tester_wifi_link_diag_init(void)
{
  first_gen_display_init();
  tester_wifi_print_init();

  wifi_link_sequence = 0U;
  wifi_link_wait_ms = 0U;
  wifi_link_retry_ms = 0U;
  wifi_link_waiting = 0U;
  wifi_link_diag_start_next();
}

void tester_wifi_link_diag_service(void)
{
  tester_wifi_print_event_t event;
  char tx_text[32];
  char rx_text[32];

  tester_wifi_print_service();
  /* Do not run first_gen_display_effect_step() in this temporary mode:
   * it advances the normal idle WIRE TESTER marquee and would compete with
   * the TX/RX diagnostic text. */

  if(wifi_link_waiting != 0U) {
    event = tester_wifi_print_poll_event(wifi_link_sequence);
    if(event == TESTER_WIFI_PRINT_EVENT_LINK_ACK) {
      (void)snprintf(tx_text, sizeof(tx_text), "TX: AT #%lu",
                     (unsigned long)wifi_link_sequence);
      (void)snprintf(rx_text, sizeof(rx_text), "RX: OK #%lu",
                     (unsigned long)wifi_link_sequence);
      wifi_link_waiting = 0U;
      wifi_link_retry_ms = WIFI_LINK_DIAG_RETRY_MS;
      wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_GREEN,
                          FIRST_GEN_DISPLAY_COLOR_GREEN,
                          FIRST_GEN_DISPLAY_COLOR_WHITE,
                          tx_text,
                          rx_text,
                          "ESP-AT / PC3-PB9 OK");
    } else if(event == TESTER_WIFI_PRINT_EVENT_LINK_ERROR) {
      (void)snprintf(tx_text, sizeof(tx_text), "TX: AT #%lu",
                     (unsigned long)wifi_link_sequence);
      wifi_link_waiting = 0U;
      wifi_link_retry_ms = WIFI_LINK_DIAG_RETRY_MS;
      wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_RED,
                          FIRST_GEN_DISPLAY_COLOR_PALE_PINK,
                          FIRST_GEN_DISPLAY_COLOR_RED,
                          tx_text,
                          "RX: AT ERROR",
                          "PB9 RX OK; CHECK ESP AT");
    } else if(event == TESTER_WIFI_PRINT_EVENT_LINK_FLASH_INVALID) {
      (void)snprintf(tx_text, sizeof(tx_text), "TX: AT #%lu",
                     (unsigned long)wifi_link_sequence);
      wifi_link_waiting = 0U;
      wifi_link_retry_ms = WIFI_LINK_DIAG_RETRY_MS;
      wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_RED,
                          FIRST_GEN_DISPLAY_COLOR_PALE_PINK,
                          FIRST_GEN_DISPLAY_COLOR_RED,
                          tx_text,
                          "RX: ESP FLASH EMPTY",
                          "INVALID HEADER: FFFFFFFF");
    } else {
      wifi_link_wait_ms = (uint16_t)(wifi_link_wait_ms + WIFI_LINK_DIAG_STEP_MS);
      if(wifi_link_wait_ms >= WIFI_LINK_DIAG_REPLY_TIMEOUT_MS) {
        (void)snprintf(tx_text, sizeof(tx_text), "TX: AT #%lu",
                       (unsigned long)wifi_link_sequence);
        wifi_link_waiting = 0U;
        wifi_link_retry_ms = WIFI_LINK_DIAG_RETRY_MS;
        wifi_link_diag_draw(FIRST_GEN_DISPLAY_COLOR_RED,
                            FIRST_GEN_DISPLAY_COLOR_PALE_PINK,
                            FIRST_GEN_DISPLAY_COLOR_RED,
                            tx_text,
                            "RX: TIMEOUT",
                            "CHECK ESP EN / BAUD / PB9");
      }
    }
  } else if(wifi_link_retry_ms <= WIFI_LINK_DIAG_STEP_MS) {
    wifi_link_diag_start_next();
  } else {
    wifi_link_retry_ms = (uint16_t)(wifi_link_retry_ms - WIFI_LINK_DIAG_STEP_MS);
  }

  delay_ms(WIFI_LINK_DIAG_STEP_MS);
}
