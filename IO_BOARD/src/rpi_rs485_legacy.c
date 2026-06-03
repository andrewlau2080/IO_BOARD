#include "rpi_rs485_legacy.h"

#include "io_scan.h"
#include "rpi_rs485.h"

#include <string.h>

#ifndef RPI_RS485_LEGACY_PROFILE
#define RPI_RS485_LEGACY_PROFILE IO_SCAN_PROFILE_DB78_64X4
#endif

#define LEGACY_SOF  0x9EU
#define LEGACY_EOF  0xCCU
#define LEGACY_OK   0x00U
#define LEGACY_BAD  0x03U
#define LEGACY_DONE 0xFFU

volatile uint32_t g_rpi_rs485_rx_byte_counter;
volatile uint32_t g_rpi_rs485_request_counter;
volatile uint32_t g_rpi_rs485_reply_counter;
volatile uint32_t g_rpi_rs485_bad_frame_counter;
volatile uint8_t g_rpi_rs485_last_cmd;
volatile uint8_t g_rpi_rs485_last_request[RPI_RS485_LEGACY_REQUEST_LEN];
volatile uint8_t g_rpi_rs485_last_response[RPI_RS485_LEGACY_RESPONSE_LEN];

static uint8_t rx_frame[RPI_RS485_LEGACY_REQUEST_LEN];
static uint8_t rx_index;
static uint8_t scan_running;
static uint8_t force_scan_running;

static uint8_t active_io_count(void)
{
  const io_scan_profile_t *profile = io_scan_active_profile();

  if(profile == 0 || profile->out_count == 0U || profile->in_count == 0U) {
    return 0U;
  }

  return (profile->out_count < profile->in_count) ? profile->out_count : profile->in_count;
}

static void copy_to_volatile(volatile uint8_t *dst, const uint8_t *src, uint8_t len)
{
  uint8_t i;

  for(i = 0U; i < len; i++) {
    dst[i] = src[i];
  }
}

static void fill_ok_bitmap(uint8_t *response, uint8_t io_count)
{
  uint8_t i;
  uint8_t byte_index;
  uint8_t bit_index;

  for(i = 0U; i < 16U; i++) {
    response[5U + i] = 0U;
  }

  for(i = 0U; i < io_count; i++) {
    byte_index = (uint8_t)(5U + (i >> 3));
    bit_index = (uint8_t)(i & 0x07U);
    response[byte_index] |= (uint8_t)(1U << bit_index);
  }
}

static void build_summary_response(uint8_t *response)
{
  uint8_t io_count = active_io_count();

  response[4] = io_count;
  if(scan_running != 0U && io_count != 0U) {
    response[5] = LEGACY_DONE;
    response[6] = LEGACY_DONE;
    response[7] = io_count;
    response[8] = 0U;
  } else {
    response[5] = 0U;
    response[6] = 0U;
    response[7] = 0U;
    response[8] = 0U;
  }
}

static uint8_t handle_request(const uint8_t *request, uint8_t *response)
{
  uint8_t cmd = request[1];
  uint8_t status = LEGACY_OK;
  uint8_t io_count = active_io_count();

  memset(response, 0, RPI_RS485_LEGACY_RESPONSE_LEN);
  response[0] = LEGACY_SOF;
  response[1] = (uint8_t)(cmd | 0x80U);
  response[21] = LEGACY_EOF;

  switch(cmd) {
  case RPI_RS485_LEGACY_SIMPLE_START:
    scan_running = 1U;
    force_scan_running = 0U;
    build_summary_response(response);
    break;

  case RPI_RS485_LEGACY_SIMPLE_STOP:
    scan_running = 0U;
    force_scan_running = 0U;
    build_summary_response(response);
    break;

  case RPI_RS485_LEGACY_SIMPLE_READ:
    build_summary_response(response);
    break;

  case RPI_RS485_LEGACY_SIMPLE_START_FORCE:
    scan_running = 1U;
    force_scan_running = 1U;
    response[4] = io_count;
    fill_ok_bitmap(response, io_count);
    break;

  case RPI_RS485_LEGACY_SIMPLE_READ_FORCE:
    (void)force_scan_running;
    response[4] = io_count;
    fill_ok_bitmap(response, io_count);
    break;

  case RPI_RS485_LEGACY_SIMPLE_WRITE:
  case RPI_RS485_LEGACY_SIMPLE_KEY_READ:
  case RPI_RS485_LEGACY_SIMPLE_KEYMAP_READ:
    response[4] = io_count;
    break;

  default:
    status = LEGACY_BAD;
    break;
  }

  response[2] = status;
  return status;
}

static void process_frame(const uint8_t *request)
{
  uint8_t response[RPI_RS485_LEGACY_RESPONSE_LEN];

  copy_to_volatile(g_rpi_rs485_last_request, request, RPI_RS485_LEGACY_REQUEST_LEN);
  g_rpi_rs485_last_cmd = request[1];
  g_rpi_rs485_request_counter++;

  (void)handle_request(request, response);
  copy_to_volatile(g_rpi_rs485_last_response, response, RPI_RS485_LEGACY_RESPONSE_LEN);

  rpi_rs485_write(response, RPI_RS485_LEGACY_RESPONSE_LEN);
  g_rpi_rs485_reply_counter++;
}

static void feed_byte(uint8_t byte)
{
  if(rx_index == 0U) {
    if(byte == LEGACY_SOF) {
      rx_frame[rx_index++] = byte;
    }
    return;
  }

  rx_frame[rx_index++] = byte;
  if(rx_index < RPI_RS485_LEGACY_REQUEST_LEN) {
    return;
  }

  if(rx_frame[0] == LEGACY_SOF && rx_frame[6] == LEGACY_EOF) {
    process_frame(rx_frame);
  } else {
    g_rpi_rs485_bad_frame_counter++;
  }

  rx_index = (byte == LEGACY_SOF) ? 1U : 0U;
  if(rx_index == 1U) {
    rx_frame[0] = byte;
  }
}

void rpi_rs485_legacy_init(void)
{
  io_scan_init((io_scan_profile_id_t)RPI_RS485_LEGACY_PROFILE);
  rpi_rs485_init();

  rx_index = 0U;
  scan_running = 1U;
  force_scan_running = 0U;
  g_rpi_rs485_rx_byte_counter = 0U;
  g_rpi_rs485_request_counter = 0U;
  g_rpi_rs485_reply_counter = 0U;
  g_rpi_rs485_bad_frame_counter = 0U;
  g_rpi_rs485_last_cmd = 0U;
  memset(rx_frame, 0, sizeof(rx_frame));
}

void rpi_rs485_legacy_service(void)
{
  uint8_t byte;

  while(rpi_rs485_poll_byte(&byte) != 0U) {
    g_rpi_rs485_rx_byte_counter++;
    feed_byte(byte);
  }
}
