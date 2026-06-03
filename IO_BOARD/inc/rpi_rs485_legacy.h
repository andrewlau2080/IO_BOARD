#ifndef RPI_RS485_LEGACY_H
#define RPI_RS485_LEGACY_H

#include <stdint.h>

#define RPI_RS485_LEGACY_REQUEST_LEN  7U
#define RPI_RS485_LEGACY_RESPONSE_LEN 22U

typedef enum {
  RPI_RS485_LEGACY_SIMPLE_START = 0x10,
  RPI_RS485_LEGACY_SIMPLE_STOP = 0x11,
  RPI_RS485_LEGACY_SIMPLE_READ = 0x12,
  RPI_RS485_LEGACY_SIMPLE_WRITE = 0x13,
  RPI_RS485_LEGACY_SIMPLE_START_FORCE = 0x14,
  RPI_RS485_LEGACY_SIMPLE_READ_FORCE = 0x15,
  RPI_RS485_LEGACY_SIMPLE_KEY_READ = 0x16,
  RPI_RS485_LEGACY_SIMPLE_KEYMAP_READ = 0x17
} rpi_rs485_legacy_cmd_t;

extern volatile uint32_t g_rpi_rs485_rx_byte_counter;
extern volatile uint32_t g_rpi_rs485_request_counter;
extern volatile uint32_t g_rpi_rs485_reply_counter;
extern volatile uint32_t g_rpi_rs485_bad_frame_counter;
extern volatile uint8_t g_rpi_rs485_last_cmd;
extern volatile uint8_t g_rpi_rs485_last_request[RPI_RS485_LEGACY_REQUEST_LEN];
extern volatile uint8_t g_rpi_rs485_last_response[RPI_RS485_LEGACY_RESPONSE_LEN];

void rpi_rs485_legacy_init(void);
void rpi_rs485_legacy_service(void);

#endif
