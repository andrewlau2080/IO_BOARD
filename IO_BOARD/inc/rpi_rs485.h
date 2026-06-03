#ifndef RPI_RS485_H
#define RPI_RS485_H

#include <stdint.h>

#define RPI_RS485_BAUDRATE 115200U

void rpi_rs485_init(void);
uint8_t rpi_rs485_poll_byte(uint8_t *out_byte);
void rpi_rs485_write(const uint8_t *data, uint16_t len);

#endif
