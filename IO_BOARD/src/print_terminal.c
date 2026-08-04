#include "print_terminal.h"

#include "lcdm_tjc.h"
#include "line_comm_transport.h"
#include "print_driver.h"
#include "print_host_wifi.h"
#include "print_job_model.h"
#include "print_rs485.h"
#include "print_terminal_settings.h"
#include "print_terminal_store.h"
#include "tester_wifi_print.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PRINT_TERMINAL_PAGE_EDIT 0U

/*
 * The production tester TFT (260728song.tft) deliberately contains the
 * shared normal-test page and its Song fonts, not a separate print-terminal
 * HMI project.  Do not depend on named page objects such as tTitle here:
 * they do not exist on that TFT and leave a blank white page.  The print
 * host instead draws its compact page directly, using exactly the same font
 * IDs, colour palette, and K1--K4 band as the validated tester screen.
 */
#define PRINT_LCDM_W                    480U
#define PRINT_LCDM_H                    272U
#define PRINT_LCDM_KEY_Y                214U
#define PRINT_LCDM_KEY_W                120U
#define PRINT_LCDM_KEY_H                 58U
#define PRINT_LCDM_FONT_SMALL             4U
#define PRINT_LCDM_FONT_STATUS            3U
#define PRINT_LCDM_FONT_TITLE             2U
#define PRINT_LCDM_BLACK                  0U
#define PRINT_LCDM_BLUE                  31U
#define PRINT_LCDM_GREEN               2016U
#define PRINT_LCDM_RED                63488U
#define PRINT_LCDM_WHITE              65535U
#define PRINT_LCDM_NAVY                  16U
#define PRINT_LCDM_ROW_BG             61374U
#define PRINT_LCDM_PALE_BLUE          50719U
#define PRINT_LCDM_PALE_CYAN          49151U
#define PRINT_LCDM_SOFT_PINK          64593U
#define PRINT_LCDM_MAGENTA            63519U
#define PRINT_LCDM_ORANGE             64512U

volatile uint32_t g_print_terminal_event_count;
volatile uint32_t g_print_terminal_refresh_count;
volatile uint32_t g_print_terminal_print_count;
volatile uint32_t g_print_terminal_ir_request_count;
volatile uint32_t g_print_terminal_ir_print_count;
volatile uint8_t g_print_terminal_state;

static print_job_t active_job;
static char preview_text[PRINT_LABEL_TEXT_MAX];
static char lcdm_status_text[32];
static uint8_t refresh_requested;
static uint8_t lcdm_frame_drawn;

static uint16_t host_status_color(void)
{
  return print_host_wifi_is_online() != 0U ? PRINT_LCDM_GREEN : PRINT_LCDM_BLUE;
}

static uint8_t ascii_is_allowed(char ch)
{
  return (ch >= 0x20 && ch <= 0x7E) ? 1U : 0U;
}

static void sanitize_ascii(char *text)
{
  uint16_t i;

  if(text == 0) {
    return;
  }

  for(i = 0U; text[i] != '\0'; i++) {
    if(ascii_is_allowed(text[i]) == 0U) {
      text[i] = ' ';
    } else if(text[i] == '"') {
      text[i] = '\'';
    } else if(text[i] == '\\') {
      text[i] = '/';
    }
  }
}

static uint16_t parse_u16(const char *text, uint16_t fallback)
{
  uint32_t value = 0U;
  uint8_t any = 0U;

  if(text == 0) {
    return fallback;
  }

  while(*text != '\0') {
    if(isdigit((unsigned char)*text) == 0) {
      break;
    }
    any = 1U;
    value = (value * 10UL) + (uint32_t)(*text - '0');
    if(value > 65535UL) {
      return 65535U;
    }
    text++;
  }

  return (any != 0U) ? (uint16_t)value : fallback;
}

static uint32_t parse_u32(const char *text, uint32_t fallback)
{
  uint32_t value = 0U;
  uint8_t any = 0U;

  if(text == 0) {
    return fallback;
  }

  while(*text != '\0') {
    if(isdigit((unsigned char)*text) == 0) {
      break;
    }
    any = 1U;
    if(value > 429496729UL) {
      return fallback;
    }
    value = (value * 10UL) + (uint32_t)(*text - '0');
    text++;
  }

  return (any != 0U) ? value : fallback;
}

