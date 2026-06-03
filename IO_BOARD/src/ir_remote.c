#include "ir_remote.h"

#include "at32f45x_board.h"

#include <stdio.h>

#define IR_RX_GPIO                 GPIOA
#define IR_RX_PIN                  GPIO_PINS_6
#define IR_RX_GPIO_CRM_CLK         CRM_GPIOA_PERIPH_CLOCK

#define IR_TX_GPIO                 GPIOA
#define IR_TX_PIN                  GPIO_PINS_7
#define IR_TX_GPIO_CRM_CLK         CRM_GPIOA_PERIPH_CLOCK

#define IR_IDLE_LEVEL              1U
#define IR_MARK_LEVEL              0U
#define IR_GAP_TIMEOUT_US          30000U
#define IR_MIN_FRAME_US            4000U
#define IR_DEFAULT_CARRIER_HALF_US 12U

volatile uint8_t g_ir_rx_level;
volatile uint8_t g_ir_rx_last_level;
volatile uint8_t g_ir_new_frame_flag;
volatile uint8_t g_ir_code_start_level;
volatile uint16_t g_ir_code_count;
volatile uint16_t g_ir_code_table_us[IR_CAPTURE_MAX_EDGES];
volatile uint32_t g_ir_poll_counter;
volatile uint32_t g_ir_rx_edge_counter;
volatile uint32_t g_ir_frame_counter;
volatile uint32_t g_ir_capture_timeout_counter;
volatile uint32_t g_ir_last_edge_delta_us;
volatile uint8_t g_ir_tx_active;
volatile uint8_t g_ir_tx_level;
volatile uint16_t g_ir_tx_segment_index;
volatile uint16_t g_ir_tx_segment_count;
volatile uint32_t g_ir_tx_duration_us;
volatile uint32_t g_ir_tx_packet_us;
volatile uint32_t g_ir_tx_gap_us;
volatile uint32_t g_ir_carrier_half_us = IR_DEFAULT_CARRIER_HALF_US;

static uint32_t g_cycles_per_us;
static ir_raw_signal_t g_saved_signal;
static uint32_t g_last_edge_us;

