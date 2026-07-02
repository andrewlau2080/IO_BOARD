#ifndef PRINT_RS485_H
#define PRINT_RS485_H

#include <stdint.h>

#ifndef PRINT_RS485_BAUDRATE
#define PRINT_RS485_BAUDRATE 9600U
#endif

typedef struct {
  uint32_t baudrate;
  uint8_t data_bits;
  uint8_t stop_bits;
  uint8_t parity;
  uint8_t direction_enabled;
  uint8_t direction_active_high;
} print_rs485_config_t;

extern volatile uint32_t g_print_rs485_tx_byte_count;
extern volatile uint32_t g_print_rs485_tx_frame_count;
extern volatile uint32_t g_print_rs485_reconfig_count;
extern volatile uint32_t g_print_rs485_rx_byte_count;
extern volatile uint8_t g_print_rs485_last_rx_byte;

void print_rs485_init(void);
void print_rs485_config_get(print_rs485_config_t *out_config);
void print_rs485_config_set(const print_rs485_config_t *config);
void print_rs485_config_reset_default(void);
void print_rs485_write(const uint8_t *data, uint16_t len);
uint8_t print_rs485_poll_byte(uint8_t *out_byte);

#endif
