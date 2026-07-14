#include "lcdm_tjc.h"

#include "at32f45x.h"
#include "at32f45x_board.h"

#include <stdio.h>
#include <string.h>

#ifndef LCDM_TJC_USART
#define LCDM_TJC_USART USART1
#endif

#ifndef LCDM_TJC_USART_CLOCK
#define LCDM_TJC_USART_CLOCK CRM_USART1_PERIPH_CLOCK
#endif

#ifndef LCDM_TJC_GPIO
#define LCDM_TJC_GPIO GPIOA
#endif

#ifndef LCDM_TJC_GPIO_CLOCK
#define LCDM_TJC_GPIO_CLOCK CRM_GPIOA_PERIPH_CLOCK
#endif

#ifndef LCDM_TJC_TX_PIN
#define LCDM_TJC_TX_PIN GPIO_PINS_9
#endif

#ifndef LCDM_TJC_RX_PIN
#define LCDM_TJC_RX_PIN GPIO_PINS_10
#endif

#ifndef LCDM_TJC_TX_PIN_SOURCE
#define LCDM_TJC_TX_PIN_SOURCE GPIO_PINS_SOURCE9
#endif

#ifndef LCDM_TJC_RX_PIN_SOURCE
#define LCDM_TJC_RX_PIN_SOURCE GPIO_PINS_SOURCE10
#endif

#ifndef LCDM_TJC_GPIO_MUX
#define LCDM_TJC_GPIO_MUX GPIO_MUX_7
#endif

#ifndef LCDM_TJC_CMD_GAP_MS
#define LCDM_TJC_CMD_GAP_MS 2U
#endif

#ifndef LCDM_TJC_POWER_ON_WAIT_MS
#define LCDM_TJC_POWER_ON_WAIT_MS 500U
#endif

#ifndef LCDM_TJC_PROBE_ATTEMPTS
#define LCDM_TJC_PROBE_ATTEMPTS 5U
#endif

#ifndef LCDM_TJC_PROBE_WAIT_MS
#define LCDM_TJC_PROBE_WAIT_MS 80U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD1
#define LCDM_TJC_RECOVERY_BAUD1 9600U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD2
#define LCDM_TJC_RECOVERY_BAUD2 38400U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD3
#define LCDM_TJC_RECOVERY_BAUD3 115200U
#endif

volatile uint32_t g_lcdm_tjc_tx_cmd_count;
volatile uint32_t g_lcdm_tjc_tx_error_count;
volatile uint32_t g_lcdm_tjc_rx_byte_count;
volatile uint32_t g_lcdm_tjc_rx_packet_count;
volatile uint32_t g_lcdm_tjc_rx_overflow_count;
volatile uint32_t g_lcdm_tjc_rx_queue_overflow_count;
volatile uint32_t g_lcdm_tjc_uart_error_count;
volatile uint8_t g_lcdm_tjc_last_packet_len;
volatile uint8_t g_lcdm_tjc_last_packet0;
volatile uint8_t g_lcdm_tjc_last_packet1;
volatile uint8_t g_lcdm_tjc_last_packet2;
volatile uint8_t g_lcdm_tjc_last_event_type;
volatile uint16_t g_lcdm_tjc_last_x;
volatile uint16_t g_lcdm_tjc_last_y;
volatile uint8_t g_lcdm_tjc_last_touch_event;
volatile uint8_t g_lcdm_tjc_last_page_id;
volatile uint8_t g_lcdm_tjc_last_component_id;
volatile uint32_t g_lcdm_tjc_last_number;
volatile uint8_t g_lcdm_tjc_last_raw0;
volatile uint8_t g_lcdm_tjc_last_raw1;
volatile uint8_t g_lcdm_tjc_last_raw2;
volatile uint8_t g_lcdm_tjc_last_raw3;
volatile uint8_t g_lcdm_tjc_last_raw4;
volatile uint8_t g_lcdm_tjc_last_raw5;
volatile uint8_t g_lcdm_tjc_last_raw6;
volatile uint8_t g_lcdm_tjc_last_raw7;
volatile uint8_t g_lcdm_tjc_debug_page;

