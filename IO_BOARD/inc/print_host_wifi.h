#ifndef PRINT_HOST_WIFI_H
#define PRINT_HOST_WIFI_H

#include <stdint.h>

/* WiFi server used by the independent print-controller firmware.  The
 * tester remains a TCP client; this module owns the ESP-AT server socket and
 * translates print requests into a small foreground queue. */
typedef enum {
  PRINT_HOST_WIFI_STOPPED = 0,
  PRINT_HOST_WIFI_CONFIG_REQUIRED,
  PRINT_HOST_WIFI_STARTING,
  PRINT_HOST_WIFI_JOINING,
  PRINT_HOST_WIFI_STARTING_SERVER,
  PRINT_HOST_WIFI_ONLINE,
  PRINT_HOST_WIFI_ERROR
} print_host_wifi_state_t;

typedef struct {
  uint32_t event_id;
  uint8_t link_id;
  uint8_t station;
  uint16_t quantity;
  uint8_t pass;
  char device_uid[32];
  char title[24];
  char item[24];
  char content[48];
  char code[32];
} print_host_wifi_request_t;

extern volatile uint32_t g_print_host_wifi_rx_request_count;
extern volatile uint32_t g_print_host_wifi_rx_duplicate_count;
extern volatile uint32_t g_print_host_wifi_rx_error_count;
extern volatile uint32_t g_print_host_wifi_ack_count;
extern volatile uint32_t g_print_host_wifi_done_count;
extern volatile uint32_t g_print_host_wifi_error_count;
extern volatile uint32_t g_print_host_wifi_reconnect_count;
extern volatile uint8_t g_print_host_wifi_state;

void print_host_wifi_init(void);
void print_host_wifi_start(void);
void print_host_wifi_restart(void);
void print_host_wifi_service(void);
uint8_t print_host_wifi_is_online(void);
uint8_t print_host_wifi_is_error(void);
const char *print_host_wifi_status_text(void);
const char *print_host_wifi_ip_text(void);
const char *print_host_wifi_mac_text(void);
uint8_t print_host_wifi_poll_request(print_host_wifi_request_t *out_request);
/* Report that a queued request has entered the printer execution phase. */
uint8_t print_host_wifi_send_printing(const print_host_wifi_request_t *request);
uint8_t print_host_wifi_send_done(const print_host_wifi_request_t *request);
uint8_t print_host_wifi_send_error(const print_host_wifi_request_t *request);

#endif
