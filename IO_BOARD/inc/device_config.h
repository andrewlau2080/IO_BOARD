#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

/*
 * Per-tester configuration.  The three MCU UID words are never accepted from
 * the LCDM and are only used to identify the physical controller board.
 * Everything in device_config_t is the operator-maintained configuration.
 */
#define DEVICE_CONFIG_MACHINE_ID_MAX       24U
#define DEVICE_CONFIG_STATION_ID_MAX        8U
#define DEVICE_CONFIG_LINE_ID_MAX          16U
#define DEVICE_CONFIG_WIFI_SSID_MAX        33U
#define DEVICE_CONFIG_WIFI_PASSWORD_MAX    64U
#define DEVICE_CONFIG_SERVICE_HOST_MAX     48U
/* A printable STA MAC is 17 characters plus the terminating NUL.  It is
 * stored as a separate, CRC-protected WiFi record so adding this cache never
 * changes the layout of the existing operator configuration image. */
#define DEVICE_CONFIG_WIFI_MAC_TEXT_MAX    18U

typedef struct {
  char machine_id[DEVICE_CONFIG_MACHINE_ID_MAX];
  char station_id[DEVICE_CONFIG_STATION_ID_MAX];
  char line_id[DEVICE_CONFIG_LINE_ID_MAX];
  char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX];
  char wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX];
  char service_host[DEVICE_CONFIG_SERVICE_HOST_MAX];
  uint16_t service_port;
  uint8_t wifi_dhcp;
  uint8_t reserved[5];
} device_config_t;

/* Load the most recent CRC-checked configuration.  If neither independent
 * Flash copy is valid, a safe editable default is supplied in RAM. */
void device_config_init(void);
const device_config_t *device_config_get(void);
uint8_t device_config_is_stored(void);

/* Copy/validate/save only editable fields.  UID is deliberately absent from
 * this API, so a screen command cannot replace the physical device identity. */
void device_config_copy(device_config_t *out_config);
uint8_t device_config_validate(const device_config_t *config);
uint8_t device_config_save(const device_config_t *config);

/* The ESP-AT STA MAC is learned from AT+CIPSTAMAC? and is not operator
 * editable.  These APIs use the independent WiFi MAC record in the same
 * reserved configuration sectors; the learned harness sector is untouched. */
uint8_t device_config_get_wifi_mac(char *text, uint8_t text_size);
uint8_t device_config_save_wifi_mac(const char *mac);

void device_config_get_uid_words(uint32_t uid_words[3]);
void device_config_format_uid(char *text, uint8_t text_size);
void device_config_format_short_code(char *text, uint8_t text_size);
/* `ST01`, `ST-01`, `WT01`, `WT-01` ... `255` and plain decimal forms are
 * accepted for compact legacy print packets.  Zero means that no valid
 * station has been configured. */
uint8_t device_config_station_number(void);

#endif
