#include "line_comm_bridge.h"

static const uint16_t print_request_code_us[] = {
  /*
   * Printer-side polling/trigger frame from TestV1.0 analysis:
   * useful_printer_trigger_tx_envelope.csv, 71 MARK/SPACE segments.
   * First segment is MARK. Period is about 203.147 ms.
   */
  860U, 415U, 812U, 417U, 418U, 417U, 418U, 417U, 418U, 826U, 812U, 417U,
  418U, 417U, 418U, 417U, 1182U, 417U, 418U, 417U, 418U, 417U, 418U, 417U,
  812U, 417U, 812U, 417U, 418U, 417U, 418U, 417U, 418U, 826U, 812U, 417U,
  418U, 417U, 418U, 417U, 1182U, 417U, 418U, 417U, 418U, 417U, 418U, 417U,
  812U, 417U, 812U, 417U, 418U, 417U, 418U, 417U, 418U, 826U, 812U, 417U,
  418U, 417U, 418U, 417U, 1182U, 417U, 418U, 417U, 418U, 417U, 359U
};

static const uint16_t print_ack_code_us[] = {
  /* TODO: paste learned PRINT_ACK raw timings here. */
  0U
};

static const uint16_t print_busy_code_us[] = {
  /* TODO: paste learned PRINT_BUSY raw timings here. */
  0U
};

static const uint16_t print_done_code_us[] = {
  /* TODO: paste learned PRINT_DONE raw timings here. */
  0U
};

const line_comm_ir_code_t g_line_comm_ir_codes[LINE_COMM_CODE_COUNT] = {
  {0U, (uint16_t)(sizeof(print_request_code_us) / sizeof(print_request_code_us[0])), print_request_code_us},
  {0U, 0U, print_ack_code_us},
  {0U, 0U, print_busy_code_us},
  {0U, 0U, print_done_code_us},
};

uint8_t line_comm_code_available(line_comm_code_id_t code_id)
{
  if(code_id >= LINE_COMM_CODE_COUNT) {
    return 0U;
  }

  return (g_line_comm_ir_codes[code_id].count > 0U &&
          g_line_comm_ir_codes[code_id].durations_us != 0) ? 1U : 0U;
}

line_comm_status_t line_comm_get_code(line_comm_code_id_t code_id, const line_comm_ir_code_t **out_code)
{
  if(out_code == 0 || code_id >= LINE_COMM_CODE_COUNT) {
    return LINE_COMM_BAD_ARGUMENT;
  }

  if(line_comm_code_available(code_id) == 0U) {
    *out_code = 0;
    return LINE_COMM_NO_CODE;
  }

  *out_code = &g_line_comm_ir_codes[code_id];
  return LINE_COMM_OK;
}

uint32_t line_comm_code_duration_us(const line_comm_ir_code_t *code)
{
  uint32_t total_us = 0U;
  uint16_t i;

  if(code == 0 || code->durations_us == 0) {
    return 0U;
  }

  for(i = 0U; i < code->count; i++) {
    total_us += code->durations_us[i];
  }

  return total_us;
}

uint32_t line_comm_code_repeat_gap_us(const line_comm_ir_code_t *code, uint32_t period_us)
{
  uint32_t duration_us;

  duration_us = line_comm_code_duration_us(code);
  if(duration_us >= period_us) {
    return 0U;
  }

  return period_us - duration_us;
}

line_comm_status_t line_comm_make_print_request(uint8_t source_station,
                                                uint16_t product_id,
                                                uint16_t test_count,
                                                uint8_t pass,
                                                line_comm_print_request_t *out_request)
{
  if(out_request == 0 || source_station == 0U || source_station > LINE_COMM_STATION_COUNT) {
    return LINE_COMM_BAD_ARGUMENT;
  }

  out_request->source_station = source_station;
  out_request->terminal_id = LINE_COMM_TERMINAL_ID;
  out_request->product_id = product_id;
  out_request->test_count = test_count;
  out_request->pass = pass ? 1U : 0U;
  out_request->reserved = 0U;

  return LINE_COMM_OK;
}
