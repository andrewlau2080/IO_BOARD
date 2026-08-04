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

/* PRINT_TERMINAL selects 384 at CMake level for complete server-side +IPD
 * records.  The validated tester image stays at its original 256-byte RAM
 * footprint. */
#ifndef TESTER_WIFI_AT_LINE_MAX
#define TESTER_WIFI_AT_LINE_MAX 256U
#endif

/* Production ESP-AT transport state.  This is deliberately separate from
 * WIFI_NET_DIAG: the final LCDM tester uses the state machine below to join
 * its saved AP, open a TCP session to the line print host, and deliver print
 * frames through AT+CIPSEND. */
typedef enum {
  TESTER_WIFI_PRINT_NETWORK_STOPPED = 0,
  TESTER_WIFI_PRINT_NETWORK_CONFIG_REQUIRED,
  TESTER_WIFI_PRINT_NETWORK_BACKOFF,
  TESTER_WIFI_PRINT_NETWORK_PROBING,
  TESTER_WIFI_PRINT_NETWORK_JOINING_AP,
  /* The AP link can be healthy before a print-host endpoint is configured.
   * Keep that state visible to the LCDM without pretending TCP is online. */
  TESTER_WIFI_PRINT_NETWORK_AP_ONLINE,
  TESTER_WIFI_PRINT_NETWORK_CONNECTING_HOST,
  TESTER_WIFI_PRINT_NETWORK_ONLINE,
  TESTER_WIFI_PRINT_NETWORK_SENDING
} tester_wifi_print_network_state_t;

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
extern volatile uint8_t g_tester_wifi_print_network_state;
extern volatile uint8_t g_tester_wifi_print_online;
extern volatile uint8_t g_tester_wifi_print_ap_connected;
extern volatile uint32_t g_tester_wifi_print_connect_count;
extern volatile uint32_t g_tester_wifi_print_reconnect_count;
extern volatile uint32_t g_tester_wifi_print_tx_payload_count;
extern volatile uint32_t g_tester_wifi_print_tx_retry_count;
extern volatile uint32_t g_tester_wifi_print_network_error_count;

void tester_wifi_print_init(void);
/* Start/restart the production session from the saved AT32 configuration.
 * This API is used only by FIRST_GEN_4051_LOCAL + LCDM.  The standalone
 * diagnostics retain explicit raw ESP-AT ownership instead. */
void tester_wifi_print_start(void);
void tester_wifi_print_restart(void);
uint8_t tester_wifi_print_is_online(void);
uint8_t tester_wifi_print_is_ap_connected(void);
uint8_t tester_wifi_print_is_configured(void);
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
/* Keep the ESP-AT channel owned by a foreground settings page after a
 * command sequence finishes.  This prevents the production reconnect state
 * machine from transmitting a second command while the operator is still on
 * the WiFi setup page.  Call tester_wifi_print_at_resume() when leaving the
 * page. */
void tester_wifi_print_at_end_hold(void);
void tester_wifi_print_at_resume(void);
uint8_t tester_wifi_print_at_send(const char *command);
/* Raw payload path used by the print-host server after ESP-AT emits the
 * CIPSEND prompt.  Unlike at_send(), this function never appends CR/LF. */
uint8_t tester_wifi_print_at_send_bytes(const uint8_t *data, uint16_t length);
uint8_t tester_wifi_print_at_poll_line(char *line, uint16_t line_size);
/* Monotonic millisecond clock maintained by tester_wifi_print_service(). */
uint32_t tester_wifi_print_now_ms(void);

/* Called by EXINT9_5_IRQHandler for the PB9 WiFi RX edge stream. */
void tester_wifi_print_rx_edge_isr(void);

#endif
