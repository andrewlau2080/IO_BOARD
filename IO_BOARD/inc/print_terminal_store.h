#ifndef PRINT_TERMINAL_STORE_H
#define PRINT_TERMINAL_STORE_H

#include "print_driver.h"
#include "print_job_model.h"
#include "print_rs485.h"

#include <stdint.h>

/* The print host keeps its own operational data.  It is intentionally not
 * mixed with a tester's learned harness image or its editable WiFi identity
 * record.  Eight named templates are sufficient for a line to keep its
 * active products ready for one-touch recall without making the LCDM menu
 * unwieldy. */
#define PRINT_TERMINAL_TEMPLATE_COUNT       8U
#define PRINT_TERMINAL_TEMPLATE_NAME_MAX   16U
#define PRINT_TERMINAL_NAME_MAX             24U
#define PRINT_TERMINAL_LINE_ID_MAX          16U
#define PRINT_TERMINAL_WIFI_SSID_MAX        33U
#define PRINT_TERMINAL_WIFI_PASSWORD_MAX    64U
#define PRINT_TERMINAL_WIFI_MAC_MAX         18U

typedef struct {
  char name[PRINT_TERMINAL_TEMPLATE_NAME_MAX];
  print_job_t job;
} print_terminal_template_t;

typedef struct {
  char controller_name[PRINT_TERMINAL_NAME_MAX];
  char line_id[PRINT_TERMINAL_LINE_ID_MAX];
  char wifi_ssid[PRINT_TERMINAL_WIFI_SSID_MAX];
  char wifi_password[PRINT_TERMINAL_WIFI_PASSWORD_MAX];
  /* Learned from ESP-AT +CIFSR:STAMAC. It is read-only on LCDM and stays
   * independent of tester-side MAC storage. */
  char wifi_mac[PRINT_TERMINAL_WIFI_MAC_MAX];
  uint16_t wifi_listen_port;
  uint8_t active_template;
  uint8_t ir_fallback_enabled;
  uint8_t reserved[2];
  print_driver_config_t driver_config;
  print_rs485_config_t printer_config;
  print_terminal_template_t templates[PRINT_TERMINAL_TEMPLATE_COUNT];
} print_terminal_store_config_t;

extern volatile uint32_t g_print_terminal_store_save_count;
extern volatile uint32_t g_print_terminal_store_save_error_count;
extern volatile uint8_t g_print_terminal_store_loaded;

void print_terminal_store_init(void);
const print_terminal_store_config_t *print_terminal_store_get(void);
void print_terminal_store_copy(print_terminal_store_config_t *out_config);
uint8_t print_terminal_store_validate(const print_terminal_store_config_t *config);
uint8_t print_terminal_store_save(const print_terminal_store_config_t *config);
uint8_t print_terminal_store_is_stored(void);
/* Apply the saved printer transport and ZPL layout values after
 * print_driver_init(). */
void print_terminal_store_apply_runtime(void);
/* Persist a sanitised printer layout/serial change received through the
 * existing cfg.* command interface.  Label templates and WiFi fields are
 * preserved verbatim. */
uint8_t print_terminal_store_save_runtime_config(
  const print_driver_config_t *driver_config,
  const print_rs485_config_t *printer_config);
uint8_t print_terminal_store_update_wifi_mac(const char *mac);
uint8_t print_terminal_store_load_template(uint8_t index, print_job_t *out_job);

#endif
