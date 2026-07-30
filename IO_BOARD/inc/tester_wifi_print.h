#ifndef TESTER_WIFI_PRINT_H
#define TESTER_WIFI_PRINT_H

#include <stdint.h>

/*
 * LCDM high-end tester print link.
 *
 * This is deliberately a WiFi-only link to the ESP32-C3 network
 * coprocessor on PC3/PB9.  It has no dependency on the legacy IR protocol.
 */
#ifndef TESTER_WIFI_PRINT_UART_BAUDRATE
/* The present LCDM tester runs from 8 MHz HICK.  38400 leaves sufficient
 * EXINT software-UART timing margin for every RX edge. */
#define TESTER_WIFI_PRINT_UART_BAUDRATE 38400UL
#endif

typedef enum {
  TESTER_WIFI_PRINT_EVENT_NONE = 0,
  TESTER_WIFI_PRINT_EVENT_ACK_QUEUED,
  TESTER_WIFI_PRINT_EVENT_DONE,
  TESTER_WIFI_PRINT_EVENT_ERROR
} tester_wifi_print_event_t;

extern volatile uint32_t g_tester_wifi_print_tx_request_count;
extern volatile uint32_t g_tester_wifi_print_rx_frame_count;
extern volatile uint32_t g_tester_wifi_print_rx_error_count;
extern volatile uint32_t g_tester_wifi_print_rx_overflow_count;
extern volatile uint8_t g_tester_wifi_print_ready;

void tester_wifi_print_init(void);
uint8_t tester_wifi_print_request(uint32_t event_id,
                                  uint32_t test_count,
                                  uint16_t pair_count,
                                  uint16_t point_count);
void tester_wifi_print_service(void);
tester_wifi_print_event_t tester_wifi_print_poll_event(uint32_t expected_event_id);
void tester_wifi_print_cancel(void);

/* Called by EXINT9_5_IRQHandler for the PB9 WiFi RX edge stream. */
void tester_wifi_print_rx_edge_isr(void);

#endif
