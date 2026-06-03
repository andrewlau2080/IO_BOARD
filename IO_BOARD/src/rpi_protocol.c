#include "rpi_protocol.h"

static void put_u16_le(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static uint16_t get_u16_le(const uint8_t *src)
{
  return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

uint16_t rpi_protocol_crc16(const uint8_t *data, uint16_t len)
{
  uint16_t crc = 0xFFFFU;
  uint16_t i;
  uint8_t bit;

  for(i = 0; i < len; i++) {
    crc ^= data[i];
    for(bit = 0; bit < 8U; bit++) {
      if((crc & 1U) != 0U) {
        crc = (uint16_t)((crc >> 1) ^ 0xA001U);
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

uint16_t rpi_protocol_make_position(uint8_t is_input, uint8_t point_1_based)
{
  return is_input ? IO_POS_IN(point_1_based) : IO_POS_OUT(point_1_based);
}

uint8_t rpi_protocol_encode(uint8_t command,
                            uint8_t sequence,
                            const uint8_t *payload,
                            uint16_t payload_len,
                            uint8_t *out_frame,
                            uint16_t out_capacity,
                            uint16_t *out_len)
{
  uint16_t frame_len;
  uint16_t crc;
  uint16_t i;

  if(out_frame == 0 || out_len == 0 || payload_len > RPI_PROTO_MAX_PAYLOAD) {
    return 0U;
  }

  frame_len = (uint16_t)(RPI_PROTO_HEADER_SIZE + payload_len + RPI_PROTO_CRC_SIZE);
  if(out_capacity < frame_len || (payload_len > 0U && payload == 0)) {
    return 0U;
  }

  out_frame[0] = RPI_PROTO_SOF0;
  out_frame[1] = RPI_PROTO_SOF1;
  out_frame[2] = RPI_PROTO_VERSION;
  out_frame[3] = command;
  out_frame[4] = sequence;
  put_u16_le(&out_frame[5], payload_len);

  for(i = 0; i < payload_len; i++) {
    out_frame[RPI_PROTO_HEADER_SIZE + i] = payload[i];
  }

  crc = rpi_protocol_crc16(&out_frame[2], (uint16_t)(5U + payload_len));
  put_u16_le(&out_frame[RPI_PROTO_HEADER_SIZE + payload_len], crc);
  *out_len = frame_len;

  return 1U;
}

uint8_t rpi_protocol_decode(const uint8_t *frame,
                            uint16_t frame_len,
                            rpi_frame_view_t *view)
{
  uint16_t payload_len;
  uint16_t expected_len;
  uint16_t received_crc;
  uint16_t calculated_crc;

  if(frame == 0 || view == 0 || frame_len < (RPI_PROTO_HEADER_SIZE + RPI_PROTO_CRC_SIZE)) {
    return RPI_STATUS_BAD_LENGTH;
  }
  if(frame[0] != RPI_PROTO_SOF0 || frame[1] != RPI_PROTO_SOF1 || frame[2] != RPI_PROTO_VERSION) {
    return RPI_STATUS_BAD_COMMAND;
  }

  payload_len = get_u16_le(&frame[5]);
  if(payload_len > RPI_PROTO_MAX_PAYLOAD) {
    return RPI_STATUS_BAD_LENGTH;
  }

  expected_len = (uint16_t)(RPI_PROTO_HEADER_SIZE + payload_len + RPI_PROTO_CRC_SIZE);
  if(frame_len != expected_len) {
    return RPI_STATUS_BAD_LENGTH;
  }

  received_crc = get_u16_le(&frame[RPI_PROTO_HEADER_SIZE + payload_len]);
  calculated_crc = rpi_protocol_crc16(&frame[2], (uint16_t)(5U + payload_len));
  if(received_crc != calculated_crc) {
    return RPI_STATUS_BAD_CRC;
  }

  view->command = frame[3];
  view->sequence = frame[4];
  view->payload_len = payload_len;
  view->payload = (payload_len == 0U) ? 0 : &frame[RPI_PROTO_HEADER_SIZE];

  return RPI_STATUS_OK;
}
