#include "lcdm_tjc.h"

#include "at32f45x.h"

#include <stdio.h>
#include <string.h>

volatile uint32_t g_lcdm_tjc_tx_cmd_count;
volatile uint32_t g_lcdm_tjc_rx_byte_count;
volatile uint32_t g_lcdm_tjc_rx_packet_count;
volatile uint32_t g_lcdm_tjc_rx_overflow_count;
volatile uint8_t g_lcdm_tjc_last_packet0;
volatile uint8_t g_lcdm_tjc_last_packet_len;

static uint8_t rx_packet[LCDM_TJC_RX_PACKET_MAX];
static uint8_t rx_len;
static uint8_t rx_ff_count;

static void lcdm_tjc_write_bytes(const uint8_t *data, uint16_t len)
{
  uint16_t i;

  if(data == 0 || len == 0U) {
    return;
  }

  for(i = 0U; i < len; i++) {
    while(usart_flag_get(USART1, USART_TDBE_FLAG) == RESET) {
    }
    usart_data_transmit(USART1, data[i]);
  }

  while(usart_flag_get(USART1, USART_TDC_FLAG) == RESET) {
  }
}

void lcdm_tjc_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_9 | GPIO_PINS_10;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(GPIOA, &gpio_init_struct);

  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE9, GPIO_MUX_7);
  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE10, GPIO_MUX_7);

  usart_init(USART1, LCDM_TJC_BAUDRATE, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(USART1, USART_PARITY_NONE);
  usart_hardware_flow_control_set(USART1, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(USART1, TRUE);
  usart_receiver_enable(USART1, TRUE);
  usart_enable(USART1, TRUE);

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

  while(usart_flag_get(USART1, USART_RDBF_FLAG) != RESET && rx_ff_count < 3U) {
    value = (uint8_t)usart_data_receive(USART1);
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
