#include "tester_wifi_print.h"

#include "at32f45x.h"
#include "at32f45x_exint.h"
#include "at32f45x_scfg.h"

#include <stdio.h>
#include <string.h>

/* The 2026-07-07-N board connects the ESP32-C3 as a network coprocessor:
 *   PC3 / WIFI_TX -> ESP32 RXD0
 *   PB9 / WIFI_RX <- ESP32 TXD0
 * These pins deliberately remain GPIO software UART pins.  They are not
 * shared with the LCDM USART or with the legacy IR transceiver. */
#define TESTER_WIFI_TX_GPIO              GPIOC
#define TESTER_WIFI_TX_GPIO_CLOCK        CRM_GPIOC_PERIPH_CLOCK
#define TESTER_WIFI_TX_PIN               GPIO_PINS_3
#define TESTER_WIFI_RX_GPIO              GPIOB
#define TESTER_WIFI_RX_GPIO_CLOCK        CRM_GPIOB_PERIPH_CLOCK
#define TESTER_WIFI_RX_PIN               GPIO_PINS_9
#define TESTER_WIFI_RX_PORT_SOURCE       SCFG_PORT_SOURCE_GPIOB
#define TESTER_WIFI_RX_PIN_SOURCE        SCFG_PINS_SOURCE9
#define TESTER_WIFI_RX_EXINT_LINE        EXINT_LINE_9

#define TESTER_WIFI_RX_EDGE_MAX          512U
#define TESTER_WIFI_RX_EDGE_MASK         (TESTER_WIFI_RX_EDGE_MAX - 1U)
#define TESTER_WIFI_RX_LINE_MAX          128U
#define TESTER_WIFI_EVENT_QUEUE_MAX      4U

typedef struct {
  tester_wifi_print_event_t type;
  uint32_t event_id;
} tester_wifi_print_event_slot_t;

volatile uint32_t g_tester_wifi_print_tx_request_count;
volatile uint32_t g_tester_wifi_print_rx_frame_count;
volatile uint32_t g_tester_wifi_print_rx_error_count;
volatile uint32_t g_tester_wifi_print_rx_overflow_count;
volatile uint8_t g_tester_wifi_print_ready;

static uint32_t wifi_bit_cycles;
static volatile uint32_t wifi_rx_edge_cycles[TESTER_WIFI_RX_EDGE_MAX];
static volatile uint8_t wifi_rx_edge_levels[TESTER_WIFI_RX_EDGE_MAX];
static volatile uint32_t wifi_rx_edge_head;
static uint32_t wifi_rx_edge_tail;
static char wifi_rx_line[TESTER_WIFI_RX_LINE_MAX];
static uint8_t wifi_rx_line_len;
static tester_wifi_print_event_slot_t wifi_event_queue[TESTER_WIFI_EVENT_QUEUE_MAX];
static uint8_t wifi_event_head;
static uint8_t wifi_event_tail;

static uint32_t wifi_cycles(void)
{
  return DWT->CYCCNT;
}

static void wifi_wait_until_cycles(uint32_t target_cycles)
{
  while((int32_t)(wifi_cycles() - target_cycles) < 0) {
    __asm volatile("nop");
  }
}

static void wifi_tx_level(uint8_t level)
{
  if(level != 0U) {
    TESTER_WIFI_TX_GPIO->scr = TESTER_WIFI_TX_PIN;
  } else {
    TESTER_WIFI_TX_GPIO->clr = TESTER_WIFI_TX_PIN;
  }
}

static uint8_t wifi_rx_level(void)
{
  /* This runs in the software-UART edge ISR.  Do not call the generic GPIO
   * driver here: at the tester's HICK clock its function-call overhead can
   * consume an entire UART bit period. */
  return ((TESTER_WIFI_RX_GPIO->idt & TESTER_WIFI_RX_PIN) != 0U) ? 1U : 0U;
}

