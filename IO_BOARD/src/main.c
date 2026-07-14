#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "first_gen_4051_scan.h"
#include "io_board.h"
#include "ir_remote.h"
#include "lcdm_tjc_verify.h"
#include "line_comm_bridge.h"
#include "print_terminal.h"
#include "rpi_rs485_legacy.h"
#include "tm1637_demo.h"

#define IO_APP_MODE_IR_PRINT_BRIDGE       1
#define IO_APP_MODE_RPI_RS485_LEGACY      2
#define IO_APP_MODE_FIRST_GEN_4051_LOCAL  3
#define IO_APP_MODE_TM1637_DEMO           4
#define IO_APP_MODE_PA4_GPIO_DIAG         5
#define IO_APP_MODE_PA4_DAC_DIAG          6
#define IO_APP_MODE_MUX_A_DIAG            7
#define IO_APP_MODE_PRINT_TERMINAL        8
#define IO_APP_MODE_UNIFIED               9
#define IO_APP_MODE_LCDM_TJC_VERIFY       10

#define UNIFIED_PRODUCT_RPI_RS485_LEGACY     IO_APP_MODE_RPI_RS485_LEGACY
#define UNIFIED_PRODUCT_FIRST_GEN_4051_LOCAL IO_APP_MODE_FIRST_GEN_4051_LOCAL
#define UNIFIED_PRODUCT_PRINT_TERMINAL       IO_APP_MODE_PRINT_TERMINAL

#ifndef IO_APP_MODE
#define IO_APP_MODE IO_APP_MODE_RPI_RS485_LEGACY
#endif

#ifndef UNIFIED_DEFAULT_PRODUCT_MODE
#define UNIFIED_DEFAULT_PRODUCT_MODE UNIFIED_PRODUCT_FIRST_GEN_4051_LOCAL
#endif

#if IO_APP_MODE == IO_APP_MODE_IR_PRINT_BRIDGE
volatile uint32_t g_tester_response_tx_counter;
volatile uint32_t g_tester_response_code_us;
volatile uint32_t g_printer_poll_rx_counter;
volatile uint32_t g_printer_poll_match_counter;
volatile uint32_t g_printer_poll_reject_counter;
volatile uint32_t g_printer_poll_trigger_delay_us;
volatile uint16_t g_printer_poll_rx_segment_count;
volatile uint16_t g_tester_response_segment_count;
volatile uint8_t g_tester_response_ready;
volatile uint8_t g_tester_response_sent;
volatile uint8_t g_tester_response_waiting_for_poll;
#endif

#if IO_APP_MODE == IO_APP_MODE_UNIFIED
volatile uint8_t g_unified_product_mode;

static uint8_t unified_select_product_mode(void)
{
  /*
   * This is the future hook for communication/config selection. Keep the
   * decision before any product module init so unselected modules never touch
   * shared pins such as PA9/PA10.
   */
  return (uint8_t)UNIFIED_DEFAULT_PRODUCT_MODE;
}
#endif

#if IO_APP_MODE == IO_APP_MODE_MUX_A_DIAG
static void diag_gpio_output(gpio_type *port, crm_periph_clock_type clock, uint16_t pin, confirm_state level)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(clock, TRUE);
  gpio_bits_write(port, pin, level);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(port, &gpio_init_struct);
}

static void diag_gpio_analog(gpio_type *port, crm_periph_clock_type clock, uint16_t pin)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(clock, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(port, &gpio_init_struct);
}

static void mux_a_diag_set_addr(uint8_t channel)
{
  gpio_bits_write(GPIOC, GPIO_PINS_10, (channel & 0x01U) ? TRUE : FALSE);
  gpio_bits_write(GPIOC, GPIO_PINS_11, (channel & 0x02U) ? TRUE : FALSE);
  gpio_bits_write(GPIOC, GPIO_PINS_12, (channel & 0x04U) ? TRUE : FALSE);

  gpio_bits_write(GPIOC, GPIO_PINS_0, (channel & 0x01U) ? TRUE : FALSE);
  gpio_bits_write(GPIOC, GPIO_PINS_1, (channel & 0x02U) ? TRUE : FALSE);
  gpio_bits_write(GPIOC, GPIO_PINS_2, (channel & 0x04U) ? TRUE : FALSE);
}