static void ir_timebase_init(void)
{
  g_cycles_per_us = system_core_clock / 1000000U;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t ir_micros(void)
{
  return DWT->CYCCNT / g_cycles_per_us;
}

uint32_t ir_time_us(void)
{
  return ir_micros();
}

static uint32_t ir_elapsed_us(uint32_t start_us)
{
  return ir_micros() - start_us;
}

static void ir_wait_until_elapsed_us(uint32_t start_us, uint32_t duration_us)
{
  while(ir_elapsed_us(start_us) < duration_us) {
    __asm volatile("nop");
  }
}

void ir_wait_until_us(uint32_t start_us, uint32_t duration_us)
{
  ir_wait_until_elapsed_us(start_us, duration_us);
}

static uint8_t ir_rx_level(void)
{
  return gpio_input_data_bit_read(IR_RX_GPIO, IR_RX_PIN) ? 1U : 0U;
}

static void ir_tx_write(uint8_t level)
{
  gpio_bits_write(IR_TX_GPIO, IR_TX_PIN, level ? TRUE : FALSE);
}

static void ir_mark_us(uint32_t duration_us)
{
  uint32_t start_us = ir_micros();
  uint32_t edge_us = start_us;
  uint32_t half_us = g_ir_carrier_half_us;

  while(ir_elapsed_us(start_us) < duration_us) {
    ir_tx_write(1U);
    ir_wait_until_elapsed_us(edge_us, half_us);
    edge_us += half_us;
    ir_tx_write(0U);
    ir_wait_until_elapsed_us(edge_us, half_us);
    edge_us += half_us;
  }

  ir_tx_write(0U);
}

static void ir_space_us(uint32_t duration_us)
{
  ir_tx_write(0U);
  ir_wait_until_elapsed_us(ir_micros(), duration_us);
}

void ir_force_space_us(uint32_t duration_us)
{
  g_ir_tx_active = 0U;
  g_ir_tx_level = 1U;
  g_ir_tx_segment_index = 0U;
  g_ir_tx_duration_us = duration_us;
  g_ir_tx_gap_us = duration_us;
  ir_space_us(duration_us);
}

void ir_set_carrier_half_us(uint32_t half_period_us)
{
  if(half_period_us < 8U) {
    half_period_us = 8U;
  } else if(half_period_us > 25U) {
    half_period_us = 25U;
  }

  g_ir_carrier_half_us = half_period_us;
}

void ir_io_init(void)
{
  gpio_init_type gpio_init_struct;

  ir_timebase_init();

  crm_periph_clock_enable(IR_RX_GPIO_CRM_CLK, TRUE);
  crm_periph_clock_enable(IR_TX_GPIO_CRM_CLK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = IR_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(IR_RX_GPIO, &gpio_init_struct);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = IR_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  ir_tx_write(0U);
  gpio_init(IR_TX_GPIO, &gpio_init_struct);

  g_ir_rx_level = ir_rx_level();
  g_ir_rx_last_level = g_ir_rx_level;
  g_last_edge_us = ir_micros();
}

void ir_poll_rx_status(void)
{
  uint8_t level;
  uint32_t now_us;

  level = ir_rx_level();
  g_ir_poll_counter++;
  g_ir_rx_level = level;

  if(level != g_ir_rx_last_level) {
    now_us = ir_micros();
    g_ir_last_edge_delta_us = now_us - g_last_edge_us;
    g_last_edge_us = now_us;
    g_ir_rx_last_level = level;
    g_ir_rx_edge_counter++;
  }
}

__attribute__((noinline)) void ir_debug_frame_ready(void)
{
  __asm volatile("nop");
}

uint16_t ir_capture_once(ir_raw_signal_t *signal, uint32_t start_timeout_ms)
{
  uint8_t level;
  uint8_t current_level;
  uint32_t wait_start_us;
  uint32_t segment_start_us;
  uint32_t frame_start_us;
  uint32_t duration_us;

  signal->start_level = IR_MARK_LEVEL;
  signal->count = 0;

  wait_start_us = ir_micros();
  while(ir_rx_level() == IR_IDLE_LEVEL) {
    if(start_timeout_ms != 0U &&
       ir_elapsed_us(wait_start_us) >= (start_timeout_ms * 1000U)) {
      return 0;
    }
  }

  level = ir_rx_level();
  signal->start_level = level;
  segment_start_us = ir_micros();
  frame_start_us = segment_start_us;

  while(signal->count < IR_CAPTURE_MAX_EDGES) {
    current_level = ir_rx_level();

    if(current_level != level) {
      duration_us = ir_elapsed_us(segment_start_us);
      if(duration_us > 0xFFFFU) {
        duration_us = 0xFFFFU;
      }

      signal->duration_us[signal->count++] = (uint16_t)duration_us;
      level = current_level;
      segment_start_us = ir_micros();
      continue;
    }

    if(level == IR_IDLE_LEVEL && ir_elapsed_us(segment_start_us) >= IR_GAP_TIMEOUT_US) {
      break;
    }
  }

  if(ir_elapsed_us(frame_start_us) < IR_MIN_FRAME_US || signal->count < 4U) {
    signal->count = 0;
    return 0;
  }

  g_saved_signal = *signal;
  g_ir_code_start_level = signal->start_level;
  g_ir_code_count = signal->count;

  for(uint16_t i = 0; i < IR_CAPTURE_MAX_EDGES; i++) {
    g_ir_code_table_us[i] = (i < signal->count) ? signal->duration_us[i] : 0U;
  }

  g_ir_new_frame_flag = 1U;
  g_ir_frame_counter++;
  ir_debug_frame_ready();
  return signal->count;
}

uint16_t ir_capture_prefix(ir_raw_signal_t *signal,
                           uint16_t required_segments,
                           uint32_t start_timeout_ms,
                           uint32_t *frame_start_us)
{
  uint8_t level;
  uint8_t current_level;
  uint32_t wait_start_us;
  uint32_t segment_start_us;
  uint32_t duration_us;

  if(signal == 0 || required_segments == 0U || required_segments > IR_CAPTURE_MAX_EDGES) {
    return 0U;
  }

  signal->start_level = IR_MARK_LEVEL;
  signal->count = 0U;

  wait_start_us = ir_micros();
  while(ir_rx_level() == IR_IDLE_LEVEL) {
    if(start_timeout_ms != 0U &&
       ir_elapsed_us(wait_start_us) >= (start_timeout_ms * 1000U)) {
      return 0U;
    }
  }

  level = ir_rx_level();
  signal->start_level = level;
  segment_start_us = ir_micros();
  if(frame_start_us != 0) {
    *frame_start_us = segment_start_us;
  }

  while(signal->count < required_segments) {
    current_level = ir_rx_level();

    if(current_level != level) {
      duration_us = ir_elapsed_us(segment_start_us);
      if(duration_us > 0xFFFFU) {
        duration_us = 0xFFFFU;
      }

      signal->duration_us[signal->count++] = (uint16_t)duration_us;
      level = current_level;
      segment_start_us = ir_micros();
      continue;
    }

    if(level == IR_IDLE_LEVEL && ir_elapsed_us(segment_start_us) >= IR_GAP_TIMEOUT_US) {
      break;
    }
  }

  g_saved_signal = *signal;
  g_ir_code_start_level = signal->start_level;
  g_ir_code_count = signal->count;

  for(uint16_t i = 0; i < IR_CAPTURE_MAX_EDGES; i++) {
    g_ir_code_table_us[i] = (i < signal->count) ? signal->duration_us[i] : 0U;
  }

  return signal->count;
}

void ir_print_signal(const ir_raw_signal_t *signal)
{
  uint16_t i;

  printf("IR capture done: start_level=%u count=%u\r\n",
         signal->start_level,
         signal->count);
  printf("IR saved C array, unit: microseconds\r\n");
  printf("static const uint8_t archived_ir_start_level = %u;\r\n", signal->start_level);
  printf("static const uint16_t archived_ir_signal_us[%u] = {\r\n  ", signal->count);

  for(i = 0; i < signal->count; i++) {
    printf("%u", signal->duration_us[i]);
    if(i + 1U < signal->count) {
      printf(", ");
    }
    if((i % 12U) == 11U && i + 1U < signal->count) {
      printf("\r\n  ");
    }
  }

  printf("\r\n};\r\n");
}

void ir_transmit_signal(const ir_raw_signal_t *signal, uint8_t repeat_count)
{
  if(signal == 0) {
    return;
  }

  ir_transmit_timings(signal->start_level,
                      signal->duration_us,
                      signal->count,
                      repeat_count,
                      40000U);
  if(signal->count != 0U) {
    g_saved_signal = *signal;
  }
}

void ir_transmit_timings(uint8_t start_level,
                         const uint16_t *durations_us,
                         uint16_t count,
                         uint8_t repeat_count,
                         uint32_t repeat_gap_us)
{
  uint8_t repeat;
  uint8_t level;
  uint16_t i;

  if(durations_us == 0 || count == 0U) {
    return;
  }

  g_ir_tx_segment_count = count;

  for(repeat = 0; repeat < repeat_count; repeat++) {
    level = start_level;
    g_ir_tx_active = 1U;
    g_ir_tx_packet_us = 0U;

    for(i = 0; i < count; i++) {
      g_ir_tx_level = level;
      g_ir_tx_segment_index = i + 1U;
      g_ir_tx_duration_us = durations_us[i];
      g_ir_tx_packet_us += durations_us[i];

      if(level == IR_MARK_LEVEL) {
        ir_mark_us(durations_us[i]);
      } else {
        ir_space_us(durations_us[i]);
      }
      level ^= 1U;
    }

    g_ir_tx_active = 0U;
    g_ir_tx_level = 1U;
    g_ir_tx_segment_index = 0U;

    if(repeat_gap_us != 0U) {
      g_ir_tx_gap_us = repeat_gap_us;
      ir_space_us(repeat_gap_us);
    }
  }
}
