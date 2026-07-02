#include "print_driver.h"

#include "print_rs485.h"

#include <stdio.h>
#include <string.h>

#ifndef PRINT_DRIVER_BACKEND_ZPL_RS485
#define PRINT_DRIVER_BACKEND_ZPL_RS485 0
#endif

volatile uint32_t g_print_driver_submit_count;
volatile uint32_t g_print_driver_error_count;
volatile uint32_t g_print_driver_zpl_submit_count;
volatile uint8_t g_print_driver_last_status;
char g_print_driver_last_payload[PRINT_LABEL_TEXT_MAX];
char g_print_driver_last_zpl[PRINT_DRIVER_PAYLOAD_MAX];

static print_driver_config_t driver_config;

static void config_set_default(print_driver_config_t *config)
{
  if(config == 0) {
    return;
  }

  config->label_width_dot = 600U;
  config->label_length_dot = 360U;
  config->origin_x_dot = 0U;
  config->origin_y_dot = 0U;
  config->rotation = 0U;
  config->dark_mode = 0U;
  config->print_speed_ips = 4U;
  config->print_darkness = 15U;
  config->title_font_dot = 34U;
  config->body_font_dot = 28U;
  config->footer_font_dot = 26U;
  config->barcode_module_width = 2U;
  config->barcode_ratio = 2U;
  config->barcode_height_dot = 70U;
  config->barcode_human_readable = 1U;
}

static char zpl_rotation_char(uint8_t rotation)
{
  switch(rotation) {
  case 1U: return 'R';
  case 2U: return 'I';
  case 3U: return 'B';
  default: return 'N';
  }
}

static uint8_t config_validate(print_driver_config_t *config)
{
  if(config == 0) {
    return 0U;
  }

  if(config->label_width_dot < 200U) {
    config->label_width_dot = 200U;
  } else if(config->label_width_dot > 2400U) {
    config->label_width_dot = 2400U;
  }

  if(config->label_length_dot < 120U) {
    config->label_length_dot = 120U;
  } else if(config->label_length_dot > 2400U) {
    config->label_length_dot = 2400U;
  }

  if(config->origin_x_dot > 1200U) {
    config->origin_x_dot = 1200U;
  }
  if(config->origin_y_dot > 1200U) {
    config->origin_y_dot = 1200U;
  }
  if(config->rotation > 3U) {
    config->rotation = 0U;
  }
  config->dark_mode = config->dark_mode ? 1U : 0U;

  if(config->print_speed_ips < 1U) {
    config->print_speed_ips = 1U;
  } else if(config->print_speed_ips > 14U) {
    config->print_speed_ips = 14U;
  }

  if(config->print_darkness > 30U) {
    config->print_darkness = 30U;
  }

  if(config->title_font_dot < 12U) {
    config->title_font_dot = 12U;
  } else if(config->title_font_dot > 120U) {
    config->title_font_dot = 120U;
  }
  if(config->body_font_dot < 10U) {
    config->body_font_dot = 10U;
  } else if(config->body_font_dot > 100U) {
    config->body_font_dot = 100U;
  }
  if(config->footer_font_dot < 10U) {
    config->footer_font_dot = 10U;
  } else if(config->footer_font_dot > 100U) {
    config->footer_font_dot = 100U;
  }

  if(config->barcode_module_width < 1U) {
    config->barcode_module_width = 1U;
  } else if(config->barcode_module_width > 10U) {
    config->barcode_module_width = 10U;
  }
  if(config->barcode_ratio < 2U) {
    config->barcode_ratio = 2U;
  } else if(config->barcode_ratio > 3U) {
    config->barcode_ratio = 3U;
  }
  if(config->barcode_height_dot < 20U) {
    config->barcode_height_dot = 20U;
  } else if(config->barcode_height_dot > 300U) {
    config->barcode_height_dot = 300U;
  }
  config->barcode_human_readable = config->barcode_human_readable ? 1U : 0U;

  return 1U;
}

