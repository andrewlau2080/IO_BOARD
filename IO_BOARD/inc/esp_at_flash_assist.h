#ifndef ESP_AT_FLASH_ASSIST_H
#define ESP_AT_FLASH_ASSIST_H

/*
 * One-purpose, temporary firmware mode for programming the fitted
 * ESP32-C3 through a separate USB-TTL adapter.
 *
 * PC3 drives ESP EN and PB9 drives ESP IO9 / BOOT.  Both pins are configured
 * as open-drain outputs so that writing a high level only releases the ESP's
 * existing pull-up network; this cannot drive either ESP strap above 3.3 V.
 */
void esp_at_flash_assist_init(void);

#endif
