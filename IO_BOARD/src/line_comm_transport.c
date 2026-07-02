#include "line_comm_transport.h"

#include "ir_remote.h"

#define LINE_COMM_IR_PRINT_REQUEST_PREFIX_SEGMENTS 12U
#define LINE_COMM_IR_PRINT_REQUEST_CARRIER_HALF_US 13U

volatile uint32_t g_line_comm_transport_tx_request_count;
volatile uint32_t g_line_comm_transport_rx_request_count;
volatile uint32_t g_line_comm_transport_rx_reject_count;
volatile uint8_t g_line_comm_transport_active;
volatile uint8_t g_line_comm_transport_last_rx_station;

static line_comm_transport_type_t active_transport = LINE_COMM_TRANSPORT_IR;

void line_comm_transport_init(line_comm_transport_type_t transport)
{
  active_transport = transport;
  g_line_comm_transport_active = (uint8_t)transport;

  if(active_transport == LINE_COMM_TRANSPORT_IR) {
    ir_io_init();
    ir_set_carrier_half_us(LINE_COMM_IR_PRINT_REQUEST_CARRIER_HALF_US);
  }
}

line_comm_status_t line_comm_transport_send_print_request(const line_comm_print_request_t *request)
{
  const line_comm_ir_code_t *code = 0;

  if(request == 0) {
    return LINE_COMM_BAD_ARGUMENT;
  }

  if(active_transport != LINE_COMM_TRANSPORT_IR) {
    return LINE_COMM_NOT_READY;
  }

  if(line_comm_get_code(LINE_COMM_CODE_PRINT_REQUEST, &code) != LINE_COMM_OK || code == 0) {
    return LINE_COMM_NO_CODE;
  }

  ir_transmit_timings(code->start_level, code->durations_us, code->count, 1U, 0U);
  g_line_comm_transport_tx_request_count++;
  return LINE_COMM_OK;
}

line_comm_status_t line_comm_transport_poll_print_request(line_comm_print_request_t *out_request,
                                                          uint32_t timeout_ms)
{
  ir_raw_signal_t rx_prefix;
  const line_comm_ir_code_t *code = 0;
  uint32_t frame_start_us = 0U;
  uint16_t rx_count;

  if(out_request == 0) {
    return LINE_COMM_BAD_ARGUMENT;
  }

  if(active_transport != LINE_COMM_TRANSPORT_IR) {
    return LINE_COMM_NOT_READY;
  }

  if(line_comm_get_code(LINE_COMM_CODE_PRINT_REQUEST, &code) != LINE_COMM_OK || code == 0) {
    return LINE_COMM_NO_CODE;
  }

  rx_count = ir_capture_prefix(&rx_prefix,
                               LINE_COMM_IR_PRINT_REQUEST_PREFIX_SEGMENTS,
                               timeout_ms,
                               &frame_start_us);
  (void)frame_start_us;
  if(rx_count == 0U) {
    return LINE_COMM_NOT_READY;
  }

  if(line_comm_prefix_matches(rx_prefix.start_level,
                              rx_prefix.duration_us,
                              rx_prefix.count,
                              code,
                              LINE_COMM_IR_PRINT_REQUEST_PREFIX_SEGMENTS) == 0U) {
    g_line_comm_transport_rx_reject_count++;
    return LINE_COMM_NO_CODE;
  }

  (void)line_comm_make_print_request(1U, 0U, 0U, 1U, out_request);
  g_line_comm_transport_last_rx_station = out_request->source_station;
  g_line_comm_transport_rx_request_count++;
  return LINE_COMM_OK;
}
