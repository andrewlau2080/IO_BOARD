#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "io_board.h"
#include "ir_remote.h"
#include "line_comm_bridge.h"

volatile uint32_t g_printer_poll_tx_counter;
volatile uint32_t g_printer_poll_code_us;
volatile uint32_t g_printer_poll_gap_us;
volatile uint16_t g_printer_poll_segment_count;
volatile uint8_t g_printer_poll_ready;

int main(void)
{
  const line_comm_ir_code_t *poll_code = 0;

  system_clock_config();
  delay_init();
  io_board_init();
  ir_io_init();

  if(line_comm_get_code(LINE_COMM_CODE_PRINT_REQUEST, &poll_code) == LINE_COMM_OK) {
    g_printer_poll_ready = 1U;
    g_printer_poll_segment_count = poll_code->count;
    g_printer_poll_code_us = line_comm_code_duration_us(poll_code);
    g_printer_poll_gap_us = line_comm_code_repeat_gap_us(poll_code, LINE_COMM_PRINTER_POLL_PERIOD_US);
  }

  while(1)
  {
    if(poll_code != 0) {
      io_debug_write(1U);
      ir_transmit_timings(poll_code->start_level,
                          poll_code->durations_us,
                          poll_code->count,
                          1U,
                          0U);
      io_debug_write(0U);
      ir_force_space_us(g_printer_poll_gap_us);
      g_printer_poll_tx_counter++;
    } else {
      delay_ms(100);
    }
  }
}
