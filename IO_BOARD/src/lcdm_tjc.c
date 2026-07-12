#include "lcdm_tjc.h"

#include "at32f45x_board.h"
#include "at32f45x.h"
#include "at32f45x_exint.h"
#include "at32f45x_scfg.h"

#include <stdio.h>
#include <string.h>

#ifndef LCDM_TJC_GPIO
#define LCDM_TJC_GPIO GPIOB
#endif

#ifndef LCDM_TJC_GPIO_CLOCK
#define LCDM_TJC_GPIO_CLOCK CRM_GPIOB_PERIPH_CLOCK
#endif

#ifndef LCDM_TJC_SWAP_PINS
#define LCDM_TJC_SWAP_PINS 1
#endif

#if LCDM_TJC_SWAP_PINS
#ifndef LCDM_TJC_TX_PIN
#define LCDM_TJC_TX_PIN GPIO_PINS_5
#endif

#ifndef LCDM_TJC_RX_PIN
#define LCDM_TJC_RX_PIN GPIO_PINS_3
#endif
#else
#ifndef LCDM_TJC_TX_PIN
#define LCDM_TJC_TX_PIN GPIO_PINS_3
#endif

#ifndef LCDM_TJC_RX_PIN
#define LCDM_TJC_RX_PIN GPIO_PINS_5
#endif
#endif

#ifndef LCDM_TJC_USE_RESET_PIN
#define LCDM_TJC_USE_RESET_PIN 0
#endif

#ifndef LCDM_TJC_RX_EXINT_ENABLE
#define LCDM_TJC_RX_EXINT_ENABLE 0
#endif

#ifndef LCDM_TJC_TX_FOREVER_DIAG
#define LCDM_TJC_TX_FOREVER_DIAG 0
#endif

#ifndef LCDM_TJC_BIT_CYCLES_OVERRIDE
#define LCDM_TJC_BIT_CYCLES_OVERRIDE 0U
#endif

#if LCDM_TJC_USE_RESET_PIN && !defined(LCDM_TJC_RESET_PIN)
#error "LCDM_TJC_RESET_PIN must be assigned explicitly. PB4 is reserved for BUZZER_2K."
#endif

volatile uint32_t g_lcdm_tjc_tx_cmd_count;
volatile uint32_t g_lcdm_tjc_rx_byte_count;
volatile uint32_t g_lcdm_tjc_rx_packet_count;
volatile uint32_t g_lcdm_tjc_rx_overflow_count;
volatile uint32_t g_lcdm_tjc_rx_start_count;
volatile uint32_t g_lcdm_tjc_rx_frame_error_count;
volatile uint32_t g_lcdm_tjc_rx_low_sample_count;
volatile uint32_t g_lcdm_tjc_exint_count;
volatile uint32_t g_lcdm_tjc_exint_rx_ok_count;
volatile uint32_t g_lcdm_tjc_exint_rx_fail_count;
volatile uint32_t g_lcdm_tjc_rx_edge_min_cycles;
volatile uint32_t g_lcdm_tjc_rx_decode_bit_cycles;
volatile uint8_t g_lcdm_tjc_last_rx_byte;
volatile uint8_t g_lcdm_tjc_rx_level_now;
volatile uint8_t g_lcdm_tjc_last_packet0;
volatile uint8_t g_lcdm_tjc_last_packet1;
volatile uint8_t g_lcdm_tjc_last_packet2;
volatile uint8_t g_lcdm_tjc_last_packet_len;
volatile uint8_t g_lcdm_tjc_rx_debug_index;
volatile uint8_t g_lcdm_tjc_rx_debug_bytes[LCDM_TJC_RX_DEBUG_MAX];
volatile uint8_t g_lcdm_tjc_rx_edge_debug_index;
volatile uint8_t g_lcdm_tjc_rx_edge_debug_levels[16];
volatile uint16_t g_lcdm_tjc_rx_edge_debug_deltas[16];

#define LCDM_TJC_RX_EDGE_MAX 256U
#define LCDM_TJC_RX_EDGE_MASK (LCDM_TJC_RX_EDGE_MAX - 1U)