static void copy_zpl_text(char *dst, uint16_t dst_capacity, const char *src)
{
  uint16_t i = 0U;

  if(dst == 0 || dst_capacity == 0U) {
    return;
  }

  if(src == 0) {
    dst[0] = '\0';
    return;
  }

  while(i < (uint16_t)(dst_capacity - 1U) && src[i] != '\0') {
    if(src[i] < 0x20 || src[i] > 0x7E || src[i] == '^' || src[i] == '~') {
      dst[i] = ' ';
    } else {
      dst[i] = src[i];
    }
    i++;
  }
  dst[i] = '\0';
}

static print_driver_status_t format_zpl_label(const print_job_t *job,
                                              char *out_text,
                                              uint16_t out_capacity)
{
  char title[PRINT_FIELD_TITLE_LEN];
  char item[PRINT_FIELD_ITEM_LEN];
  char content[PRINT_FIELD_CONTENT_LEN];
  char code[PRINT_FIELD_CODE_LEN];
  char rotation;
  uint16_t title_y;
  uint16_t item_y;
  uint16_t content_y;
  uint16_t code_y;
  uint16_t barcode_y;
  uint16_t footer_y;
  int written;

  if(job == 0 || out_text == 0 || out_capacity == 0U) {
    return PRINT_DRIVER_BAD_ARGUMENT;
  }

  copy_zpl_text(title, sizeof(title), job->title);
  copy_zpl_text(item, sizeof(item), job->item);
  copy_zpl_text(content, sizeof(content), job->content);
  copy_zpl_text(code, sizeof(code), job->code);
  rotation = zpl_rotation_char(driver_config.rotation);
  title_y = 25U;
  item_y = (uint16_t)(title_y + driver_config.title_font_dot + 12U);
  content_y = (uint16_t)(item_y + driver_config.body_font_dot + 12U);
  code_y = (uint16_t)(content_y + driver_config.body_font_dot + 12U);
  barcode_y = (uint16_t)(code_y + driver_config.body_font_dot + 12U);
  footer_y = (uint16_t)(barcode_y + driver_config.barcode_height_dot + 40U);

  written = snprintf(out_text,
                     out_capacity,
                     "^XA\r\n"
                     "^PW%u\r\n"
                     "^LL%u\r\n"
                     "^LH%u,%u\r\n"
                     "^PR%u\r\n"
                     "^MD%u\r\n"
                     "^FO30,%u^A0%c,%u,%u^FD%s^FS\r\n"
                     "^FO30,%u^A0%c,%u,%u^FDITEM:%s^FS\r\n"
                     "^FO30,%u^A0%c,%u,%u^FDCONT:%s^FS\r\n"
                     "^FO30,%u^A0%c,%u,%u^FDCODE:%s^FS\r\n"
                     "^BY%u,%u,%u\r\n"
                     "^FO30,%u^BC%c,%u,%c,N,N^FD%s^FS\r\n"
                     "^FO30,%u^A0%c,%u,%u^FDST:%02u QTY:%u RESULT:%s^FS\r\n"
                     "^PQ%u,0,1,Y\r\n"
                     "^XZ\r\n",
                     (unsigned int)driver_config.label_width_dot,
                     (unsigned int)driver_config.label_length_dot,
                     (unsigned int)driver_config.origin_x_dot,
                     (unsigned int)driver_config.origin_y_dot,
                     (unsigned int)driver_config.print_speed_ips,
                     (unsigned int)driver_config.print_darkness,
                     (unsigned int)title_y,
                     rotation,
                     (unsigned int)driver_config.title_font_dot,
                     (unsigned int)driver_config.title_font_dot,
                     title,
                     (unsigned int)item_y,
                     rotation,
                     (unsigned int)driver_config.body_font_dot,
                     (unsigned int)driver_config.body_font_dot,
                     item,
                     (unsigned int)content_y,
                     rotation,
                     (unsigned int)driver_config.body_font_dot,
                     (unsigned int)driver_config.body_font_dot,
                     content,
                     (unsigned int)code_y,
                     rotation,
                     (unsigned int)driver_config.body_font_dot,
                     (unsigned int)driver_config.body_font_dot,
                     code,
                     (unsigned int)driver_config.barcode_module_width,
                     (unsigned int)driver_config.barcode_ratio,
                     (unsigned int)driver_config.barcode_height_dot,
                     (unsigned int)barcode_y,
                     rotation,
                     (unsigned int)driver_config.barcode_height_dot,
                     driver_config.barcode_human_readable ? 'Y' : 'N',
                     code,
                     (unsigned int)footer_y,
                     rotation,
                     (unsigned int)driver_config.footer_font_dot,
                     (unsigned int)driver_config.footer_font_dot,
                     (unsigned int)job->station_id,
                     (unsigned int)job->quantity,
                     job->pass ? "PASS" : "NG",
                     (unsigned int)((job->copies == 0U) ? 1U : job->copies));

  if(written < 0 || written >= (int)out_capacity) {
    return PRINT_DRIVER_FORMAT_ERROR;
  }

  return PRINT_DRIVER_OK;
}

