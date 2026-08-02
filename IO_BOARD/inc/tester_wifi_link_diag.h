#ifndef TESTER_WIFI_LINK_DIAG_H
#define TESTER_WIFI_LINK_DIAG_H

/* Shared internal-HICK PLL clock for every 115200-baud PC3/PB9 software-UART
 * image.  It avoids relying on the fixture's unproven external crystal. */
void tester_wifi_clock_config(void);
/* Temporary standalone LCDM/ESP32-C3 wiring diagnostic.  This mode is
 * selected explicitly as IO_APP_MODE=WIFI_LINK_DIAG and never participates
 * in the normal cable-test or print workflow. */
void tester_wifi_link_diag_init(void);
void tester_wifi_link_diag_service(void);

#endif
