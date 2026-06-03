#ifndef RPI_PROTOCOL_H
#define RPI_PROTOCOL_H

#include "io_scan.h"

#include <stdint.h>

#define RPI_PROTO_SOF0                  0x55U
#define RPI_PROTO_SOF1                  0xAAU
#define RPI_PROTO_VERSION               0x01U
#define RPI_PROTO_MAX_PAYLOAD           240U
#define RPI_PROTO_HEADER_SIZE           7U
#define RPI_PROTO_CRC_SIZE              2U
#define RPI_PROTO_MAX_FRAME             (RPI_PROTO_HEADER_SIZE + RPI_PROTO_MAX_PAYLOAD + RPI_PROTO_CRC_SIZE)

typedef enum {
  RPI_CMD_PING = 0x01,
  RPI_CMD_GET_INFO = 0x02,
  RPI_CMD_SET_PROFILE = 0x03,
  RPI_CMD_START_SCAN = 0x10,
  RPI_CMD_STOP_SCAN = 0x11,
  RPI_CMD_GET_SCAN_STATUS = 0x12,
  RPI_CMD_GET_SCAN_ROW = 0x13,
  RPI_CMD_READ_PAIR = 0x20,
  RPI_CMD_SELECT_OUT = 0x21,
  RPI_CMD_SELECT_IN = 0x22,
  RPI_CMD_LED7SEG_WRITE = 0x30,
  RPI_CMD_PRINT_REQUEST = 0x40,
  RPI_CMD_PRINT_STATUS = 0x41,
  RPI_CMD_PRINT_TEMPLATE_WRITE = 0x42,
  RPI_CMD_ERROR = 0x7F,
  RPI_RSP_BASE = 0x80
} rpi_command_t;

typedef enum {
  RPI_STATUS_OK = 0x00,
  RPI_STATUS_BAD_CRC = 0x01,
  RPI_STATUS_BAD_LENGTH = 0x02,
  RPI_STATUS_BAD_COMMAND = 0x03,
  RPI_STATUS_BAD_POSITION = 0x04,
  RPI_STATUS_BUSY = 0x05,
  RPI_STATUS_NOT_READY = 0x06
} rpi_status_t;

typedef struct {
  uint8_t command;
  uint8_t sequence;
  uint16_t payload_len;
  const uint8_t *payload;
} rpi_frame_view_t;

uint16_t rpi_protocol_crc16(const uint8_t *data, uint16_t len);
uint16_t rpi_protocol_make_position(uint8_t is_input, uint8_t point_1_based);
uint8_t rpi_protocol_encode(uint8_t command,
                            uint8_t sequence,
                            const uint8_t *payload,
                            uint16_t payload_len,
                            uint8_t *out_frame,
                            uint16_t out_capacity,
                            uint16_t *out_len);
uint8_t rpi_protocol_decode(const uint8_t *frame,
                            uint16_t frame_len,
                            rpi_frame_view_t *view);

#endif
