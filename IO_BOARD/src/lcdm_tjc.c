#include "lcdm_tjc.h"

#include "at32f45x_board.h"
#include "at32f45x.h"

#include <stdio.h>
#include <string.h>

#ifndef LCDM_TJC_GPIO
#define LCDM_TJC_GPIO GPIOB
#endif

#ifndef LCDM_TJC_GPIO_CLOCK
#define LCDM_TJC_GPIO_CLOCK CRM_GPIOB_PERIPH_CLOCK
#endif

#ifndef LCDM_TJC_TX_PIN
#define LCDM_TJC_TX_PIN GPIO_PINS_3
#endif

#ifndef LCDM_TJC_RX_PIN
#define LCDM_TJC_RX_PIN GPIO_PINS_5
#endif

#ifndef LCDM_TJC_RESET_PIN
#define LCDM_TJC_RESET_PIN GPIO_PINS_4
#endif

#ifndef LCDM_TJC_USE_RESET_PIN
#define LCDM_TJC_USE_RESET_PIN 0
#endif

volatile uint32_t g_lcdm_tjc_tx_cmd_count;
volatile uint32_t g_lcdm_tjc_rx_byte_count;
volatile uint32_t g_lcdm_tjc_rx_packet_count;
volatile uint32_t g_lcdm_tjc_rx_overflow_count;
volatile uint8_t g_lcdm_tjc_last_packet0;
volatile uint8_t g_lcdm_tjc_last_packet_len;

static uint8_t rx_packet[LCDM_TJC_RX_PACKET_MAX];
static uint8_t rx_len;
static uint8_t rx_ff_count;
static uint32_t bit_us;

static void lcdm_tjc_wait_bit(void)
{
  delay_us(bit_us);
}

static void lcdm_tjc_tx_level(confirm_state level)
{
  gpio_bits_write(LCDM_TJC_GPIO, LCDM_TJC_TX_PIN, level);
}

static uint8_t lcdm_tjc_rx_level(void)
{
  return gpio_input_data_bit_read(LCDM_TJC_GPIO, LCDM_TJC_RX_PIN) != RESET;
}

static uint8_t lcdm_tjc_wait_for_start(uint32_t max_wait_us)
{
  uint32_t waited_us = 0U;

  while(lcdm_tjc_rx_level() != 0U) {
    if(waited_us >= max_wait_us) {
      return 0U;
    }
    delay_us(1U);
    waited_us++;
  }

  return 1U;
}

static void lcdm_tjc_write_byte(uint8_t value)
{
  uint8_t i;

  lcdm_tjc_tx_level(FALSE);
  lcdm_tjc_wait_bit();

  for(i = 0U; i < 8U; i++) {
    lcdm_tjc_tx_level((value & (1U << i)) ? TRUE : FALSE);
    lcdm_tjc_wait_bit();
  }

  lcdm_tjc_tx_level(TRUE);
  lcdm_tjc_wait_bit();
}

static uint8_t lcdm_tjc_read_byte(uint8_t *out_value)
{
  uint8_t i;
  uint8_t value = 0U;

  if(out_value == 0 || lcdm_tjc_rx_level() != 0U) {
    return 0U;
  }

  delay_us(bit_us + (bit_us / 2U));
  for(i = 0U; i < 8U; i++) {
    if(lcdm_tjc_rx_level() != 0U) {
      value |= (uint8_t)(1U << i);
    }
    lcdm_tjc_wait_bit();
  }

  if(lcdm_tjc_rx_level() == 0U) {
    return 0U;
  }

  *out_value = value;
  return 1U;
}

static void lcdm_tjc_write_bytes(const uint8_t *data, uint16_t len)
{
  uint16_t i;

  if(data == 0 || len == 0U) {
    return;
  }

  for(i = 0U; i < len; i++) {
    lcdm_tjc_write_byte(data[i]);
  }
}

