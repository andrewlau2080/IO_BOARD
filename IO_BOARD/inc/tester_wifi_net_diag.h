#ifndef TESTER_WIFI_NET_DIAG_H
#define TESTER_WIFI_NET_DIAG_H

/* Standalone ESP-AT network diagnostic.  It uses the PC3/PB9 WiFi UART,
 * verifies ESP-AT, queries station/AP state, optionally joins a locally
 * configured test AP, then reads the station IP address. */
void tester_wifi_net_diag_init(void);
void tester_wifi_net_diag_service(void);

#endif
