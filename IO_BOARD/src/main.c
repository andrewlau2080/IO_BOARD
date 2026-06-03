#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "io_board.h"
#include "ir_remote.h"
#include "line_comm_bridge.h"
#include "rpi_rs485_legacy.h"

#define IO_APP_MODE_IR_PRINT_BRIDGE  1
#define IO_APP_MODE_RPI_RS485_LEGACY 2

#ifndef IO_APP_MODE
#define IO_APP_MODE IO_APP_MODE_RPI_RS485_LEGACY
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
#else
#error "Unsupported IO_APP_MODE"
#endif
}
