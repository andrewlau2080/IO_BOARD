#ifndef LINE_COMM_BRIDGE_H
#define LINE_COMM_BRIDGE_H

#include <stdint.h>

#define LINE_COMM_MAX_RAW_US          4096U
#define LINE_COMM_STATION_COUNT       10U
#define LINE_COMM_TERMINAL_ID         0xFEU
#define LINE_COMM_PRINTER_POLL_PERIOD_US 203147UL
#define LINE_COMM_PRINTER_POLL_PREFIX_SEGMENTS 12U
#define LINE_COMM_TESTER_RESPONSE_CARRIER_HALF_US 13U
#define LINE_COMM_TESTER_RESPONSE_TRIGGER_DELAY_US 24786UL
#define LINE_COMM_TESTER_RESPONSE_POST_TX_GUARD_US 100000UL
#define LINE_COMM_IR_MATCH_ABS_TOLERANCE_US 180U
#define LINE_COMM_IR_MATCH_PERCENT_TOLERANCE 35U

typedef enum {
  LINE_COMM_OK = 0,
  LINE_COMM_NOT_READY,
  LINE_COMM_BAD_ARGUMENT,
  LINE_COMM_NO_CODE
} line_comm_status_t;

typedef enum {
  LINE_COMM_CODE_PRINT_REQUEST = 0,
  LINE_COMM_CODE_TESTER_RESPONSE,
  LINE_COMM_CODE_PRINT_ACK,
  LINE_COMM_CODE_PRINT_BUSY,
  LINE_COMM_CODE_PRINT_DONE,
  LINE_COMM_CODE_COUNT
} line_comm_code_id_t;

typedef struct {
  uint8_t start_level;
  uint16_t count;
  const uint16_t *durations_us;
} line_comm_ir_code_t;

typedef struct {
  uint8_t source_station;
  uint8_t terminal_id;
  uint16_t product_id;
  uint16_t test_count;
  uint8_t pass;
  uint8_t reserved;
} line_comm_print_request_t;

extern const line_comm_ir_code_t g_line_comm_ir_codes[LINE_COMM_CODE_COUNT];

uint8_t line_comm_code_available(line_comm_code_id_t code_id);
line_comm_status_t line_comm_get_code(line_comm_code_id_t code_id, const line_comm_ir_code_t **out_code);
uint8_t line_comm_prefix_matches(uint8_t start_level,
                                 const uint16_t *durations_us,
                                 uint16_t count,
                                 const line_comm_ir_code_t *code,
                                 uint16_t prefix_count);
line_comm_status_t line_comm_make_print_request(uint8_t source_station,
                                                uint16_t product_id,
                                                uint16_t test_count,
                                                uint8_t pass,
                                                line_comm_print_request_t *out_request);
uint32_t line_comm_code_duration_us(const line_comm_ir_code_t *code);
uint32_t line_comm_code_repeat_gap_us(const line_comm_ir_code_t *code, uint32_t period_us);

#endif