static uint8_t rx_packet[LCDM_TJC_RX_PACKET_MAX];
static uint8_t rx_len;
static uint8_t rx_ff_count;
static uint8_t ready_packet[LCDM_TJC_RX_QUEUE_DEPTH][LCDM_TJC_RX_PACKET_MAX];
static uint8_t ready_len[LCDM_TJC_RX_QUEUE_DEPTH];
static volatile uint8_t ready_head;
static volatile uint8_t ready_tail;
static volatile uint8_t ready_count;
static uint8_t current_page;
static uint8_t refresh_requested;
static uint32_t active_baudrate;

static void poll_rx_bytes(void);

static void rx_reset(void)
{
  rx_len = 0U;
  rx_ff_count = 0U;
}

static void lcdm_tjc_delay_ms(uint32_t duration_ms)
{
  while(duration_ms != 0U) {
    uint32_t chunk_ms = (duration_ms > 100U) ? 100U : duration_ms;
    delay_ms(chunk_ms);
    duration_ms -= chunk_ms;
  }
}

static void lcdm_tjc_usart_config(uint32_t baudrate)
{
  gpio_init_type gpio_init_struct;

  if(baudrate == 0U) {
    baudrate = LCDM_TJC_BAUDRATE;
  }

  crm_periph_clock_enable(LCDM_TJC_GPIO_CLOCK, TRUE);
  crm_periph_clock_enable(LCDM_TJC_USART_CLOCK, TRUE);

  usart_enable(LCDM_TJC_USART, FALSE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = LCDM_TJC_TX_PIN | LCDM_TJC_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(LCDM_TJC_GPIO, &gpio_init_struct);

  gpio_pin_mux_config(LCDM_TJC_GPIO, LCDM_TJC_TX_PIN_SOURCE, LCDM_TJC_GPIO_MUX);
  gpio_pin_mux_config(LCDM_TJC_GPIO, LCDM_TJC_RX_PIN_SOURCE, LCDM_TJC_GPIO_MUX);

  usart_init(LCDM_TJC_USART, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(LCDM_TJC_USART, USART_PARITY_NONE);
  usart_hardware_flow_control_set(LCDM_TJC_USART, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(LCDM_TJC_USART, TRUE);
  usart_receiver_enable(LCDM_TJC_USART, TRUE);
  usart_interrupt_enable(LCDM_TJC_USART, USART_RDBF_INT, TRUE);
  usart_interrupt_enable(LCDM_TJC_USART, USART_ERR_INT, TRUE);
  usart_enable(LCDM_TJC_USART, TRUE);
  nvic_irq_enable(USART1_IRQn, 1U, 0U);
  active_baudrate = baudrate;
}

static void send_byte(uint8_t value)
{
  uint32_t timeout = 100000U;

  while(usart_flag_get(LCDM_TJC_USART, USART_TDBE_FLAG) == RESET) {
    if(timeout == 0U) {
      g_lcdm_tjc_tx_error_count++;
      return;
    }
    timeout--;
  }
  usart_data_transmit(LCDM_TJC_USART, value);
}

static void send_end(void)
{
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};
  uint8_t i;
  uint32_t timeout;

  for(i = 0U; i < sizeof(end_bytes); i++) {
    send_byte(end_bytes[i]);
  }

  timeout = 100000U;
  while(usart_flag_get(LCDM_TJC_USART, USART_TDC_FLAG) == RESET) {
    if(timeout == 0U) {
      g_lcdm_tjc_tx_error_count++;
      return;
    }
    timeout--;
  }
}

static void send_cmd(const char *cmd)
{
  const uint8_t *bytes = (const uint8_t *)cmd;
  uint8_t gap_ms;

  if(cmd == 0) {
    return;
  }

  while(*bytes != 0U) {
    send_byte(*bytes);
    bytes++;
  }
  g_lcdm_tjc_tx_cmd_count++;
  send_end();

  for(gap_ms = 0U; gap_ms < LCDM_TJC_CMD_GAP_MS; gap_ms++) {
    poll_rx_bytes();
    lcdm_tjc_delay_ms(1U);
  }
  poll_rx_bytes();
}

static void process_packet(const uint8_t *packet, uint8_t len)
{
  uint8_t i;

  if(len == 0U) {
    return;
  }

  g_lcdm_tjc_rx_packet_count++;
  g_lcdm_tjc_last_packet_len = len;
  g_lcdm_tjc_last_packet0 = packet[0];
  g_lcdm_tjc_last_packet1 = (len > 1U) ? packet[1] : 0U;
  g_lcdm_tjc_last_packet2 = (len > 2U) ? packet[2] : 0U;
  g_lcdm_tjc_last_event_type = packet[0];

  if((len >= 4U) && (packet[0] == 0x65U)) {
    g_lcdm_tjc_last_page_id = packet[1];
    g_lcdm_tjc_last_component_id = packet[2];
    g_lcdm_tjc_last_touch_event = packet[3];
    current_page = packet[1];
    g_lcdm_tjc_debug_page = current_page;
    return;
  }

  if((len >= 6U) && (packet[0] == 0x67U)) {
    g_lcdm_tjc_last_x = (uint16_t)(((uint16_t)packet[1] << 8) | packet[2]);
    g_lcdm_tjc_last_y = (uint16_t)(((uint16_t)packet[3] << 8) | packet[4]);
    g_lcdm_tjc_last_touch_event = packet[5];
    return;
  }

  if((len >= 5U) && (packet[0] == 0x71U)) {
    g_lcdm_tjc_last_number = ((uint32_t)packet[1]) |
                             ((uint32_t)packet[2] << 8) |
                             ((uint32_t)packet[3] << 16) |
                             ((uint32_t)packet[4] << 24);
    return;
  }

  if((len >= 6U) && (memcmp(packet, "page=", 5U) == 0)) {
    current_page = (uint8_t)(packet[5] - (uint8_t)'0');
    g_lcdm_tjc_debug_page = current_page;
    refresh_requested = 1U;
    return;
  }

  if((len >= 7U) && (memcmp(packet, "refresh", 7U) == 0)) {
    refresh_requested = 1U;
  }

  for(i = 0U; i < len; i++) {
    if((packet[i] < 0x20U) || (packet[i] > 0x7EU)) {
      return;
    }
  }
}

static void enqueue_ready_packet(const uint8_t *packet, uint8_t len)
{
  uint8_t slot;

  if((packet == 0) || (len == 0U)) {
    return;
  }

  if(len > LCDM_TJC_RX_PACKET_MAX) {
    len = LCDM_TJC_RX_PACKET_MAX;
  }

  if(ready_count >= LCDM_TJC_RX_QUEUE_DEPTH) {
    ready_tail = (uint8_t)((ready_tail + 1U) % LCDM_TJC_RX_QUEUE_DEPTH);
    ready_count--;
    g_lcdm_tjc_rx_queue_overflow_count++;
  }

  slot = ready_head;
  memcpy(ready_packet[slot], packet, len);
  ready_len[slot] = len;
  ready_head = (uint8_t)((ready_head + 1U) % LCDM_TJC_RX_QUEUE_DEPTH);
  ready_count++;
}

static void store_raw_debug(uint8_t value)
{
  g_lcdm_tjc_last_raw7 = g_lcdm_tjc_last_raw6;
  g_lcdm_tjc_last_raw6 = g_lcdm_tjc_last_raw5;
  g_lcdm_tjc_last_raw5 = g_lcdm_tjc_last_raw4;
  g_lcdm_tjc_last_raw4 = g_lcdm_tjc_last_raw3;
  g_lcdm_tjc_last_raw3 = g_lcdm_tjc_last_raw2;
  g_lcdm_tjc_last_raw2 = g_lcdm_tjc_last_raw1;
  g_lcdm_tjc_last_raw1 = g_lcdm_tjc_last_raw0;
  g_lcdm_tjc_last_raw0 = value;
}

static void store_rx_byte(uint8_t value)
{
  g_lcdm_tjc_rx_byte_count++;
  store_raw_debug(value);

  if(value == 0xFFU) {
    rx_ff_count++;
    if(rx_ff_count >= 3U) {
      process_packet(rx_packet, rx_len);
      if(rx_len != 0U) {
        enqueue_ready_packet(rx_packet, rx_len);
      }
      rx_reset();
    }
    return;
  }

  rx_ff_count = 0U;
  if(rx_len < sizeof(rx_packet)) {
    rx_packet[rx_len] = value;
    rx_len++;
  } else {
    rx_reset();
    g_lcdm_tjc_rx_overflow_count++;
  }
}

static void poll_rx_bytes(void)
{
  uint16_t guard = 256U;

  while(guard != 0U) {
    uint8_t has_data = (usart_flag_get(LCDM_TJC_USART, USART_RDBF_FLAG) != RESET) ? 1U : 0U;
    uint8_t has_error =
      ((usart_flag_get(LCDM_TJC_USART, USART_PERR_FLAG) != RESET) ||
       (usart_flag_get(LCDM_TJC_USART, USART_FERR_FLAG) != RESET) ||
       (usart_flag_get(LCDM_TJC_USART, USART_NERR_FLAG) != RESET) ||
       (usart_flag_get(LCDM_TJC_USART, USART_ROERR_FLAG) != RESET)) ? 1U : 0U;

    if((has_data == 0U) && (has_error == 0U)) {
      break;
    }

    if(has_error != 0U) {
      g_lcdm_tjc_uart_error_count++;
    }

    if(has_data != 0U) {
      volatile uint32_t status = LCDM_TJC_USART->sts;
      uint8_t value = (uint8_t)LCDM_TJC_USART->dt;
      (void)status;
      store_rx_byte(value);
    } else if(has_error != 0U) {
      usart_flag_clear(LCDM_TJC_USART,
                       USART_PERR_FLAG | USART_FERR_FLAG | USART_NERR_FLAG | USART_ROERR_FLAG);
    }
    guard--;
  }
}

void lcdm_tjc_init(void)
{
  current_page = 0U;
  refresh_requested = 1U;
  ready_head = 0U;
  ready_tail = 0U;
  ready_count = 0U;
  rx_reset();

  lcdm_tjc_usart_config(LCDM_TJC_BAUDRATE);
  lcdm_tjc_delay_ms(LCDM_TJC_POWER_ON_WAIT_MS);
}

void lcdm_tjc_set_baudrate(uint32_t baudrate)
{
  ready_head = 0U;
  ready_tail = 0U;
  ready_count = 0U;
  rx_reset();
  lcdm_tjc_usart_config(baudrate);
}

static void lcdm_tjc_send_baud_cmds(uint32_t source_baudrate, uint32_t target_baudrate)
{
  char cmd[32];

  lcdm_tjc_set_baudrate(source_baudrate);
  send_cmd("bkcmd=0");
  (void)snprintf(cmd, sizeof(cmd), "bauds=%lu", (unsigned long)target_baudrate);
  send_cmd(cmd);
  (void)snprintf(cmd, sizeof(cmd), "baud=%lu", (unsigned long)target_baudrate);
  send_cmd(cmd);
  lcdm_tjc_delay_ms(50U);
}

static void lcdm_tjc_try_recovery_baud(uint32_t source_baudrate, uint32_t target_baudrate)
{
  if((source_baudrate == 0U) ||
     ((source_baudrate == target_baudrate) && (target_baudrate != LCDM_TJC_BAUDRATE)) ||
     (source_baudrate == active_baudrate)) {
    return;
  }
  lcdm_tjc_send_baud_cmds(source_baudrate, target_baudrate);
}

void lcdm_tjc_force_baudrate(uint32_t target_baudrate)
{
  if(target_baudrate == 0U) {
    target_baudrate = LCDM_TJC_BAUDRATE;
  }

  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD1, target_baudrate);
  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD2, target_baudrate);
  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD3, target_baudrate);
  lcdm_tjc_send_baud_cmds(target_baudrate, target_baudrate);
  lcdm_tjc_set_baudrate(target_baudrate);
  send_cmd("bkcmd=0");
  lcdm_tjc_delay_ms(80U);
}