static uint8_t rx_packet[LCDM_TJC_RX_PACKET_MAX];
static uint8_t rx_len;
static uint8_t rx_ff_count;
static uint32_t cycles_per_us;
static uint32_t tx_bit_cycles;
static uint32_t rx_bit_cycles;
static uint32_t active_baudrate;
static uint8_t rx_exint_enabled;
static uint32_t rx_start_detect_cycles;
static volatile uint32_t rx_edge_cycles[LCDM_TJC_RX_EDGE_MAX];
static volatile uint8_t rx_edge_levels[LCDM_TJC_RX_EDGE_MAX];
static volatile uint32_t rx_edge_head;
static uint32_t rx_edge_tail;
static uint32_t rx_decode_bit_cycles;
static uint32_t rx_edge_last_cycles;

static void lcdm_tjc_store_rx_byte(uint8_t value);

#ifndef LCDM_TJC_CMD_GAP_MS
#define LCDM_TJC_CMD_GAP_MS 2U
#endif

#ifndef LCDM_TJC_CAPTURE_AFTER_TX
#define LCDM_TJC_CAPTURE_AFTER_TX 0
#endif

#ifndef LCDM_TJC_POWER_ON_WAIT_MS
#define LCDM_TJC_POWER_ON_WAIT_MS 500U
#endif

#ifndef LCDM_TJC_RX_POLL_US
#define LCDM_TJC_RX_POLL_US 2000U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD1
#define LCDM_TJC_RECOVERY_BAUD1 38400U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD2
#define LCDM_TJC_RECOVERY_BAUD2 115200U
#endif

#ifndef LCDM_TJC_RECOVERY_BAUD3
#define LCDM_TJC_RECOVERY_BAUD3 230400U
#endif