void print_driver_init(void)
{
  config_set_default(&driver_config);
  (void)config_validate(&driver_config);
  memset(g_print_driver_last_payload, 0, sizeof(g_print_driver_last_payload));
  memset(g_print_driver_last_zpl, 0, sizeof(g_print_driver_last_zpl));
  g_print_driver_last_status = PRINT_DRIVER_OK;

#if PRINT_DRIVER_BACKEND_ZPL_RS485
  print_rs485_init();
#endif
}

void print_driver_config_get(print_driver_config_t *out_config)
{
  if(out_config == 0) {
    return;
  }

  *out_config = driver_config;
}

print_driver_status_t print_driver_config_set(const print_driver_config_t *config)
{
  if(config == 0) {
    return PRINT_DRIVER_BAD_ARGUMENT;
  }

  driver_config = *config;
  (void)config_validate(&driver_config);
  return PRINT_DRIVER_OK;
}

void print_driver_config_reset_default(void)
{
  config_set_default(&driver_config);
  (void)config_validate(&driver_config);
}

print_driver_status_t print_driver_submit(const print_job_t *job)
{
  print_job_status_t status;
  print_driver_status_t driver_status;

  if(job == 0) {
    g_print_driver_error_count++;
    g_print_driver_last_status = PRINT_DRIVER_BAD_ARGUMENT;
    return PRINT_DRIVER_BAD_ARGUMENT;
  }

  status = print_job_format_ascii_label(job,
                                        g_print_driver_last_payload,
                                        (uint16_t)sizeof(g_print_driver_last_payload));
  if(status != PRINT_JOB_OK) {
    g_print_driver_error_count++;
    g_print_driver_last_status = PRINT_DRIVER_FORMAT_ERROR;
    return PRINT_DRIVER_FORMAT_ERROR;
  }

  driver_status = format_zpl_label(job,
                                   g_print_driver_last_zpl,
                                   (uint16_t)sizeof(g_print_driver_last_zpl));
  if(driver_status != PRINT_DRIVER_OK) {
    g_print_driver_error_count++;
    g_print_driver_last_status = driver_status;
    return driver_status;
  }

#if PRINT_DRIVER_BACKEND_ZPL_RS485
  print_rs485_write((const uint8_t *)g_print_driver_last_zpl,
                    (uint16_t)strlen(g_print_driver_last_zpl));
  g_print_driver_zpl_submit_count++;
#endif

  g_print_driver_submit_count++;
  g_print_driver_last_status = PRINT_DRIVER_OK;
  return PRINT_DRIVER_OK;
}

uint8_t print_driver_is_busy(void)
{
  return 0U;
}
