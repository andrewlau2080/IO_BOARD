#ifndef TESTER_SETTINGS_H
#define TESTER_SETTINGS_H

#include <stdint.h>

/* LCDM-only maintenance UI for editable tester identity and WiFi settings.
 * It is entered by the first-generation scan state machine after a deliberate
 * K3 long press, never by an ordinary touch outside K1-K4. */
void tester_settings_init(void);
uint8_t tester_settings_begin(void);
void tester_settings_service(void);
uint8_t tester_settings_is_active(void);
uint8_t tester_settings_take_reset_request(void);
/* Start/restart the saved production AP + print-host session after boot or a
 * K3 recovery.  The regular tester keeps scanning while ESP-AT reconnects;
 * only the actual Hall-to-print transaction waits for ACK/DONE. */
void tester_settings_start_saved_wifi(void);
void tester_settings_network_service(uint16_t elapsed_ms);
uint8_t tester_settings_wifi_is_busy(void);

extern volatile uint8_t g_tester_settings_active;
extern volatile uint8_t g_tester_settings_wifi_test_running;
extern volatile uint8_t g_tester_settings_wifi_test_passed;
extern volatile uint32_t g_tester_settings_save_count;
extern volatile uint32_t g_tester_settings_save_error_count;

#endif