static void mux_a_diag_set_all_addr(confirm_state level)
{
  gpio_bits_write(GPIOC, GPIO_PINS_10 | GPIO_PINS_11 | GPIO_PINS_12, level);
  gpio_bits_write(GPIOC, GPIO_PINS_0 | GPIO_PINS_1 | GPIO_PINS_2, level);
}

static void mux_a_diag_init(void)
{
  system_core_clock_update();
  delay_init();
  io_board_init();

  diag_gpio_output(GPIOA, CRM_GPIOA_PERIPH_CLOCK, GPIO_PINS_4, TRUE);
  diag_gpio_analog(GPIOA, CRM_GPIOA_PERIPH_CLOCK, GPIO_PINS_0);

  io_mux_disable_all();
  gpio_bits_write(GPIOD, GPIO_PINS_0, FALSE);
  gpio_bits_write(GPIOE, GPIO_PINS_0, FALSE);
  mux_a_diag_set_addr(0U);
}
#endif

int main(void)
{
#if IO_APP_MODE == IO_APP_MODE_IR_PRINT_BRIDGE
  ir_raw_signal_t rx_prefix;
  const line_comm_ir_code_t *poll_code = 0;
  const line_comm_ir_code_t *response_code = 0;
  uint32_t poll_start_us = 0U;
  uint16_t rx_count;

  system_clock_config();
  delay_init();
  io_board_init();
  ir_io_init();
  ir_set_carrier_half_us(LINE_COMM_TESTER_RESPONSE_CARRIER_HALF_US);

  (void)line_comm_get_code(LINE_COMM_CODE_PRINT_REQUEST, &poll_code);
  if(line_comm_get_code(LINE_COMM_CODE_TESTER_RESPONSE, &response_code) == LINE_COMM_OK) {
    g_tester_response_ready = 1U;
    g_tester_response_segment_count = response_code->count;
    g_tester_response_code_us = line_comm_code_duration_us(response_code);
  }

  while(1)
  {
    if(poll_code == 0 || response_code == 0) {
      delay_ms(100);
      continue;
    }

    g_tester_response_waiting_for_poll = 1U;
    rx_count = ir_capture_prefix(&rx_prefix,
                                 LINE_COMM_PRINTER_POLL_PREFIX_SEGMENTS,
                                 0U,
                                 &poll_start_us);
    if(rx_count == 0U) {
      continue;
    }

    g_printer_poll_rx_counter++;
    g_printer_poll_rx_segment_count = rx_count;

    if(line_comm_prefix_matches(rx_prefix.start_level,
                                rx_prefix.duration_us,
                                rx_prefix.count,
                                poll_code,
                                LINE_COMM_PRINTER_POLL_PREFIX_SEGMENTS) == 0U) {
      g_printer_poll_reject_counter++;
      continue;
    }

    g_printer_poll_match_counter++;
    g_tester_response_waiting_for_poll = 0U;
    g_printer_poll_trigger_delay_us = LINE_COMM_TESTER_RESPONSE_TRIGGER_DELAY_US;
    ir_wait_until_us(poll_start_us, LINE_COMM_TESTER_RESPONSE_TRIGGER_DELAY_US);

    io_debug_write(1U);
    ir_transmit_timings(response_code->start_level,
                        response_code->durations_us,
                        response_code->count,
                        1U,
                        0U);
    io_debug_write(0U);
    ir_force_space_us(LINE_COMM_TESTER_RESPONSE_POST_TX_GUARD_US);
    g_tester_response_tx_counter++;
    g_tester_response_sent = 1U;
  }
#elif IO_APP_MODE == IO_APP_MODE_RPI_RS485_LEGACY
  system_clock_config();
  delay_init();
  io_board_init();
  rpi_rs485_legacy_init();

  while(1)
  {
    rpi_rs485_legacy_service();
  }
#elif IO_APP_MODE == IO_APP_MODE_FIRST_GEN_4051_LOCAL
  /*
   * The current fixture test board has not proven HEXT startup. Keep the
   * first-gen local tester on the internal HICK clock so display and scan
   * firmware start reliably on the assembled PCB.
   */
  system_core_clock_update();
  delay_init();
  io_board_init();
  first_gen_4051_scan_init();

  while(1)
  {
    first_gen_4051_scan_service();
  }
#elif IO_APP_MODE == IO_APP_MODE_TM1637_DEMO
  /*
   * Keep the LEDM bench firmware on the internal HICK clock. The fixture PCB
   * currently does not prove HEXT is populated/running, and waiting for HEXT
   * prevents PA9/PA10 TM1637 traffic from starting.
   */
  system_core_clock_update();
  delay_init();
  tm1637_demo_init();

  while(1)
  {
    tm1637_demo_service();
  }
#elif IO_APP_MODE == IO_APP_MODE_PA4_GPIO_DIAG
  {
    gpio_init_type gpio_init_struct;

    system_core_clock_update();
    delay_init();
    io_board_init();

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_4;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    while(1)
    {
      gpio_bits_toggle(GPIOA, GPIO_PINS_4);
      delay_ms(500U);
    }
  }
#elif IO_APP_MODE == IO_APP_MODE_PA4_DAC_DIAG
  {
    gpio_init_type gpio_init_struct;
    uint8_t high = 0U;

    system_core_clock_update();
    delay_init();
    io_board_init();

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init_struct.gpio_pins = GPIO_PINS_4;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    crm_periph_clock_enable(CRM_DAC_PERIPH_CLOCK, TRUE);
    dac_reset();
    dac_trigger_enable(DAC1_SELECT, FALSE);
    dac_wave_generate(DAC1_SELECT, DAC_WAVE_GENERATE_NONE);
    dac_output_buffer_enable(DAC1_SELECT, TRUE);
    dac_enable(DAC1_SELECT, TRUE);

    while(1)
    {
      dac_1_data_set(DAC1_12BIT_RIGHT, high ? 4095U : 0U);
      high = (uint8_t)!high;
      delay_ms(2000U);
    }
  }
#elif IO_APP_MODE == IO_APP_MODE_MUX_A_DIAG
  {
    uint8_t high = 0U;

    mux_a_diag_init();

    while(1)
    {
      mux_a_diag_set_all_addr(high ? TRUE : FALSE);
      io_debug_toggle();
      high = (uint8_t)!high;
      delay_ms(5000U);
    }
  }
#elif IO_APP_MODE == IO_APP_MODE_PRINT_TERMINAL
  system_core_clock_update();
  delay_init();
  io_board_init();
  print_terminal_init();

  while(1)
  {
    print_terminal_service();
  }
#elif IO_APP_MODE == IO_APP_MODE_LCDM_TJC_VERIFY
  system_core_clock_update();
  delay_init();
  io_board_init();
  lcdm_tjc_verify_init();

  while(1)
  {
    lcdm_tjc_verify_service();
  }
#elif IO_APP_MODE == IO_APP_MODE_UNIFIED
  {
    uint8_t product_mode = unified_select_product_mode();

    g_unified_product_mode = product_mode;

    if(product_mode == UNIFIED_PRODUCT_RPI_RS485_LEGACY) {
      system_clock_config();
      delay_init();
      io_board_init();
      rpi_rs485_legacy_init();

      while(1)
      {
        rpi_rs485_legacy_service();
      }
    }

    if(product_mode == UNIFIED_PRODUCT_PRINT_TERMINAL) {
      system_core_clock_update();
      delay_init();
      io_board_init();
      print_terminal_init();

      while(1)
      {
        print_terminal_service();
      }
    }

    system_core_clock_update();
    delay_init();
    io_board_init();
    first_gen_4051_scan_init();

    while(1)
    {
      first_gen_4051_scan_service();
    }
  }
#else
#error "Unsupported IO_APP_MODE"
#endif
}