static void lcdm_tjc_timebase_init(void)
{
#if LCDM_TJC_BIT_CYCLES_OVERRIDE != 0U
  cycles_per_us = (LCDM_TJC_BIT_CYCLES_OVERRIDE * LCDM_TJC_BAUDRATE + 999999U) / 1000000U;
#else
  cycles_per_us = system_core_clock / 1000000U;
#endif
  if(cycles_per_us == 0U) {
    cycles_per_us = 1U;
  }

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t lcdm_tjc_cycles(void)
{
  return DWT->CYCCNT;
}

static uint32_t baud_to_cycles(uint32_t baudrate)
{
  uint32_t bit_cycles;

  if(baudrate == 0U) {
    baudrate = 9600U;
  }

  bit_cycles = (system_core_clock + (baudrate / 2U)) / baudrate;
  if(bit_cycles == 0U) {
    bit_cycles = 1U;
  }
  return bit_cycles;
}

static uint32_t baud_to_us_ceil(uint32_t baudrate)
{
  uint32_t bit_us;

  if(baudrate == 0U) {
    baudrate = 9600U;
  }

  bit_us = (1000000UL + baudrate - 1UL) / baudrate;
  if(bit_us == 0UL) {
    bit_us = 1UL;
  }
  return bit_us;
}

static void lcdm_tjc_set_active_baud(uint32_t baudrate)
{
  active_baudrate = baudrate;
#if LCDM_TJC_BIT_CYCLES_OVERRIDE != 0U
  (void)active_baudrate;
  tx_bit_cycles = LCDM_TJC_BIT_CYCLES_OVERRIDE;
#else
  tx_bit_cycles = baud_to_cycles(active_baudrate);
#endif
  rx_bit_cycles = tx_bit_cycles;
  rx_decode_bit_cycles = rx_bit_cycles;
  g_lcdm_tjc_rx_decode_bit_cycles = rx_decode_bit_cycles;
}

static void lcdm_tjc_wait_until_cycles(uint32_t target_cycles)
{
  while((int32_t)(lcdm_tjc_cycles() - target_cycles) < 0) {
    __asm volatile("nop");
  }
}

static void lcdm_tjc_delay_cycles(uint32_t duration_cycles)
{
  uint32_t start_cycles = lcdm_tjc_cycles();

  while((uint32_t)(lcdm_tjc_cycles() - start_cycles) < duration_cycles) {
    __asm volatile("nop");
  }
}

static void lcdm_tjc_delay_us(uint32_t duration_us)
{
  lcdm_tjc_delay_cycles(duration_us * cycles_per_us);
}

static void lcdm_tjc_delay_ms(uint32_t duration_ms)
{
  while(duration_ms != 0U) {
    uint32_t chunk_ms = (duration_ms > 100U) ? 100U : duration_ms;
    lcdm_tjc_delay_cycles(chunk_ms * (system_core_clock / 1000U));
    duration_ms -= chunk_ms;
  }
}

static void lcdm_tjc_tx_level(uint8_t level)
{
  if(level != 0U) {
    LCDM_TJC_GPIO->scr = LCDM_TJC_TX_PIN;
  } else {
    LCDM_TJC_GPIO->clr = LCDM_TJC_TX_PIN;
  }
}

static uint8_t lcdm_tjc_rx_level(void)
{
  g_lcdm_tjc_rx_level_now = gpio_input_data_bit_read(LCDM_TJC_GPIO, LCDM_TJC_RX_PIN) != RESET;
  if(g_lcdm_tjc_rx_level_now == 0U) {
    g_lcdm_tjc_rx_low_sample_count++;
  }
  return g_lcdm_tjc_rx_level_now;
}

static uint8_t lcdm_tjc_wait_for_start(uint32_t max_wait_us)
{
  uint32_t waited_us = 0U;
  uint8_t was_high = lcdm_tjc_rx_level();

  while(waited_us < max_wait_us) {
    uint8_t level = lcdm_tjc_rx_level();
    if(was_high != 0U && level == 0U) {
      rx_start_detect_cycles = lcdm_tjc_cycles();
      return 1U;
    }
    was_high = level;
    if(waited_us >= max_wait_us) {
      return 0U;
    }
    lcdm_tjc_delay_us(1U);
    waited_us++;
  }

  return 0U;
}

static void lcdm_tjc_write_byte(uint8_t value)
{
  uint8_t i;
  uint32_t target_cycles;
  uint32_t primask;

  target_cycles = lcdm_tjc_cycles();
  primask = __get_PRIMASK();
  __disable_irq();
  lcdm_tjc_tx_level(0U);
  target_cycles += tx_bit_cycles;
  lcdm_tjc_wait_until_cycles(target_cycles);

  for(i = 0U; i < 8U; i++) {
    lcdm_tjc_tx_level((value & (1U << i)) ? 1U : 0U);
    target_cycles += tx_bit_cycles;
    lcdm_tjc_wait_until_cycles(target_cycles);
  }

  lcdm_tjc_tx_level(1U);
  target_cycles += tx_bit_cycles;
  lcdm_tjc_wait_until_cycles(target_cycles);
  if(primask == 0U) {
    __enable_irq();
  }
}

static uint8_t lcdm_tjc_read_byte(uint8_t *out_value)
{
  uint8_t i;
  uint8_t value = 0U;
  uint32_t target_cycles;
  uint32_t primask;

  if(out_value == 0 || lcdm_tjc_rx_level() != 0U) {
    return 0U;
  }

  target_cycles = rx_start_detect_cycles + rx_bit_cycles + (rx_bit_cycles / 2U);
  primask = __get_PRIMASK();
  __disable_irq();
  lcdm_tjc_wait_until_cycles(target_cycles);
  for(i = 0U; i < 8U; i++) {
    if(lcdm_tjc_rx_level() != 0U) {
      value |= (uint8_t)(1U << i);
    }
    target_cycles += rx_bit_cycles;
    lcdm_tjc_wait_until_cycles(target_cycles);
  }

  if(lcdm_tjc_rx_level() == 0U) {
    if(primask == 0U) {
      __enable_irq();
    }
    g_lcdm_tjc_rx_frame_error_count++;
    return 0U;
  }
  if(primask == 0U) {
    __enable_irq();
  }

  *out_value = value;
  return 1U;
}

static uint8_t lcdm_tjc_read_byte_from_start_isr(uint8_t *out_value)
{
  uint8_t i;
  uint8_t value = 0U;
  uint32_t target_cycles;

  if(out_value == 0 || lcdm_tjc_rx_level() != 0U) {
    return 0U;
  }

  target_cycles = lcdm_tjc_cycles() + rx_bit_cycles + (rx_bit_cycles / 2U);
  lcdm_tjc_wait_until_cycles(target_cycles);
  for(i = 0U; i < 8U; i++) {
    if(lcdm_tjc_rx_level() != 0U) {
      value |= (uint8_t)(1U << i);
    }
    target_cycles += rx_bit_cycles;
    lcdm_tjc_wait_until_cycles(target_cycles);
  }

  if(lcdm_tjc_rx_level() == 0U) {
    g_lcdm_tjc_rx_frame_error_count++;
    return 0U;
  }

  *out_value = value;
  return 1U;
}

static uint8_t lcdm_tjc_edge_level_at(uint32_t edge_start,
                                      uint32_t edge_end,
                                      uint32_t sample_cycles)
{
  uint32_t edge;
  uint8_t level = rx_edge_levels[edge_start & LCDM_TJC_RX_EDGE_MASK];

  for(edge = edge_start + 1U; edge != edge_end; edge++) {
    uint32_t edge_cycles = rx_edge_cycles[edge & LCDM_TJC_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    level = rx_edge_levels[edge & LCDM_TJC_RX_EDGE_MASK];
  }

  return level;
}

static uint32_t lcdm_tjc_edge_after(uint32_t edge_start,
                                    uint32_t edge_end,
                                    uint32_t sample_cycles)
{
  uint32_t edge = edge_start + 1U;

  while(edge != edge_end) {
    uint32_t edge_cycles = rx_edge_cycles[edge & LCDM_TJC_RX_EDGE_MASK];
    if((int32_t)(edge_cycles - sample_cycles) > 0) {
      break;
    }
    edge++;
  }

  return edge;
}

static void lcdm_tjc_update_rx_bit_cycles(uint32_t edge_start, uint32_t edge_end)
{
  uint32_t edge;
  uint32_t min_delta = 0xFFFFFFFFUL;
  uint32_t min_valid = cycles_per_us * 3U;
  uint32_t max_valid = cycles_per_us * 30U;

  for(edge = edge_start + 1U; edge != edge_end; edge++) {
    uint32_t current = rx_edge_cycles[edge & LCDM_TJC_RX_EDGE_MASK];
    uint32_t previous = rx_edge_cycles[(edge - 1U) & LCDM_TJC_RX_EDGE_MASK];
    uint32_t delta = current - previous;
    if(delta >= min_valid && delta <= max_valid && delta < min_delta) {
      min_delta = delta;
    }
  }

  if(min_delta != 0xFFFFFFFFUL) {
    rx_decode_bit_cycles = min_delta;
    g_lcdm_tjc_rx_edge_min_cycles = min_delta;
  } else if(rx_decode_bit_cycles == 0U) {
    rx_decode_bit_cycles = rx_bit_cycles;
  }

  g_lcdm_tjc_rx_decode_bit_cycles = rx_decode_bit_cycles;
}

static void lcdm_tjc_decode_rx_edges(void)
{
  uint32_t head_snapshot = rx_edge_head;
  uint32_t bit_cycles;
  uint32_t now_cycles = lcdm_tjc_cycles();

  if(head_snapshot - rx_edge_tail >= (LCDM_TJC_RX_EDGE_MAX - 2U)) {
    rx_edge_tail = head_snapshot;
    g_lcdm_tjc_rx_overflow_count++;
    return;
  }

  lcdm_tjc_update_rx_bit_cycles(rx_edge_tail, head_snapshot);
  bit_cycles = (rx_decode_bit_cycles != 0U) ? rx_decode_bit_cycles : rx_bit_cycles;

  while(rx_edge_tail != head_snapshot) {
    uint8_t i;
    uint8_t value = 0U;
    uint32_t start_cycles;
    uint32_t stop_sample;
    uint32_t wait_cycles = bit_cycles * 10U;

    if(rx_edge_levels[rx_edge_tail & LCDM_TJC_RX_EDGE_MASK] != 0U) {
      rx_edge_tail++;
      continue;
    }

    start_cycles = rx_edge_cycles[rx_edge_tail & LCDM_TJC_RX_EDGE_MASK];
    if((uint32_t)(now_cycles - start_cycles) < wait_cycles) {
      break;
    }

    for(i = 0U; i < 8U; i++) {
      uint32_t sample_cycles = start_cycles + bit_cycles + (bit_cycles / 2U) + (bit_cycles * i);
      if(lcdm_tjc_edge_level_at(rx_edge_tail, head_snapshot, sample_cycles) != 0U) {
        value |= (uint8_t)(1U << i);
      }
    }

    stop_sample = start_cycles + (bit_cycles * 9U) + (bit_cycles / 2U);
    if(lcdm_tjc_edge_level_at(rx_edge_tail, head_snapshot, stop_sample) == 0U) {
      g_lcdm_tjc_rx_frame_error_count++;
      rx_edge_tail++;
      continue;
    }

    g_lcdm_tjc_exint_rx_ok_count++;
    g_lcdm_tjc_rx_start_count++;
    lcdm_tjc_store_rx_byte(value);
    rx_edge_tail = lcdm_tjc_edge_after(rx_edge_tail, head_snapshot, stop_sample);
  }
}

static void lcdm_tjc_store_rx_byte(uint8_t value)
{
  g_lcdm_tjc_rx_byte_count++;
  g_lcdm_tjc_last_rx_byte = value;
  g_lcdm_tjc_rx_debug_bytes[g_lcdm_tjc_rx_debug_index & (LCDM_TJC_RX_DEBUG_MAX - 1U)] = value;
  g_lcdm_tjc_rx_debug_index++;

  if(value == 0xFFU) {
    if(rx_ff_count < 3U) {
      rx_ff_count++;
    }
    return;
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

static uint8_t lcdm_tjc_read_next_byte(uint32_t max_wait_us)
{
  uint8_t value;

  if(lcdm_tjc_wait_for_start(max_wait_us) == 0U) {
    return 0U;
  }

  g_lcdm_tjc_rx_start_count++;
  if(lcdm_tjc_read_byte(&value) == 0U) {
    return 0U;
  }

  lcdm_tjc_store_rx_byte(value);
  return 1U;
}

#if LCDM_TJC_CAPTURE_AFTER_TX
static void lcdm_tjc_capture_rx_after_tx(uint32_t first_wait_us)
{
  uint8_t retry;
  uint32_t next_wait_us = baud_to_us_ceil(active_baudrate) * 3U;

  for(retry = 0U; retry < 16U; retry++) {
    uint32_t wait_us = (retry == 0U) ? first_wait_us : next_wait_us;
    if(lcdm_tjc_read_next_byte(wait_us) == 0U) {
      break;
    }
    if(rx_ff_count >= 3U) {
      break;
    }
  }
}
#endif

static uint8_t lcdm_tjc_wait_for_start_cycles_isr(uint32_t max_wait_cycles)
{
  uint32_t start_cycles = lcdm_tjc_cycles();

  while(lcdm_tjc_rx_level() != 0U) {
    if((uint32_t)(lcdm_tjc_cycles() - start_cycles) >= max_wait_cycles) {
      return 0U;
    }
  }

  return 1U;
}

static void lcdm_tjc_rx_exint_config(void)
{
  exint_init_type exint_init_struct;

#if LCDM_TJC_RX_EXINT_ENABLE && LCDM_TJC_RX_PIN == GPIO_PINS_3
  crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK, TRUE);
  scfg_exint_line_config(SCFG_PORT_SOURCE_GPIOB, SCFG_PINS_SOURCE3);

  exint_default_para_init(&exint_init_struct);
  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select = EXINT_LINE_3;
  exint_init_struct.line_polarity = EXINT_TRIGGER_BOTH_EDGE;
  exint_init(&exint_init_struct);
  exint_flag_clear(EXINT_LINE_3);
  rx_edge_last_cycles = lcdm_tjc_cycles();
  nvic_irq_enable(EXINT3_IRQn, 1U, 0U);
  rx_exint_enabled = 1U;
#else
  (void)exint_init_struct;
  rx_exint_enabled = 0U;
#endif
}

void lcdm_tjc_rx_falling_edge_isr(void)
{
  uint32_t head;
  uint32_t cycles;
  uint32_t delta;
  uint8_t level;
  uint8_t index;

  if(rx_exint_enabled == 0U) {
    return;
  }

  head = rx_edge_head;
  cycles = lcdm_tjc_cycles();
  level = gpio_input_data_bit_read(LCDM_TJC_GPIO, LCDM_TJC_RX_PIN) != RESET;
  delta = cycles - rx_edge_last_cycles;
  rx_edge_last_cycles = cycles;

  rx_edge_cycles[head & LCDM_TJC_RX_EDGE_MASK] = cycles;
  rx_edge_levels[head & LCDM_TJC_RX_EDGE_MASK] = level;
  rx_edge_head = head + 1U;

  index = g_lcdm_tjc_rx_edge_debug_index & 0x0FU;
  g_lcdm_tjc_rx_edge_debug_levels[index] = level;
  g_lcdm_tjc_rx_edge_debug_deltas[index] = (delta > 0xFFFFU) ? 0xFFFFU : (uint16_t)delta;
  g_lcdm_tjc_rx_edge_debug_index++;
  g_lcdm_tjc_exint_count++;
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

static void lcdm_tjc_send_cmd_at_active_baud(const char *cmd)
{
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};

  if(cmd == 0) {
    return;
  }

  lcdm_tjc_write_bytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  lcdm_tjc_write_bytes(end_bytes, sizeof(end_bytes));
  g_lcdm_tjc_tx_cmd_count++;
#if LCDM_TJC_CAPTURE_AFTER_TX
  lcdm_tjc_capture_rx_after_tx(30000U);
#endif
  if(LCDM_TJC_CMD_GAP_MS != 0U) {
    lcdm_tjc_delay_ms(LCDM_TJC_CMD_GAP_MS);
  }
}

static void lcdm_tjc_send_baud_cmds(uint32_t source_baud, uint32_t target_baud)
{
  char cmd[24];

  lcdm_tjc_set_active_baud(source_baud);
  lcdm_tjc_send_cmd_at_active_baud("bkcmd=0");
  (void)snprintf(cmd, sizeof(cmd), "bauds=%lu", (unsigned long)target_baud);
  lcdm_tjc_send_cmd_at_active_baud(cmd);
  (void)snprintf(cmd, sizeof(cmd), "baud=%lu", (unsigned long)target_baud);
  lcdm_tjc_send_cmd_at_active_baud(cmd);
  lcdm_tjc_delay_ms(50U);
}

static void lcdm_tjc_try_recovery_baud(uint32_t source_baud, uint32_t target_baud)
{
  if(source_baud == 0U ||
     ((source_baud == target_baud) && (target_baud != LCDM_TJC_BAUDRATE)) ||
     (source_baud == LCDM_TJC_BAUDRATE)) {
    return;
  }
  lcdm_tjc_send_baud_cmds(source_baud, target_baud);
}

static void lcdm_tjc_force_target_baud(void)
{
  lcdm_tjc_try_recovery_baud(9600U, LCDM_TJC_BAUDRATE);
  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD1, LCDM_TJC_BAUDRATE);
  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD2, LCDM_TJC_BAUDRATE);
  lcdm_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD3, LCDM_TJC_BAUDRATE);
  lcdm_tjc_send_baud_cmds(LCDM_TJC_BAUDRATE, LCDM_TJC_BAUDRATE);
  lcdm_tjc_set_active_baud(LCDM_TJC_BAUDRATE);
  lcdm_tjc_send_cmd_at_active_baud("bkcmd=0");
  lcdm_tjc_delay_ms(80U);
}

