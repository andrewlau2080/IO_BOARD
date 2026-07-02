#ifndef PRINT_DRIVER_H
#define PRINT_DRIVER_H

#include "print_job_model.h"

#include <stdint.h>

#define PRINT_DRIVER_PAYLOAD_MAX 512U

typedef struct {
  uint16_t label_width_dot;
  uint16_t label_length_dot;
  uint16_t origin_x_dot;
  uint16_t origin_y_dot;
  uint8_t rotation;
  uint8_t dark_mode;
  uint8_t print_speed_ips;
  uint8_t print_darkness;
  uint16_t title_font_dot;
  uint16_t body_font_dot;
  uint16_t footer_font_dot;
  uint8_t barcode_module_width;
  uint8_t barcode_ratio;
  uint16_t barcode_height_dot;
  uint8_t barcode_human_readable;
} print_driver_config_t;

typedef enum {
  PRINT_DRIVER_OK = 0,
  PRINT_DRIVER_BAD_ARGUMENT,
  PRINT_DRIVER_BUSY,
  PRINT_DRIVER_FORMAT_ERROR,
  PRINT_DRIVER_TRANSPORT_ERROR
} print_driver_status_t;

extern volatile uint32_t g_print_driver_submit_count;
extern volatile uint32_t g_print_driver_error_count;
extern volatile uint32_t g_print_driver_zpl_submit_count;
extern volatile uint8_t g_print_driver_last_status;
extern char g_print_driver_last_payload[PRINT_LABEL_TEXT_MAX];
extern char g_print_driver_last_zpl[PRINT_DRIVER_PAYLOAD_MAX];

void print_driver_init(void);
void print_driver_config_get(print_driver_config_t *out_config);
print_driver_status_t print_driver_config_set(const print_driver_config_t *config);
void print_driver_config_reset_default(void);
print_driver_status_t print_driver_submit(const print_job_t *job);
uint8_t print_driver_is_busy(void);

#endif