static void wifi_write_byte(uint8_t value)
{
  uint8_t bit;
  uint32_t target_cycles;
  uint32_t primask;

  target_cycles = wifi_cycles();
  primask = __get_PRIMASK();
  __disable_irq();

  wifi_tx_level(0U);
  target_cycles += wifi_bit_cycles;
  wifi_wait_until_cycles(target_cycles);

  for(bit = 0U; bit < 8U; bit++) {
    wifi_tx_level((value & (uint8_t)(1U << bit)) != 0U ? 1U : 0U);
    target_cycles += wifi_bit_cycles;
    wifi_wait_until_cycles(target_cycles);
  }

  wifi_tx_level(1U);
  target_cycles += wifi_bit_cycles;
  wifi_wait_until_cycles(target_cycles);

  if(primask == 0U) {
    __enable_irq();
  }
}

static void wifi_write_text(const char *text)
{
  if(text == 0) {
    return;
  }

  while(*text != '\0') {
    wifi_write_byte((uint8_t)*text);
    text++;
  }
}

static uint8_t wifi_parse_uint32(const char *text, uint32_t *out_value)
{
  uint32_t value = 0U;
  uint8_t digits = 0U;

  if(text == 0 || out_value == 0) {
    return 0U;
  }

  while(*text >= '0' && *text <= '9') {
    value = (value * 10U) + (uint32_t)(*text - '0');
    text++;
    digits++;
    if(digits > 10U) {
      return 0U;
    }
  }

  if(digits == 0U) {
    return 0U;
  }

  *out_value = value;
  return 1U;
}

static uint8_t wifi_frame_event_id(const char *frame, uint32_t *event_id)
{
  const char *value_start;

  if(frame == 0 || event_id == 0) {
    return 0U;
  }

  value_start = strstr(frame, "\"event_id\":");
  if(value_start != 0) {
    return wifi_parse_uint32(value_start + 11U, event_id);
  }

  /* Compact fallback accepted for production bring-up tools:
   * ACK,<event_id>,QUEUED / DONE,<event_id> / ERROR,<event_id>. */
  value_start = strchr(frame, ',');
  if(value_start == 0) {
    return 0U;
  }
  return wifi_parse_uint32(value_start + 1U, event_id);
}