static void update_preview(void)
{
  if(print_job_format_ascii_label(&active_job,
                                  preview_text,
                                  (uint16_t)sizeof(preview_text)) != PRINT_JOB_OK) {
    (void)snprintf(preview_text, sizeof(preview_text), "LABEL FORMAT ERROR");
  }
}

static void lcdm_raw_fill(uint16_t x,
                          uint16_t y,
                          uint16_t width,
                          uint16_t height,
                          uint16_t color)
{
  char command[48];

  (void)snprintf(command,
                 sizeof(command),
                 "fill %u,%u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)width,
                 (unsigned int)height,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(command);
}

static void lcdm_raw_cirs(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
  char command[48];

  (void)snprintf(command,
                 sizeof(command),
                 "cirs %u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)radius,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(command);
}

static void lcdm_raw_round_rect(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                uint16_t radius,
                                uint16_t color)
{
  if(width <= (uint16_t)(radius * 2U) || height <= (uint16_t)(radius * 2U)) {
    lcdm_raw_fill(x, y, width, height, color);
    return;
  }

  lcdm_raw_fill((uint16_t)(x + radius),
                y,
                (uint16_t)(width - (uint16_t)(radius * 2U)),
                height,
                color);
  lcdm_raw_fill(x,
                (uint16_t)(y + radius),
                width,
                (uint16_t)(height - (uint16_t)(radius * 2U)),
                color);
  lcdm_raw_cirs((uint16_t)(x + radius), (uint16_t)(y + radius), radius, color);
  lcdm_raw_cirs((uint16_t)(x + width - radius - 1U),
                (uint16_t)(y + radius),
                radius,
                color);
  lcdm_raw_cirs((uint16_t)(x + radius),
                (uint16_t)(y + height - radius - 1U),
                radius,
                color);
  lcdm_raw_cirs((uint16_t)(x + width - radius - 1U),
                (uint16_t)(y + height - radius - 1U),
                radius,
                color);
}

static void lcdm_raw_xstr(uint16_t x,
                          uint16_t y,
                          uint16_t width,
                          uint16_t height,
                          uint16_t font,
                          uint16_t foreground,
                          uint16_t background,
                          uint8_t align,
                          const char *text)
{
  char command[192];

  if(text == 0) {
    text = "";
  }

  (void)snprintf(command,
                 sizeof(command),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)width,
                 (unsigned int)height,
                 (unsigned int)font,
                 (unsigned int)foreground,
                 (unsigned int)background,
                 (unsigned int)align,
                 text);
  lcdm_tjc_send_cmd(command);
}

static uint16_t lcdm_key_color(uint8_t index)
{
  if(index == 1U) {
    return PRINT_LCDM_SOFT_PINK;
  }
  if(index == 2U) {
    return PRINT_LCDM_MAGENTA;
  }
  if(index == 3U) {
    return PRINT_LCDM_ORANGE;
  }
  return PRINT_LCDM_BLUE;
}

static void lcdm_draw_key(uint8_t index, const char *caption)
{
  char key_name[8];
  uint16_t x = (uint16_t)(index * PRINT_LCDM_KEY_W);
  uint16_t background = lcdm_key_color(index);

  (void)snprintf(key_name, sizeof(key_name), "K%u", (unsigned int)(index + 1U));
  lcdm_raw_fill(x, PRINT_LCDM_KEY_Y, PRINT_LCDM_KEY_W, PRINT_LCDM_KEY_H, PRINT_LCDM_WHITE);
  lcdm_raw_round_rect((uint16_t)(x + 2U),
                      (uint16_t)(PRINT_LCDM_KEY_Y + 2U),
                      (uint16_t)(PRINT_LCDM_KEY_W - 4U),
                      (uint16_t)(PRINT_LCDM_KEY_H - 4U),
                      7U,
                      background);
  lcdm_raw_xstr((uint16_t)(x + 4U),
                (uint16_t)(PRINT_LCDM_KEY_Y + 5U),
                112U,
                31U,
                PRINT_LCDM_FONT_TITLE,
                PRINT_LCDM_WHITE,
                background,
                1U,
                key_name);
  lcdm_raw_xstr((uint16_t)(x + 4U),
                (uint16_t)(PRINT_LCDM_KEY_Y + 34U),
                112U,
                19U,
                PRINT_LCDM_FONT_SMALL,
                PRINT_LCDM_WHITE,
                background,
                1U,
                caption);
}

static void lcdm_draw_static_frame(void)
{
  lcdm_tjc_draw_batch_begin();
  lcdm_raw_fill(0U, 0U, PRINT_LCDM_W, PRINT_LCDM_H, PRINT_LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, PRINT_LCDM_W, 32U, PRINT_LCDM_NAVY);
  lcdm_raw_xstr(0U,
                1U,
                PRINT_LCDM_W,
                30U,
                PRINT_LCDM_FONT_TITLE,
                PRINT_LCDM_WHITE,
                PRINT_LCDM_NAVY,
                1U,
                "PRINT CONTROLLER");
  lcdm_draw_key(0U, "PREVIEW");
  lcdm_draw_key(1U, "TEST PRINT");
  lcdm_draw_key(2U, "SETUP");
  lcdm_draw_key(3U, "REFRESH");
  lcdm_tjc_draw_batch_end();
  lcdm_frame_drawn = 1U;
}

static void lcdm_draw_dynamic_content(void)
{
  print_rs485_config_t rs485_cfg;
  char station_text[32];
  char link_text[48];
  char result_text[48];
  char template_text[48];
  const print_terminal_store_config_t *store;
  const char *status = lcdm_status_text;
  uint16_t status_fg = PRINT_LCDM_BLUE;
  uint16_t status_bg = PRINT_LCDM_ROW_BG;

  print_rs485_config_get(&rs485_cfg);
  if(g_print_terminal_state == PRINT_TERMINAL_STATE_PREVIEW) {
    status = "PREVIEW";
    status_bg = PRINT_LCDM_PALE_BLUE;
    status_fg = PRINT_LCDM_NAVY;
  } else if(g_print_terminal_state == PRINT_TERMINAL_STATE_PRINTED) {
    status_bg = PRINT_LCDM_GREEN;
    status_fg = PRINT_LCDM_WHITE;
  } else if(g_print_terminal_state == PRINT_TERMINAL_STATE_ERROR) {
    status_bg = PRINT_LCDM_RED;
    status_fg = PRINT_LCDM_WHITE;
  }
  if(status == 0 || status[0] == '\0') {
    status = "READY";
  }

  (void)snprintf(station_text,
                 sizeof(station_text),
                 "ST%02u  %s  QTY %u",
                 (unsigned int)active_job.station_id,
                 active_job.pass ? "PASS" : "NG",
                 (unsigned int)active_job.quantity);
  (void)snprintf(link_text,
                 sizeof(link_text),
                 "%s  %lu",
                 print_host_wifi_status_text(),
                 (unsigned long)rs485_cfg.baudrate);
  (void)snprintf(result_text,
                 sizeof(result_text),
                 "CODE: %s",
                 active_job.code);
  store = print_terminal_store_get();
  if(store != 0 && store->active_template < PRINT_TERMINAL_TEMPLATE_COUNT) {
    (void)snprintf(template_text, sizeof(template_text), "T%u %s / PRINT HOST",
                   (unsigned int)(store->active_template + 1U),
                   store->templates[store->active_template].name);
  } else {
    (void)snprintf(template_text, sizeof(template_text), "PRINT HOST / LOCAL TEST");
  }

  /* Only these changing rectangles are repainted after a touch or a print
   * result.  The title and K1--K4 strip stay intact; typing or refreshes do
   * not cause the visibly poor full-screen flicker seen in the old draft. */
  lcdm_raw_fill(16U, 42U, 448U, 33U, status_bg);
  lcdm_raw_xstr(24U, 47U, 432U, 23U, PRINT_LCDM_FONT_STATUS,
                status_fg, status_bg, 1U, status);
  lcdm_raw_fill(16U, 82U, 448U, 124U, PRINT_LCDM_WHITE);
  lcdm_raw_xstr(24U, 84U, 432U, 22U, PRINT_LCDM_FONT_STATUS,
                PRINT_LCDM_NAVY, PRINT_LCDM_WHITE, 1U, template_text);
  lcdm_raw_xstr(24U, 108U, 432U, 20U, PRINT_LCDM_FONT_SMALL,
                PRINT_LCDM_BLUE, PRINT_LCDM_WHITE, 0U, active_job.title);
  lcdm_raw_xstr(24U, 130U, 432U, 20U, PRINT_LCDM_FONT_SMALL,
                PRINT_LCDM_NAVY, PRINT_LCDM_WHITE, 0U, active_job.item);
  lcdm_raw_xstr(24U, 152U, 432U, 20U, PRINT_LCDM_FONT_SMALL,
                PRINT_LCDM_NAVY, PRINT_LCDM_WHITE, 0U, result_text);
  lcdm_raw_xstr(24U, 174U, 216U, 20U, PRINT_LCDM_FONT_SMALL,
                PRINT_LCDM_NAVY, PRINT_LCDM_WHITE, 0U, station_text);
  lcdm_raw_xstr(240U, 174U, 216U, 20U, PRINT_LCDM_FONT_SMALL,
                host_status_color() == PRINT_LCDM_GREEN ? PRINT_LCDM_WHITE : PRINT_LCDM_NAVY,
                host_status_color() == PRINT_LCDM_GREEN ? PRINT_LCDM_GREEN : PRINT_LCDM_WHITE,
                2U, link_text);
}

static void send_job_to_lcdm(void)
{
  update_preview();
  if(lcdm_frame_drawn == 0U) {
    lcdm_draw_static_frame();
  }
  lcdm_draw_dynamic_content();
}

static void set_status_text(const char *text)
{
  if(text == 0) {
    text = "";
  }
  (void)snprintf(lcdm_status_text, sizeof(lcdm_status_text), "%s", text);
}

static void submit_current_print_job(const char *ok_text)
{
  if(print_driver_submit(&active_job) == PRINT_DRIVER_OK) {
    g_print_terminal_print_count++;
    g_print_terminal_state = PRINT_TERMINAL_STATE_PRINTED;
    set_status_text(ok_text);
  } else {
    g_print_terminal_state = PRINT_TERMINAL_STATE_ERROR;
    set_status_text("PRINT ERROR");
  }
  refresh_requested = 1U;
}

static uint8_t starts_with(const char *text, const char *prefix)
{
  return (strncmp(text, prefix, strlen(prefix)) == 0) ? 1U : 0U;
}

static void handle_config_packet(const char *packet)
{
  print_driver_config_t driver_cfg;
  print_rs485_config_t rs485_cfg;
  const char *value;

  if(packet == 0 || starts_with(packet, "cfg.") == 0U) {
    return;
  }

  value = strchr(packet, '=');
  if(value == 0) {
    return;
  }
  value++;

  print_driver_config_get(&driver_cfg);
  print_rs485_config_get(&rs485_cfg);

  if(starts_with(packet, "cfg.baud=") != 0U) {
    rs485_cfg.baudrate = parse_u32(value, rs485_cfg.baudrate);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.databits=") != 0U) {
    rs485_cfg.data_bits = (uint8_t)parse_u16(value, rs485_cfg.data_bits);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.stop=") != 0U) {
    rs485_cfg.stop_bits = (uint8_t)parse_u16(value, rs485_cfg.stop_bits);
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.parity=") != 0U) {
    if(strcmp(value, "EVEN") == 0 || strcmp(value, "even") == 0 || strcmp(value, "1") == 0) {
      rs485_cfg.parity = 1U;
    } else if(strcmp(value, "ODD") == 0 || strcmp(value, "odd") == 0 || strcmp(value, "2") == 0) {
      rs485_cfg.parity = 2U;
    } else {
      rs485_cfg.parity = 0U;
    }
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.dir_en=") != 0U) {
    rs485_cfg.direction_enabled = (parse_u16(value, rs485_cfg.direction_enabled) == 0U) ? 0U : 1U;
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.dir_hi=") != 0U) {
    rs485_cfg.direction_active_high = (parse_u16(value, rs485_cfg.direction_active_high) == 0U) ? 0U : 1U;
    print_rs485_config_set(&rs485_cfg);
  } else if(starts_with(packet, "cfg.width=") != 0U) {
    driver_cfg.label_width_dot = parse_u16(value, driver_cfg.label_width_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.length=") != 0U) {
    driver_cfg.label_length_dot = parse_u16(value, driver_cfg.label_length_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.orgx=") != 0U) {
    driver_cfg.origin_x_dot = parse_u16(value, driver_cfg.origin_x_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.orgy=") != 0U) {
    driver_cfg.origin_y_dot = parse_u16(value, driver_cfg.origin_y_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.rot=") != 0U) {
    driver_cfg.rotation = (uint8_t)parse_u16(value, driver_cfg.rotation);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.speed=") != 0U) {
    driver_cfg.print_speed_ips = (uint8_t)parse_u16(value, driver_cfg.print_speed_ips);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.dark=") != 0U) {
    driver_cfg.print_darkness = (uint8_t)parse_u16(value, driver_cfg.print_darkness);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.titlefont=") != 0U) {
    driver_cfg.title_font_dot = parse_u16(value, driver_cfg.title_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.bodyfont=") != 0U) {
    driver_cfg.body_font_dot = parse_u16(value, driver_cfg.body_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.footfont=") != 0U) {
    driver_cfg.footer_font_dot = parse_u16(value, driver_cfg.footer_font_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barw=") != 0U) {
    driver_cfg.barcode_module_width = (uint8_t)parse_u16(value, driver_cfg.barcode_module_width);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barratio=") != 0U) {
    driver_cfg.barcode_ratio = (uint8_t)parse_u16(value, driver_cfg.barcode_ratio);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.barh=") != 0U) {
    driver_cfg.barcode_height_dot = parse_u16(value, driver_cfg.barcode_height_dot);
    (void)print_driver_config_set(&driver_cfg);
  } else if(starts_with(packet, "cfg.bartext=") != 0U) {
    driver_cfg.barcode_human_readable = (parse_u16(value, driver_cfg.barcode_human_readable) == 0U) ? 0U : 1U;
    (void)print_driver_config_set(&driver_cfg);
  } else if(strcmp(packet, "cfg.default=1") == 0) {
    print_driver_config_reset_default();
    print_rs485_config_reset_default();
  }

  print_driver_config_get(&driver_cfg);
  print_rs485_config_get(&rs485_cfg);
  if(print_terminal_store_save_runtime_config(&driver_cfg, &rs485_cfg) != 0U) {
    set_status_text("CFG SAVED");
  } else {
    set_status_text("CFG SAVE ERROR");
  }
  refresh_requested = 1U;
}

static void handle_field_packet(const char *packet)
{
  char field_text[PRINT_FIELD_CONTENT_LEN];
  const char *value;

  if(packet == 0) {
    return;
  }

  if(starts_with(packet, "cfg.") != 0U) {
    handle_config_packet(packet);
    return;
  }

  if(starts_with(packet, "title=") != 0U) {
    value = &packet[6];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_title(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "station=") != 0U) {
    uint16_t station = parse_u16(&packet[8], active_job.station_id);
    if(station >= 1U && station <= 10U) {
      (void)print_job_set_station(&active_job, (uint8_t)station);
    }
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "item=") != 0U) {
    value = &packet[5];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_item(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "content=") != 0U) {
    value = &packet[8];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_content(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "code=") != 0U) {
    value = &packet[5];
    (void)snprintf(field_text, sizeof(field_text), "%s", value);
    sanitize_ascii(field_text);
    (void)print_job_set_code(&active_job, field_text);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "qty=") != 0U) {
    active_job.quantity = parse_u16(&packet[4], active_job.quantity);
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "copies=") != 0U) {
    active_job.copies = (uint8_t)parse_u16(&packet[7], active_job.copies);
    if(active_job.copies == 0U) {
      active_job.copies = 1U;
    }
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "pass=") != 0U) {
    active_job.pass = (parse_u16(&packet[5], active_job.pass) == 0U) ? 0U : 1U;
    refresh_requested = 1U;
    return;
  }

  if(starts_with(packet, "result=") != 0U) {
    value = &packet[7];
    if(strcmp(value, "PASS") == 0 || strcmp(value, "pass") == 0 || strcmp(value, "1") == 0) {
      active_job.pass = 1U;
    } else {
      active_job.pass = 0U;
    }
    refresh_requested = 1U;
    return;
  }

  if(strcmp(packet, "preview") == 0) {
    g_print_terminal_state = PRINT_TERMINAL_STATE_PREVIEW;
    refresh_requested = 1U;
    return;
  }

  if(strcmp(packet, "print") == 0) {
    submit_current_print_job("PRINT OK");
    return;
  }

  if(strcmp(packet, "setup") == 0 || strcmp(packet, "settings") == 0) {
    (void)print_terminal_settings_begin(PRINT_TERMINAL_SETTINGS_LABEL,
                                         &active_job);
    return;
  }

  if(strcmp(packet, "default") == 0) {
    print_job_init_default(&active_job);
    g_print_terminal_state = PRINT_TERMINAL_STATE_EDIT;
    set_status_text("DEFAULT READY");
    refresh_requested = 1U;
    return;
  }

  if(strcmp(packet, "refresh") == 0 || starts_with(packet, "page=") != 0U) {
    refresh_requested = 1U;
  }
}

static void handle_direct_button(uint8_t button)
{
  if(button == 0U) {
    handle_field_packet("preview");
  } else if(button == 1U) {
    handle_field_packet("print");
  } else if(button == 2U) {
    (void)print_terminal_settings_begin_from_touch(
        PRINT_TERMINAL_SETTINGS_LABEL, &active_job);
  } else if(button == 3U) {
    set_status_text("READY");
    refresh_requested = 1U;
  }
}

static void handle_touch_event(const lcdm_tjc_event_t *event)
{
  if(event == 0 || event->touch_event == 0U) {
    return;
  }

  if(event->component_id == 1U) {
    handle_direct_button(0U);
  } else if(event->component_id == 2U) {
    handle_direct_button(1U);
  } else if(event->component_id == 3U) {
    handle_direct_button(2U);
  } else if(event->component_id >= 11U && event->component_id <= 14U) {
    handle_direct_button((uint8_t)(event->component_id - 11U));
  }
}

static void handle_touch_coordinate(const lcdm_tjc_event_t *event)
{
  uint8_t button;

  if(event == 0 || event->touch_event == 0U || event->x >= PRINT_LCDM_W) {
    return;
  }

  /* The header is an always-visible entry point for the editable label and
   * communication pages.  It avoids depending on hidden TFT component IDs. */
  if(event->y >= 32U && event->y < 64U) {
    (void)print_terminal_settings_begin_from_touch(
        PRINT_TERMINAL_SETTINGS_LABEL, &active_job);
    return;
  }
  if(event->y < PRINT_LCDM_KEY_Y) {
    return;
  }

  button = (uint8_t)(event->x / PRINT_LCDM_KEY_W);
  if(button < 4U) {
    handle_direct_button(button);
  }
}

static void apply_wifi_request(const print_host_wifi_request_t *request)
{
  char text[PRINT_FIELD_CONTENT_LEN];

  if(request == 0) {
    return;
  }
  if(request->station >= 1U && request->station <= 10U) {
    (void)print_job_set_station(&active_job, request->station);
  }
  if(request->quantity != 0U) {
    active_job.quantity = request->quantity;
  }
  active_job.pass = request->pass ? 1U : 0U;
  if(request->title[0] != '\0') {
    (void)snprintf(text, sizeof(text), "%s", request->title);
    sanitize_ascii(text);
    (void)print_job_set_title(&active_job, text);
  }
  if(request->item[0] != '\0') {
    (void)snprintf(text, sizeof(text), "%s", request->item);
    sanitize_ascii(text);
    (void)print_job_set_item(&active_job, text);
  }
  if(request->content[0] != '\0') {
    (void)snprintf(text, sizeof(text), "%s", request->content);
    sanitize_ascii(text);
    (void)print_job_set_content(&active_job, text);
  }
  if(request->code[0] != '\0') {
    (void)snprintf(text, sizeof(text), "%s", request->code);
    sanitize_ascii(text);
    (void)print_job_set_code(&active_job, text);
  }
}

static void service_wifi_print_requests(void)
{
  print_host_wifi_request_t request;

  while(print_host_wifi_poll_request(&request) != 0U) {
    apply_wifi_request(&request);
    if(print_driver_submit(&active_job) == PRINT_DRIVER_OK) {
      g_print_terminal_print_count++;
      g_print_terminal_state = PRINT_TERMINAL_STATE_PRINTED;
      set_status_text("WIFI PRINT OK");
      (void)print_host_wifi_send_done(&request);
    } else {
      g_print_terminal_state = PRINT_TERMINAL_STATE_ERROR;
      set_status_text("WIFI PRINT ERROR");
      (void)print_host_wifi_send_error(&request);
    }
    refresh_requested = 1U;
  }
}

void print_terminal_init(void)
{
  const print_terminal_store_config_t *store;

  g_print_terminal_state = PRINT_TERMINAL_STATE_EDIT;
  print_terminal_store_init();
  print_driver_init();
  print_terminal_store_apply_runtime();
  store = print_terminal_store_get();
  if(store == 0 || print_terminal_store_load_template(store->active_template,
                                                        &active_job) == 0U) {
    print_job_init_default(&active_job);
  }
  line_comm_transport_init(LINE_COMM_TRANSPORT_IR);
  print_terminal_settings_init();
  tester_wifi_print_init();
  print_host_wifi_init();
  print_host_wifi_start();
  lcdm_tjc_init();
  /* Match the proven tester startup path.  The panel may still be at a
   * factory/default baud after a power cycle, whereas the shared tester TFT
   * is operated at LCDM_TJC_BAUDRATE. */
  lcdm_tjc_force_baudrate(LCDM_TJC_BAUDRATE);
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("dim=100");
  /* The print page is drawn directly over the shared TFT, so it has no
   * hidden page components to consume touches.  Keep the panel touch engine
   * enabled for K1-K4 and the raw coordinate keyboard/settings UI. */
  lcdm_tjc_send_cmd("tsw 255,1");
  lcdm_tjc_page(PRINT_TERMINAL_PAGE_EDIT);
  /* The existing tester TFT already has the normal bottom touch band.  Raw
   * coordinate events let this direct-draw print page use it without a new
   * TFT download or hidden page components. */
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_frame_drawn = 0U;
  set_status_text("READY");
  send_job_to_lcdm();
}

void print_terminal_service(void)
{
  lcdm_tjc_event_t event;
  line_comm_print_request_t request;

  print_host_wifi_service();
  print_terminal_settings_set_network_status(print_host_wifi_status_text(),
                                             print_host_wifi_is_online() != 0U ?
                                             PRINT_LCDM_GREEN : PRINT_LCDM_BLUE);
  service_wifi_print_requests();

  if(print_terminal_settings_is_active() != 0U) {
    print_terminal_settings_service();
    if(print_terminal_settings_take_job(&active_job) != 0U) {
      g_print_terminal_state = PRINT_TERMINAL_STATE_EDIT;
      refresh_requested = 1U;
    }
    if(print_terminal_settings_take_return() != 0U) {
      lcdm_frame_drawn = 0U;
      refresh_requested = 1U;
    }
    if(refresh_requested != 0U && print_terminal_settings_is_active() == 0U) {
      refresh_requested = 0U;
      g_print_terminal_refresh_count++;
      send_job_to_lcdm();
    }
    return;
  }

  while(lcdm_tjc_poll_event(&event) != 0U) {
    g_print_terminal_event_count++;
    if(event.type == LCDM_TJC_EVENT_ASCII) {
      handle_field_packet(event.ascii);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH) {
      handle_touch_event(&event);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      handle_touch_coordinate(&event);
    }
    /* Stop interpreting the normal-page queue as soon as a touch opens the
     * settings overlay.  The settings module consumes any matching release
     * and debounces further sendxy=1 packets itself. */
    if(print_terminal_settings_is_active() != 0U) {
      break;
    }
  }

  /* The event that opened the settings overlay may have been followed by a
   * pending normal-page refresh request.  Do not draw the normal print page
   * over the freshly opened settings page in this same service pass. */
  if(print_terminal_settings_is_active() != 0U) {
    return;
  }

  if(print_terminal_store_get()->ir_fallback_enabled != 0U &&
     line_comm_transport_poll_print_request(&request, 1U) == LINE_COMM_OK) {
    g_print_terminal_ir_request_count++;
    if(request.source_station >= 1U && request.source_station <= 10U) {
      (void)print_job_set_station(&active_job, request.source_station);
    }
    active_job.pass = request.pass ? 1U : 0U;
    submit_current_print_job("IR PRINT");
    g_print_terminal_ir_print_count++;
  }

  if(refresh_requested != 0U) {
    refresh_requested = 0U;
    g_print_terminal_refresh_count++;
    send_job_to_lcdm();
  }
}