void lcdm_tjc_init(void)
{
  gpio_init_type gpio_init_struct;

  bit_us = (1000000UL + (LCDM_TJC_BAUDRATE / 2UL)) / LCDM_TJC_BAUDRATE;
  if(bit_us == 0UL) {
    bit_us = 1UL;
  }

  crm_periph_clock_enable(LCDM_TJC_GPIO_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = LCDM_TJC_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_bits_set(LCDM_TJC_GPIO, LCDM_TJC_TX_PIN);
  gpio_init(LCDM_TJC_GPIO, &gpio_init_struct);

#if LCDM_TJC_USE_RESET_PIN
  gpio_init_struct.gpio_pins = LCDM_TJC_RESET_PIN;
  gpio_bits_set(LCDM_TJC_GPIO, LCDM_TJC_RESET_PIN);
  gpio_init(LCDM_TJC_GPIO, &gpio_init_struct);
#endif

  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = LCDM_TJC_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(LCDM_TJC_GPIO, &gpio_init_struct);

  rx_len = 0U;
  rx_ff_count = 0U;
}

void lcdm_tjc_send_cmd(const char *cmd)
{
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};

  if(cmd == 0) {
    return;
  }

  lcdm_tjc_write_bytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  lcdm_tjc_write_bytes(end_bytes, sizeof(end_bytes));
  g_lcdm_tjc_tx_cmd_count++;
}

void lcdm_tjc_set_text(const char *obj, const char *text)
{
  char cmd[160];

  if(obj == 0 || text == 0) {
    return;
  }

  (void)snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, text);
  lcdm_tjc_send_cmd(cmd);
}

void lcdm_tjc_set_num(const char *obj, int32_t value)
{
  char cmd[48];

  if(obj == 0) {
    return;
  }

  (void)snprintf(cmd, sizeof(cmd), "%s.val=%ld", obj, (long)value);
  lcdm_tjc_send_cmd(cmd);
}

void lcdm_tjc_page(uint8_t page_id)
{
  char cmd[16];

  (void)snprintf(cmd, sizeof(cmd), "page %u", (unsigned int)page_id);
  lcdm_tjc_send_cmd(cmd);
}

static void poll_rx_bytes(void)
{
  uint8_t value;
  uint8_t retry;

  for(retry = 0U; retry < 8U && rx_ff_count < 3U; retry++) {
    if(lcdm_tjc_wait_for_start((rx_len != 0U || rx_ff_count != 0U) ? (bit_us * 2U) : 0U) == 0U) {
      break;
    }
    if(lcdm_tjc_read_byte(&value) == 0U) {
      break;
    }
    g_lcdm_tjc_rx_byte_count++;

    if(value == 0xFFU) {
      if(rx_ff_count < 3U) {
        rx_ff_count++;
      }
      continue;
    }

    if(rx_ff_count != 0U) {
      rx_ff_count = 0U;
    }

    if(rx_len >= LCDM_TJC_RX_PACKET_MAX) {
      rx_len = 0U;
      g_lcdm_tjc_rx_overflow_count++;
    }

    rx_packet[rx_len] = value;
    rx_len++;
  }
}

uint8_t lcdm_tjc_poll_event(lcdm_tjc_event_t *event)
{
  uint8_t i;
  uint8_t len;

  if(event == 0) {
    return 0U;
  }

  event->type = LCDM_TJC_EVENT_NONE;
  poll_rx_bytes();

  if(rx_ff_count < 3U) {
    return 0U;
  }

  len = rx_len;
  rx_len = 0U;
  rx_ff_count = 0U;

  g_lcdm_tjc_rx_packet_count++;
  g_lcdm_tjc_last_packet_len = len;
  g_lcdm_tjc_last_packet0 = (len > 0U) ? rx_packet[0] : 0U;

  if(len >= 4U && rx_packet[0] == 0x65U) {
    event->type = LCDM_TJC_EVENT_TOUCH;
    event->page_id = rx_packet[1];
    event->component_id = rx_packet[2];
    event->touch_event = rx_packet[3];
    event->len = 0U;
    event->ascii[0] = '\0';
    return 1U;
  }

  if(len == 0U) {
    return 0U;
  }

  if(len >= LCDM_TJC_RX_PACKET_MAX) {
    len = (uint8_t)(LCDM_TJC_RX_PACKET_MAX - 1U);
  }

  for(i = 0U; i < len; i++) {
    event->ascii[i] = (char)rx_packet[i];
  }
  event->ascii[len] = '\0';
  event->len = len;
  event->type = LCDM_TJC_EVENT_ASCII;
  return 1U;
}