static void wifi_queue_event(tester_wifi_print_event_t type, uint32_t event_id)
{
  uint8_t next_head;

  if(type == TESTER_WIFI_PRINT_EVENT_NONE) {
    return;
  }

  next_head = (uint8_t)((wifi_event_head + 1U) % TESTER_WIFI_EVENT_QUEUE_MAX);
  if(next_head == wifi_event_tail) {
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  wifi_event_queue[wifi_event_head].type = type;
  wifi_event_queue[wifi_event_head].event_id = event_id;
  wifi_event_head = next_head;
}

static void wifi_handle_frame(const char *frame)
{
  tester_wifi_print_event_t event = TESTER_WIFI_PRINT_EVENT_NONE;
  uint32_t event_id;

  if(wifi_frame_event_id(frame, &event_id) == 0U) {
    g_tester_wifi_print_rx_error_count++;
    return;
  }

  if((strstr(frame, "\"type\":\"print_ack\"") != 0 &&
      strstr(frame, "\"state\":\"QUEUED\"") != 0) ||
     strstr(frame, "ACK,") == frame) {
    event = TESTER_WIFI_PRINT_EVENT_ACK_QUEUED;
  } else if((strstr(frame, "\"type\":\"print_status\"") != 0 &&
             strstr(frame, "\"state\":\"DONE\"") != 0) ||
            strstr(frame, "DONE,") == frame) {
    event = TESTER_WIFI_PRINT_EVENT_DONE;
  } else if((strstr(frame, "\"type\":\"print_status\"") != 0 &&
             strstr(frame, "\"state\":\"ERROR\"") != 0) ||
            strstr(frame, "ERROR,") == frame) {
    event = TESTER_WIFI_PRINT_EVENT_ERROR;
  }

  if(event == TESTER_WIFI_PRINT_EVENT_NONE) {
    g_tester_wifi_print_rx_error_count++;
    return;
  }

  g_tester_wifi_print_rx_frame_count++;
  wifi_queue_event(event, event_id);
}

static void wifi_store_rx_byte(uint8_t value)
{
  if(value == '\r') {
    return;
  }

  if(value == '\n') {
    if(wifi_rx_line_len != 0U) {
      wifi_rx_line[wifi_rx_line_len] = '\0';
      wifi_handle_frame(wifi_rx_line);
    }
    wifi_rx_line_len = 0U;
    return;
  }

  if(value < 0x20U || value > 0x7EU) {
    wifi_rx_line_len = 0U;
    g_tester_wifi_print_rx_error_count++;
    return;
  }

  if(wifi_rx_line_len >= (TESTER_WIFI_RX_LINE_MAX - 1U)) {
    wifi_rx_line_len = 0U;
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  wifi_rx_line[wifi_rx_line_len] = (char)value;
  wifi_rx_line_len++;
}

static uint8_t wifi_edge_level_at(uint32_t edge_start,
                                  uint32_t edge_end,
                                  uint32_t sample_cycles)
{
  uint32_t edge;
  uint8_t level = wifi_rx_edge_levels[edge_start & TESTER_WIFI_RX_EDGE_MASK];

  for(edge = edge_start + 1U; edge != edge_end; edge++) {
    uint32_t edge_cycles = wifi_rx_edge_cycles[edge & TESTER_WIFI_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    level = wifi_rx_edge_levels[edge & TESTER_WIFI_RX_EDGE_MASK];
  }

  return level;
}

static uint32_t wifi_edge_after(uint32_t edge_start,
                                uint32_t edge_end,
                                uint32_t sample_cycles)
{
  uint32_t edge = edge_start + 1U;

  while(edge != edge_end) {
    uint32_t edge_cycles = wifi_rx_edge_cycles[edge & TESTER_WIFI_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    edge++;
  }

  return edge;
}

static void wifi_decode_rx_edges(void)
{
  uint32_t head_snapshot = wifi_rx_edge_head;
  uint32_t now_cycles = wifi_cycles();

  if((head_snapshot - wifi_rx_edge_tail) >= (TESTER_WIFI_RX_EDGE_MAX - 2U)) {
    wifi_rx_edge_tail = head_snapshot;
    g_tester_wifi_print_rx_overflow_count++;
    return;
  }

  while(wifi_rx_edge_tail != head_snapshot) {
    uint8_t bit;
    uint8_t value = 0U;
    uint32_t start_cycles;
    uint32_t stop_sample;

    if(wifi_rx_edge_levels[wifi_rx_edge_tail & TESTER_WIFI_RX_EDGE_MASK] != 0U) {
      wifi_rx_edge_tail++;
      continue;
    }

    start_cycles = wifi_rx_edge_cycles[wifi_rx_edge_tail & TESTER_WIFI_RX_EDGE_MASK];
    if((uint32_t)(now_cycles - start_cycles) < (wifi_bit_cycles * 10U)) {
      break;
    }

    for(bit = 0U; bit < 8U; bit++) {
      uint32_t sample_cycles = start_cycles + wifi_bit_cycles +
                               (wifi_bit_cycles / 2U) + (wifi_bit_cycles * bit);
      if(wifi_edge_level_at(wifi_rx_edge_tail, head_snapshot, sample_cycles) != 0U) {
        value |= (uint8_t)(1U << bit);
      }
    }

    stop_sample = start_cycles + (wifi_bit_cycles * 9U) + (wifi_bit_cycles / 2U);
    if(wifi_edge_level_at(wifi_rx_edge_tail, head_snapshot, stop_sample) == 0U) {
      g_tester_wifi_print_rx_error_count++;
      wifi_rx_edge_tail++;
      continue;
    }

    wifi_store_rx_byte(value);
    wifi_rx_edge_tail = wifi_edge_after(wifi_rx_edge_tail, head_snapshot, stop_sample);
  }
}

void tester_wifi_print_init(void)
{
  gpio_init_type gpio_init_struct;
  exint_init_type exint_init_struct;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  wifi_bit_cycles = (system_core_clock + (TESTER_WIFI_PRINT_UART_BAUDRATE / 2U)) /
                    TESTER_WIFI_PRINT_UART_BAUDRATE;
  if(wifi_bit_cycles == 0U) {
    wifi_bit_cycles = 1U;
  }

  crm_periph_clock_enable(TESTER_WIFI_TX_GPIO_CLOCK, TRUE);
  crm_periph_clock_enable(TESTER_WIFI_RX_GPIO_CLOCK, TRUE);

  TESTER_WIFI_TX_GPIO->scr = TESTER_WIFI_TX_PIN;
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = TESTER_WIFI_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(TESTER_WIFI_TX_GPIO, &gpio_init_struct);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = TESTER_WIFI_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(TESTER_WIFI_RX_GPIO, &gpio_init_struct);

  wifi_rx_edge_head = 0U;
  wifi_rx_edge_tail = 0U;
  wifi_rx_line_len = 0U;
  wifi_event_head = 0U;
  wifi_event_tail = 0U;

  crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK, TRUE);
  scfg_exint_line_config(TESTER_WIFI_RX_PORT_SOURCE, TESTER_WIFI_RX_PIN_SOURCE);
  exint_default_para_init(&exint_init_struct);
  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select = TESTER_WIFI_RX_EXINT_LINE;
  exint_init_struct.line_polarity = EXINT_TRIGGER_BOTH_EDGE;
  exint_init(&exint_init_struct);
  exint_flag_clear(TESTER_WIFI_RX_EXINT_LINE);
  nvic_irq_enable(EXINT9_5_IRQn, 1U, 0U);

  g_tester_wifi_print_ready = 1U;
}

uint8_t tester_wifi_print_request(uint32_t event_id,
                                  uint32_t test_count,
                                  uint16_t pair_count,
                                  uint16_t point_count)
{
  char frame[128];
  int written;

  if(g_tester_wifi_print_ready == 0U || event_id == 0U) {
    return 0U;
  }

  /* ESP32-C3 network firmware forwards this request to the line print host.
   * The LF is the transport frame delimiter; all values are decimal ASCII. */
  written = snprintf(frame,
                     sizeof(frame),
                     "{\"type\":\"print_request\",\"event_id\":%lu,\"station\":1,\"test_count\":%lu,\"pairs\":%u,\"points\":%u}\n",
                     (unsigned long)event_id,
                     (unsigned long)test_count,
                     (unsigned int)pair_count,
                     (unsigned int)point_count);
  if(written <= 0 || (uint32_t)written >= sizeof(frame)) {
    return 0U;
  }

  /* Old queued responses must never complete a newly triggered product. */
  wifi_event_head = 0U;
  wifi_event_tail = 0U;
  wifi_write_text(frame);
  g_tester_wifi_print_tx_request_count++;
  return 1U;
}

void tester_wifi_print_service(void)
{
  if(g_tester_wifi_print_ready == 0U) {
    return;
  }
  wifi_decode_rx_edges();
}

tester_wifi_print_event_t tester_wifi_print_poll_event(uint32_t expected_event_id)
{
  while(wifi_event_tail != wifi_event_head) {
    tester_wifi_print_event_slot_t event = wifi_event_queue[wifi_event_tail];

    wifi_event_tail = (uint8_t)((wifi_event_tail + 1U) % TESTER_WIFI_EVENT_QUEUE_MAX);
    if(event.event_id == expected_event_id) {
      return event.type;
    }
  }

  return TESTER_WIFI_PRINT_EVENT_NONE;
}

void tester_wifi_print_cancel(void)
{
  wifi_event_head = 0U;
  wifi_event_tail = 0U;
  wifi_rx_line_len = 0U;
}

void tester_wifi_print_rx_edge_isr(void)
{
  uint32_t head;

  if(g_tester_wifi_print_ready == 0U) {
    return;
  }

  head = wifi_rx_edge_head;
  wifi_rx_edge_cycles[head & TESTER_WIFI_RX_EDGE_MASK] = wifi_cycles();
  wifi_rx_edge_levels[head & TESTER_WIFI_RX_EDGE_MASK] = wifi_rx_level();
  wifi_rx_edge_head = head + 1U;
}

/* This handler lives in the timing-optimised software UART translation unit,
 * rather than the normal -O0 interrupt file.  An edge may be only one bit
 * period after the previous edge, so capture must be direct and inlined with
 * no peripheral-driver function calls. */
void EXINT9_5_IRQHandler(void)
{
  if((EXINT->intsts & TESTER_WIFI_RX_EXINT_LINE) != 0U) {
    tester_wifi_print_rx_edge_isr();
    EXINT->intsts = TESTER_WIFI_RX_EXINT_LINE;
  }
}