uint8_t lcdm_tjc_probe(uint8_t attempts)
{
  uint8_t attempt;

  if(attempts == 0U) {
    attempts = LCDM_TJC_PROBE_ATTEMPTS;
  }

  lcdm_tjc_init();
  for(attempt = 0U; attempt < attempts; attempt++) {
    uint8_t wait_step;
    send_cmd("connect");
    for(wait_step = 0U; wait_step < LCDM_TJC_PROBE_WAIT_MS; wait_step++) {
      lcdm_tjc_event_t event;
      if(lcdm_tjc_poll_event(&event) != 0U) {
        return 1U;
      }
      lcdm_tjc_delay_ms(1U);
    }
  }

  return 0U;
}

void lcdm_tjc_rx_falling_edge_isr(void)
{
}

void lcdm_tjc_usart_irq_handler(void)
{
  uint16_t guard = 32U;

  while(guard != 0U) {
    uint32_t status = LCDM_TJC_USART->sts;
    uint8_t has_data = ((status & USART_RDBF_FLAG) != 0U) ? 1U : 0U;
    uint8_t has_error =
      ((status & (USART_PERR_FLAG | USART_FERR_FLAG | USART_NERR_FLAG | USART_ROERR_FLAG)) != 0U) ? 1U : 0U;

    if((has_data == 0U) && (has_error == 0U)) {
      break;
    }

    if(has_error != 0U) {
      g_lcdm_tjc_uart_error_count++;
    }

    if(has_data != 0U) {
      store_rx_byte((uint8_t)LCDM_TJC_USART->dt);
    } else {
      volatile uint32_t dummy = LCDM_TJC_USART->dt;
      (void)dummy;
    }
    guard--;
  }
}