void lcdm_tjc_init(void)
{
  gpio_init_type gpio_init_struct;

  lcdm_tjc_timebase_init();
  lcdm_tjc_set_active_baud(LCDM_TJC_BAUDRATE);

  crm_periph_clock_enable(LCDM_TJC_GPIO_CLOCK, TRUE);
  gpio_pin_mux_config(LCDM_TJC_GPIO, GPIO_PINS_SOURCE3, GPIO_MUX_0);
  gpio_pin_mux_config(LCDM_TJC_GPIO, GPIO_PINS_SOURCE5, GPIO_MUX_0);

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
  rx_edge_head = 0U;
  rx_edge_tail = 0U;
  lcdm_tjc_delay_ms(LCDM_TJC_POWER_ON_WAIT_MS);
  lcdm_tjc_force_target_baud();
#if LCDM_TJC_TX_FOREVER_DIAG
  while(1) {
    lcdm_tjc_send_cmd_at_active_baud("bkcmd=0");
    lcdm_tjc_send_cmd_at_active_baud("dim=100");
    lcdm_tjc_send_cmd_at_active_baud("cls 63488");
    lcdm_tjc_delay_ms(250U);
    lcdm_tjc_send_cmd_at_active_baud("cls 2016");
    lcdm_tjc_delay_ms(250U);
    lcdm_tjc_send_cmd_at_active_baud("cls 31");
    lcdm_tjc_delay_ms(250U);
  }
#endif
  lcdm_tjc_rx_exint_config();
}

void lcdm_tjc_send_cmd(const char *cmd)
{
  lcdm_tjc_send_cmd_at_active_baud(cmd);
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
  uint8_t retry;

  if(rx_exint_enabled != 0U) {
    lcdm_tjc_decode_rx_edges();
    return;
  }

  for(retry = 0U; retry < 8U && rx_ff_count < 3U; retry++) {
    if(lcdm_tjc_read_next_byte((rx_len != 0U || rx_ff_count != 0U) ? (baud_to_us_ceil(active_baudrate) * 2U) : LCDM_TJC_RX_POLL_US) == 0U) {
      break;
    }
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
  g_lcdm_tjc_last_packet1 = (len > 1U) ? rx_packet[1] : 0U;
  g_lcdm_tjc_last_packet2 = (len > 2U) ? rx_packet[2] : 0U;

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
