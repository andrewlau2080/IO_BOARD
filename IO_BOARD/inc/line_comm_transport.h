#ifndef LINE_COMM_TRANSPORT_H
#define LINE_COMM_TRANSPORT_H

#include "line_comm_bridge.h"

#include <stdint.h>

typedef enum {
  LINE_COMM_TRANSPORT_NONE = -1,
  LINE_COMM_TRANSPORT_IR = 0,
  LINE_COMM_TRANSPORT_WIRELESS_UART,
  LINE_COMM_TRANSPORT_RS485
} line_comm_transport_type_t;

extern volatile uint32_t g_line_comm_transport_tx_request_count;
extern volatile uint32_t g_line_comm_transport_rx_request_count;
extern volatile uint32_t g_line_comm_transport_rx_reject_count;
extern volatile uint8_t g_line_comm_transport_active;
extern volatile uint8_t g_line_comm_transport_last_rx_station;

void line_comm_transport_init(line_comm_transport_type_t transport);
line_comm_status_t line_comm_transport_send_print_request(const line_comm_print_request_t *request);
line_comm_status_t line_comm_transport_poll_print_request(line_comm_print_request_t *out_request,
                                                          uint32_t timeout_ms);

#endif