void lcdm_tjc_send_cmd(const char *cmd)
{
  send_cmd(cmd);
}

void lcdm_tjc_set_text(const char *obj, const char *text)
{
  char cmd[160];

  if(obj == 0 || text == 0) {
    return;
  }

  (void)snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, text);
  send_cmd(cmd);
}

void lcdm_tjc_set_num(const char *obj, int32_t value)
{
  char cmd[48];

  if(obj == 0) {
    return;
  }

  (void)snprintf(cmd, sizeof(cmd), "%s.val=%ld", obj, (long)value);
  send_cmd(cmd);
}

void lcdm_tjc_page(uint8_t page_id)
{
  char cmd[16];

  current_page = page_id;
  g_lcdm_tjc_debug_page = current_page;
  refresh_requested = 1U;
  (void)snprintf(cmd, sizeof(cmd), "page %u", (unsigned int)page_id);
  send_cmd(cmd);
}

uint8_t lcdm_tjc_poll_event(lcdm_tjc_event_t *event)
{
  uint8_t i;
  uint8_t len;
  uint8_t packet[LCDM_TJC_RX_PACKET_MAX];
  uint8_t slot;

  if(event == 0) {
    return 0U;
  }

  event->type = LCDM_TJC_EVENT_NONE;
  event->number = 0U;
  poll_rx_bytes();

  if(ready_count == 0U) {
    return 0U;
  }

  __disable_irq();
  if(ready_count == 0U) {
    __enable_irq();
    return 0U;
  }
  slot = ready_tail;
  len = ready_len[slot];
  if(len > LCDM_TJC_RX_PACKET_MAX) {
    len = LCDM_TJC_RX_PACKET_MAX;
  }
  memcpy(packet, ready_packet[slot], len);

  ready_len[slot] = 0U;
  ready_tail = (uint8_t)((ready_tail + 1U) % LCDM_TJC_RX_QUEUE_DEPTH);
  ready_count--;
  __enable_irq();

  if((len >= 4U) && (packet[0] == 0x65U)) {
    event->type = LCDM_TJC_EVENT_TOUCH;
    event->page_id = packet[1];
    event->component_id = packet[2];
    event->touch_event = packet[3];
    event->x = 0U;
    event->y = 0U;
    event->number = 0U;
    event->len = 0U;
    event->ascii[0] = '\0';
    return 1U;
  }

  if((len >= 6U) && (packet[0] == 0x67U)) {
    event->type = LCDM_TJC_EVENT_TOUCH_COORD;
    event->page_id = current_page;
    event->component_id = 0U;
    event->touch_event = packet[5];
    event->x = (uint16_t)(((uint16_t)packet[1] << 8) | packet[2]);
    event->y = (uint16_t)(((uint16_t)packet[3] << 8) | packet[4]);
    event->number = 0U;
    event->len = 0U;
    event->ascii[0] = '\0';
    return 1U;
  }

  if((len >= 5U) && (packet[0] == 0x71U)) {
    event->type = LCDM_TJC_EVENT_NUMBER;
    event->page_id = current_page;
    event->component_id = 0U;
    event->touch_event = 0U;
    event->x = 0U;
    event->y = 0U;
    event->number = ((uint32_t)packet[1]) |
                    ((uint32_t)packet[2] << 8) |
                    ((uint32_t)packet[3] << 16) |
                    ((uint32_t)packet[4] << 24);
    event->len = 0U;
    event->ascii[0] = '\0';
    return 1U;
  }

  if(len >= LCDM_TJC_RX_PACKET_MAX) {
    len = (uint8_t)(LCDM_TJC_RX_PACKET_MAX - 1U);
  }

  for(i = 0U; i < len; i++) {
    event->ascii[i] = (char)packet[i];
  }
  event->ascii[len] = '\0';
  event->len = len;
  event->type = LCDM_TJC_EVENT_ASCII;
  return 1U;
}
