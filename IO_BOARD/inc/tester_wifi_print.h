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
/* The formal tester and ESP-AT bring-up images use one fixed UART setting. */
#define TESTER_WIFI_PRINT_UART_BAUDRATE 115200UL
#endif

#define TESTER_WIFI_AT_LINE_MAX 128U

typedef enum {
  TESTER_WIFI_PRINT_EVENT_NONE = 0,
  TESTER_WIFI_PRINT_EVENT_ACK_QUEUED,
  TESTER_WIFI_PRINT_EVENT_DONE,
  TESTER_WIFI_PRINT_EVENT_ERROR,
  /* Used only by the standalone WIFI_LINK_DIAG firmware.  It never enters
   * the normal PASS-to-print state machine. */
  TESTER_WIFI_PRINT_EVENT_LINK_ACK,
  TESTER_WIFI_PRINT_EVENT_LINK_ERROR,
  TESTER_WIFI_PRINT_EVENT_LINK_FLASH_INVALID
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
/* Send the standard ESP-AT probe "AT\\r\\n".  sequence is kept locally so
 * its OK/ERROR answer is associated with the LCDM diagnostic cycle. */
uint8_t tester_wifi_print_link_test_request(uint32_t sequence);
void tester_wifi_print_service(void);
tester_wifi_print_event_t tester_wifi_print_poll_event(uint32_t expected_event_id);
void tester_wifi_print_cancel(void);

/* Raw ESP-AT line mode used only by WIFI_NET_DIAG.  While enabled, complete
 * CR/LF-delimited AT response lines are queued for the network state machine
 * instead of being interpreted as print JSON frames. */
void tester_wifi_print_at_begin(void);
void tester_wifi_print_at_end(void);
uint8_t tester_wifi_print_at_send(const char *command);
uint8_t tester_wifi_print_at_poll_line(char *line, uint8_t line_size);

/* Called by EXINT9_5_IRQHandler for the PB9 WiFi RX edge stream. */
void tester_wifi_print_rx_edge_isr(void);

#endif
