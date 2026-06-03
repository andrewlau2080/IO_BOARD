#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "io_board.h"
#include "ir_remote.h"
#include "line_comm_bridge.h"

volatile uint32_t g_tester_response_tx_counter;
volatile uint32_t g_tester_response_code_us;
volatile uint16_t g_tester_response_segment_count;
volatile uint8_t g_tester_response_ready;
volatile uint8_t g_tester_response_sent;

int main(void)
{
  const line_comm_ir_code_t *response_code = 0;

  system_clock_config();
  delay_init();
  io_board_init();
  ir_io_init();
  ir_set_carrier_half_us(LINE_COMM_TESTER_RESPONSE_CARRIER_HALF_US);

  if(line_comm_get_code(LINE_COMM_CODE_TESTER_RESPONSE, &response_code) == LINE_COMM_OK) {
    g_tester_response_ready = 1U;
    g_tester_response_segment_count = response_code->count;
    g_tester_response_code_us = line_comm_code_duration_us(response_code);
  }

  delay_ms(LINE_COMM_TESTER_RESPONSE_START_DELAY_MS);

  if(response_code != 0) {
    io_debug_write(1U);
    ir_transmit_timings(response_code->start_level,
                        response_code->durations_us,
                        response_code->count,
                        1U,
                        0U);
    io_debug_write(0U);
    ir_force_space_us(0U);
    g_tester_response_tx_counter++;
    g_tester_response_sent = 1U;
  }

  while(1)
  {
    delay_ms(100);
  }
}
