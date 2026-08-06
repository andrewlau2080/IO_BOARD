#include "tester_settings.h"

#include "at32f45x_board.h"
#include "device_config.h"
#include "first_gen_display.h"
#include "lcdm_tjc.h"
#include "tester_wifi_print.h"

#include <stdio.h>
#include <string.h>

#define SETTINGS_W                    480U
#define SETTINGS_H                    272U
/* Keep the maintenance page on the same top-bar/key-strip grid as the
 * ordinary LCDM tester page: header 0..31 and keys 214..271. */
#define SETTINGS_TITLE_H               32U
#define SETTINGS_ID_Y                  34U
#define SETTINGS_ROW_H                 28U
#define SETTINGS_MACHINE_Y             63U
#define SETTINGS_STATION_Y             93U
#define SETTINGS_SSID_Y               123U
#define SETTINGS_PASSWORD_Y           153U
#define SETTINGS_STATUS_Y             184U
#define SETTINGS_STATUS_H              28U
#define SETTINGS_MAIN_KEY_Y           214U
#define SETTINGS_MAIN_KEY_H            58U
#define SETTINGS_KEYBOARD_KEY_Y       214U
#define SETTINGS_KEYBOARD_KEY_H        58U
#define SETTINGS_LEFT_X                 0U
#define SETTINGS_RIGHT_X              240U
#define SETTINGS_CELL_W               240U
#define SETTINGS_CELL_LABEL_W         112U
#define SETTINGS_CELL_VALUE_X         (SETTINGS_CELL_LABEL_W + 2U)
#define SETTINGS_CELL_VALUE_W         (SETTINGS_CELL_W - SETTINGS_CELL_VALUE_X - 2U)
#define SETTINGS_EDIT_BAR_Y            38U
#define SETTINGS_EDIT_BAR_H            30U
#define SETTINGS_EDIT_VALUE_X         132U
#define SETTINGS_EDIT_VALUE_W         332U
/* Font ID 4 is the existing 10 px Song resource; use its fixed ASCII cell
 * width to repaint only the changed suffix of the input string. */
#define SETTINGS_EDIT_CHAR_W            10U
#define SETTINGS_MAIN_VALUE_MAX        18U

#define SETTINGS_NAVY                 16U
#define SETTINGS_BLACK                0U
#define SETTINGS_BLUE                 31U
/* The ordinary setting confirmation stays bright green.  A verified WiFi
 * connection instead uses the exact dark-green background of AUTO PASS. */
#define SETTINGS_GREEN                FIRST_GEN_DISPLAY_COLOR_GREEN
#define SETTINGS_WIFI_PASS_GREEN      FIRST_GEN_DISPLAY_COLOR_AUTO_PASS_GREEN
#define SETTINGS_RED                  63488U
#define SETTINGS_WHITE                65535U
#define SETTINGS_GRAY                 33808U
#define SETTINGS_PALE_CYAN            49151U
#define SETTINGS_PALE_BLUE            50719U
#define SETTINGS_PALE_PINK            61503U
#define SETTINGS_ROW_BG               61374U
#define SETTINGS_DARK_GRAY            16904U
#define SETTINGS_SOFT_PINK            64593U
#define SETTINGS_MAGENTA              63519U
#define SETTINGS_ORANGE               64512U

#define SETTINGS_FONT_SMALL           4U
#define SETTINGS_FONT_TITLE           2U
/* The maintenance prompt must fit one line; use the same compact field font
 * instead of the larger result/status resource used for PASS pages. */
#define SETTINGS_FONT_STATUS          SETTINGS_FONT_SMALL

#define SETTINGS_WIFI_COMMAND_TIMEOUT_MS  2500U
#define SETTINGS_WIFI_JOIN_TIMEOUT_MS    20000U
/* DHCP can complete a little after AT+CWJAP returns OK.  Keep the IP query
 * phase alive long enough to poll CIFSR instead of treating the first
 * 0.0.0.0/empty reply as an authentication failure. */
#define SETTINGS_WIFI_IP_TIMEOUT_MS       10000U
#define SETTINGS_WIFI_IP_RETRY_MS           300U
/* Includes the three short setup commands, the AP association, DHCP/MAC and
 * an optional print-host connect. */
#define SETTINGS_WIFI_TEST_OVERALL_TIMEOUT_MS 60000U
#define SETTINGS_WIFI_COMMAND_MAX        192U
#define SETTINGS_STATUS_TEXT_MAX          64U
#define SETTINGS_INPUT_MAX DEVICE_CONFIG_WIFI_PASSWORD_MAX
#define SETTINGS_KEYBOARD_ROW1_Y           72U
#define SETTINGS_KEYBOARD_ROW2_Y          101U
#define SETTINGS_KEYBOARD_ROW3_Y          130U
#define SETTINGS_KEYBOARD_ROW_H            25U
#define SETTINGS_KEYBOARD_FUNCTION_Y      178U
#define SETTINGS_KEYBOARD_FUNCTION_H       28U

typedef enum {
  SETTINGS_FIELD_MACHINE = 0,
  SETTINGS_FIELD_LINE,
  SETTINGS_FIELD_STATION,
  SETTINGS_FIELD_SSID,
  SETTINGS_FIELD_PASSWORD,
  SETTINGS_FIELD_SERVICE_HOST,
  SETTINGS_FIELD_SERVICE_PORT,
  SETTINGS_FIELD_COUNT
} settings_field_t;

typedef enum {
  SETTINGS_WIFI_TEST_IDLE = 0,
  SETTINGS_WIFI_TEST_ECHO_OFF,
  SETTINGS_WIFI_TEST_CLOSE_OLD,
  SETTINGS_WIFI_TEST_AT,
  SETTINGS_WIFI_TEST_STA_MODE,
  SETTINGS_WIFI_TEST_JOIN,
  SETTINGS_WIFI_TEST_IP,
  SETTINGS_WIFI_TEST_MAC,
  SETTINGS_WIFI_TEST_TCP_START
} settings_wifi_test_state_t;

volatile uint8_t g_tester_settings_active;
volatile uint8_t g_tester_settings_wifi_test_running;
volatile uint8_t g_tester_settings_wifi_test_passed;
volatile uint32_t g_tester_settings_save_count;
volatile uint32_t g_tester_settings_save_error_count;

static device_config_t settings_draft;
static settings_field_t settings_field;
static settings_wifi_test_state_t settings_wifi_state;
static uint32_t settings_wifi_command_deadline_ms;
static uint32_t settings_wifi_overall_deadline_ms;
static uint32_t settings_wifi_ip_retry_due_ms;
static uint8_t settings_wifi_ip_retry_pending;
static uint8_t settings_wifi_raw_owned;
static uint8_t settings_wifi_got_ip;
static uint8_t settings_wifi_got_mac;
static uint8_t settings_wifi_link_ok;
static uint8_t settings_print_host_connected;
static uint8_t settings_wifi_link_failed;
static uint8_t settings_wifi_changed;
static uint8_t settings_reset_requested;
static uint8_t settings_input_active;
/* The page is enabled while the three-second K3 press is still down.  Ignore
 * that held press until its release packet arrives, or K3 immediately exits
 * the new page and produces an apparent endless flash. */
static uint8_t settings_touch_latched;
static uint8_t settings_keyboard_upper;
static uint8_t settings_keyboard_symbols;
static char settings_input[SETTINGS_INPUT_MAX];
static uint8_t settings_input_length;
static char settings_status[SETTINGS_STATUS_TEXT_MAX];
static uint16_t settings_status_color;
static char settings_ip[24];
static char settings_mac[24];
static char settings_port_text[8];
static uint8_t settings_main_page_drawn;
static uint8_t settings_main_row_cache_valid[SETTINGS_FIELD_COUNT];
static uint8_t settings_main_row_selected_cache[SETTINGS_FIELD_COUNT];
static uint16_t settings_main_row_color_cache[SETTINGS_FIELD_COUNT];
static uint16_t settings_main_row_background_cache[SETTINGS_FIELD_COUNT];
static char settings_main_row_text_cache[SETTINGS_FIELD_COUNT][SETTINGS_MAIN_VALUE_MAX];
static uint8_t settings_main_identity_cache_valid;
static char settings_main_mac_cache[SETTINGS_MAIN_VALUE_MAX];
static uint8_t settings_main_link_cache_valid;
static uint16_t settings_main_link_color_cache;
static uint16_t settings_main_link_background_cache;
static char settings_main_link_cache[SETTINGS_MAIN_VALUE_MAX];
static uint8_t settings_main_status_cache_valid;
static uint16_t settings_main_status_color_cache;
static char settings_main_status_cache[SETTINGS_STATUS_TEXT_MAX];
static uint8_t settings_keyboard_page_drawn;
static uint8_t settings_keyboard_mode_cache_valid;
static uint8_t settings_keyboard_upper_cache;
static uint8_t settings_keyboard_symbols_cache;
static uint8_t settings_keyboard_input_cache_valid;
static char settings_keyboard_input_cache[SETTINGS_INPUT_MAX];

static void settings_send_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char command[48];

  (void)snprintf(command,
                 sizeof(command),
                 "fill %u,%u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)color);
  /* Settings pages are persistent raster content, not animation.  Use the
   * same acknowledged serializer as the ordinary tester page so no fill or
   * text command can overtake the LCDM renderer. */
  lcdm_tjc_send_cmd(command);
}

/* Match the ordinary tester key face: a small white margin and the same
 * rounded-corner geometry used by first_gen_display.c. */
static void settings_send_circle(uint16_t x, uint16_t y, uint16_t radius,
                                 uint16_t color)
{
  char command[40];

  (void)snprintf(command,
                 sizeof(command),
                 "cirs %u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)radius,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(command);
}

static void settings_send_round_rect(uint16_t x, uint16_t y, uint16_t w,
                                     uint16_t h, uint16_t radius,
                                     uint16_t color)
{
  if(w <= (uint16_t)(radius * 2U) || h <= (uint16_t)(radius * 2U)) {
    settings_send_fill(x, y, w, h, color);
    return;
  }
  settings_send_fill((uint16_t)(x + radius), y,
                     (uint16_t)(w - (uint16_t)(radius * 2U)), h, color);
  settings_send_fill(x, (uint16_t)(y + radius), w,
                     (uint16_t)(h - (uint16_t)(radius * 2U)), color);
  settings_send_circle((uint16_t)(x + radius), (uint16_t)(y + radius),
                       radius, color);
  settings_send_circle((uint16_t)(x + w - radius - 1U),
                       (uint16_t)(y + radius), radius, color);
  settings_send_circle((uint16_t)(x + radius),
                       (uint16_t)(y + h - radius - 1U), radius, color);
  settings_send_circle((uint16_t)(x + w - radius - 1U),
                       (uint16_t)(y + h - radius - 1U), radius, color);
}

static void settings_escape_text(const char *text, char *out, uint8_t out_size)
{
  uint8_t out_index = 0U;

  if(out == 0 || out_size == 0U) {
    return;
  }
  if(text == 0) {
    text = "";
  }

  while(*text != '\0' && (uint8_t)(out_index + 1U) < out_size) {
    unsigned char value = (unsigned char)*text;

    if(value < 0x20U || value == 0x7FU) {
      text++;
      continue;
    }
    if((value == '"' || value == '\\') && (uint8_t)(out_index + 2U) >= out_size) {
      break;
    }
    if(value == '"' || value == '\\') {
      out[out_index] = '\\';
      out_index++;
    }
    out[out_index] = (char)value;
    out_index++;
    text++;
  }
  out[out_index] = '\0';
}

static void settings_send_text(uint16_t x,
                               uint16_t y,
                               uint16_t w,
                               uint16_t h,
                               uint16_t font,
                               uint16_t foreground,
                               uint16_t background,
                               uint8_t align,
                               const char *text)
{
  char safe_text[100];
  char command[196];

  settings_escape_text(text, safe_text, sizeof(safe_text));
  (void)snprintf(command,
                 sizeof(command),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)font,
                 (unsigned int)foreground,
                 (unsigned int)background,
                 (unsigned int)align,
                 safe_text);
  lcdm_tjc_send_cmd(command);
}

static void settings_short_text(const char *source, char *out, uint8_t out_size)
{
  uint8_t limit;
  uint8_t index;

  if(out == 0 || out_size == 0U) {
    return;
  }
  if(source == 0) {
    source = "";
  }

  limit = (uint8_t)(out_size - 1U);
  for(index = 0U; index < limit && source[index] != '\0'; index++) {
    out[index] = source[index];
  }
  if(source[index] != '\0' && limit >= 3U) {
    out[limit - 3U] = '.';
    out[limit - 2U] = '.';
    out[limit - 1U] = '.';
    out[limit] = '\0';
  } else {
    out[index] = '\0';
  }
}

static const char *settings_field_name(settings_field_t field)
{
  switch(field) {
  case SETTINGS_FIELD_MACHINE: return "MACHINE";
  case SETTINGS_FIELD_LINE: return "LINE";
  case SETTINGS_FIELD_STATION: return "STATION";
  case SETTINGS_FIELD_SSID: return "WIFI SSID";
  case SETTINGS_FIELD_PASSWORD: return "WIFI PASSWORD";
  case SETTINGS_FIELD_SERVICE_HOST: return "PRINT HOST";
  case SETTINGS_FIELD_SERVICE_PORT: return "PRINT PORT";
  default: return "FIELD";
  }
}

static const char *settings_field_label(settings_field_t field)
{
  switch(field) {
  /* These compact labels fit the existing 10 px Song font cell without
   * changing the production font resource or clipping the value column. */
  case SETTINGS_FIELD_MACHINE: return "MACHINE NO";
  case SETTINGS_FIELD_LINE: return "PD LINE";
  case SETTINGS_FIELD_PASSWORD: return "WIFI PWD";
  default: return settings_field_name(field);
  }
}

static void settings_set_status(const char *text, uint16_t color)
{
  if(text == 0) {
    text = "";
  }
  (void)snprintf(settings_status, sizeof(settings_status), "%s", text);
  settings_status_color = color;
}

/* Defined below the drawing helpers; the forward declaration keeps the
 * incremental renderer independent of the field-storage implementation. */
static const char *settings_field_value(settings_field_t field);

/* Main-page field geometry follows the reference router-style screen.  The
 * first row is reserved for the read-only hardware identity (UID/MAC), while
 * the four rows below contain the editable left/right pairs. */
static uint16_t settings_field_y(settings_field_t field)
{
  switch(field) {
  case SETTINGS_FIELD_MACHINE:
  case SETTINGS_FIELD_LINE:
    return SETTINGS_MACHINE_Y;
  case SETTINGS_FIELD_STATION:
    return SETTINGS_STATION_Y;
  case SETTINGS_FIELD_SSID:
  case SETTINGS_FIELD_SERVICE_HOST:
    return SETTINGS_SSID_Y;
  case SETTINGS_FIELD_PASSWORD:
  case SETTINGS_FIELD_SERVICE_PORT:
    return SETTINGS_PASSWORD_Y;
  default:
    return SETTINGS_MACHINE_Y;
  }
}

static uint16_t settings_field_x(settings_field_t field)
{
  switch(field) {
  case SETTINGS_FIELD_LINE:
  case SETTINGS_FIELD_SERVICE_HOST:
  case SETTINGS_FIELD_SERVICE_PORT:
    return SETTINGS_RIGHT_X;
  default:
    return SETTINGS_LEFT_X;
  }
}

static void settings_field_value_style(settings_field_t field,
                                       uint16_t *foreground,
                                       uint16_t *background)
{
  if(foreground == 0 || background == 0) {
    return;
  }

  /* Normal values are blue on white.  A successful WiFi/print check uses the
   * exact AUTO all-connection PASS style: white text on a dark-green cell. */
  *foreground = SETTINGS_BLUE;
  *background = SETTINGS_WHITE;
  if((field == SETTINGS_FIELD_SSID || field == SETTINGS_FIELD_PASSWORD) &&
     settings_wifi_link_ok != 0U) {
    *foreground = SETTINGS_WHITE;
    *background = SETTINGS_WIFI_PASS_GREEN;
  } else if((field == SETTINGS_FIELD_SERVICE_HOST ||
             field == SETTINGS_FIELD_SERVICE_PORT) &&
            settings_print_host_connected != 0U) {
    *foreground = SETTINGS_WHITE;
    *background = SETTINGS_WIFI_PASS_GREEN;
  }
}

static void settings_draw_cell_rule(uint16_t x, uint16_t y)
{
  settings_send_fill(x,
                     (uint16_t)(y + SETTINGS_ROW_H - 2U),
                     SETTINGS_CELL_W,
                     2U,
                     SETTINGS_NAVY);
}

/* Font ID 4 has a low underscore glyph that can sit on the cell baseline.
 * Draw that glyph explicitly after xstr so the row separator never erases it
 * and the character remains visible in names such as TP-LINK_56C928. */
static void settings_draw_visible_underscores(uint16_t x,
                                              uint16_t text_y,
                                              uint16_t line_offset,
                                              uint16_t color,
                                              const char *text)
{
  uint16_t index;

  if(text == 0) {
    return;
  }
  for(index = 0U; text[index] != '\0'; index++) {
    if(text[index] == '_') {
      settings_send_fill((uint16_t)(x + (index * SETTINGS_EDIT_CHAR_W)),
                         (uint16_t)(text_y + line_offset),
                         8U,
                         2U,
                         color);
    }
  }
}

static void settings_main_field_text(settings_field_t field, char *out, uint8_t out_size)
{
  const char *value = settings_field_value(field);

  if(out == 0 || out_size == 0U) {
    return;
  }
  settings_short_text(value, out, out_size);
}

static void settings_draw_main_field(settings_field_t field)
{
  uint16_t x;
  uint16_t y;
  uint16_t label_background;
  uint16_t value_foreground;
  uint16_t value_background;
  char value[SETTINGS_MAIN_VALUE_MAX];
  uint8_t selected;

  if(field >= SETTINGS_FIELD_COUNT) {
    return;
  }

  x = settings_field_x(field);
  y = settings_field_y(field);
  selected = (settings_field == field) ? 1U : 0U;
  label_background = (selected != 0U) ? SETTINGS_PALE_CYAN : SETTINGS_PALE_BLUE;
  settings_field_value_style(field, &value_foreground, &value_background);
  settings_main_field_text(field, value, sizeof(value));

  /* No TJC traffic is generated when the value, status colour, and selection
   * state are all unchanged.  This is what keeps a single typed character
   * from repainting the whole page. */
  if(settings_main_row_cache_valid[field] != 0U &&
     settings_main_row_selected_cache[field] == selected &&
     settings_main_row_color_cache[field] == value_foreground &&
     settings_main_row_background_cache[field] == value_background &&
     strcmp(settings_main_row_text_cache[field], value) == 0) {
    return;
  }

  settings_main_row_cache_valid[field] = 1U;
  settings_main_row_selected_cache[field] = selected;
  settings_main_row_color_cache[field] = value_foreground;
  settings_main_row_background_cache[field] = value_background;
  (void)snprintf(settings_main_row_text_cache[field],
                 sizeof(settings_main_row_text_cache[field]),
                 "%s",
                 value);

  settings_send_fill(x, y, SETTINGS_CELL_W, SETTINGS_ROW_H, SETTINGS_WHITE);
  settings_send_fill(x, y, SETTINGS_CELL_LABEL_W, SETTINGS_ROW_H,
                     label_background);
  settings_send_fill((uint16_t)(x + SETTINGS_CELL_VALUE_X), y,
                     SETTINGS_CELL_VALUE_W, SETTINGS_ROW_H,
                     value_background);
  settings_send_text((uint16_t)(x + 4U),
                     (uint16_t)(y + 6U),
                     SETTINGS_CELL_LABEL_W,
                     16U,
                     SETTINGS_FONT_SMALL,
                     SETTINGS_BLACK,
                     label_background,
                     0U,
                     settings_field_label(field));
  settings_send_text((uint16_t)(x + SETTINGS_CELL_VALUE_X),
                     (uint16_t)(y + 6U),
                     SETTINGS_CELL_VALUE_W,
                     16U,
                     SETTINGS_FONT_SMALL,
                     value_foreground,
                     value_background,
                     0U,
                     value);
  settings_draw_visible_underscores((uint16_t)(x + SETTINGS_CELL_VALUE_X),
                                    (uint16_t)(y + 6U),
                                    14U,
                                    value_foreground,
                                    value);
  settings_draw_cell_rule(x, y);
}

static void settings_draw_identity_cell(uint16_t x,
                                        const char *label,
                                        const char *value)
{
  settings_send_fill(x, SETTINGS_ID_Y, SETTINGS_CELL_W, SETTINGS_ROW_H,
                     SETTINGS_WHITE);
  settings_send_fill(x, SETTINGS_ID_Y, SETTINGS_CELL_LABEL_W, SETTINGS_ROW_H,
                     SETTINGS_PALE_BLUE);
  settings_send_fill((uint16_t)(x + SETTINGS_CELL_VALUE_X), SETTINGS_ID_Y,
                     SETTINGS_CELL_VALUE_W, SETTINGS_ROW_H, SETTINGS_WHITE);
  settings_send_text((uint16_t)(x + 4U), (uint16_t)(SETTINGS_ID_Y + 6U),
                     SETTINGS_CELL_LABEL_W, 16U, SETTINGS_FONT_SMALL,
                     SETTINGS_BLACK, SETTINGS_PALE_BLUE, 0U, label);
  settings_send_text((uint16_t)(x + SETTINGS_CELL_VALUE_X),
                     (uint16_t)(SETTINGS_ID_Y + 6U), SETTINGS_CELL_VALUE_W,
                     16U, SETTINGS_FONT_SMALL, SETTINGS_BLUE, SETTINGS_WHITE,
                     0U, value);
  settings_draw_cell_rule(x, SETTINGS_ID_Y);
}

static void settings_draw_main_identity(void)
{
  char uid[32];
  char uid_short[SETTINGS_MAIN_VALUE_MAX];
  char mac_short[SETTINGS_MAIN_VALUE_MAX];
  uint16_t mac_x = SETTINGS_RIGHT_X;

  device_config_format_uid(uid, sizeof(uid));
  settings_short_text(uid, uid_short, sizeof(uid_short));
  settings_short_text((settings_mac[0] == '\0') ? "--" : settings_mac,
                      mac_short,
                      sizeof(mac_short));

  if(settings_main_identity_cache_valid == 0U) {
    settings_draw_identity_cell(SETTINGS_LEFT_X, "UID", uid_short);
    settings_main_identity_cache_valid = 1U;
    (void)snprintf(settings_main_mac_cache,
                   sizeof(settings_main_mac_cache), "%s", mac_short);
    settings_draw_identity_cell(mac_x, "MAC", mac_short);
    return;
  }

  if(strcmp(settings_main_mac_cache, mac_short) != 0) {
    (void)snprintf(settings_main_mac_cache,
                   sizeof(settings_main_mac_cache), "%s", mac_short);
    settings_draw_identity_cell(mac_x, "MAC", mac_short);
  }
}

static void settings_main_link_text(char *out, uint8_t out_size,
                                    uint16_t *out_color)
{
  if(out == 0 || out_size == 0U || out_color == 0) {
    return;
  }
  if(g_tester_settings_wifi_test_running != 0U) {
    (void)snprintf(out, out_size, "TESTING");
    *out_color = SETTINGS_BLUE;
  } else if(settings_wifi_link_ok != 0U) {
    if(settings_ip[0] != '\0') {
      (void)snprintf(out, out_size, "OK %s", settings_ip);
    } else {
      (void)snprintf(out, out_size, "WIFI OK");
    }
    *out_color = SETTINGS_WIFI_PASS_GREEN;
  } else if(settings_wifi_link_failed != 0U) {
    (void)snprintf(out, out_size, "FAIL");
    *out_color = SETTINGS_RED;
  } else {
    (void)snprintf(out, out_size, "WAITING");
    *out_color = SETTINGS_NAVY;
  }
  settings_short_text(out, out, out_size);
}

static void settings_draw_main_link(void)
{
  char link[SETTINGS_MAIN_VALUE_MAX];
  uint16_t color;
  uint16_t background;
  uint16_t x = SETTINGS_RIGHT_X;
  uint16_t y = SETTINGS_STATION_Y;

  settings_main_link_text(link, sizeof(link), &color);
  background = (color == SETTINGS_WIFI_PASS_GREEN) ? SETTINGS_WIFI_PASS_GREEN : SETTINGS_WHITE;
  if(color == SETTINGS_WIFI_PASS_GREEN) {
    color = SETTINGS_WHITE;
  }
  if(settings_main_link_cache_valid != 0U &&
     settings_main_link_color_cache == color &&
     settings_main_link_background_cache == background &&
     strcmp(settings_main_link_cache, link) == 0) {
    return;
  }
  settings_main_link_cache_valid = 1U;
  settings_main_link_color_cache = color;
  settings_main_link_background_cache = background;
  (void)snprintf(settings_main_link_cache,
                 sizeof(settings_main_link_cache), "%s", link);
  settings_send_fill(x, y, SETTINGS_CELL_W, SETTINGS_ROW_H, SETTINGS_WHITE);
  settings_send_fill(x, y, SETTINGS_CELL_LABEL_W, SETTINGS_ROW_H,
                     SETTINGS_PALE_BLUE);
  settings_send_fill((uint16_t)(x + SETTINGS_CELL_VALUE_X), y,
                     SETTINGS_CELL_VALUE_W, SETTINGS_ROW_H, background);
  settings_send_text((uint16_t)(x + 4U), (uint16_t)(y + 6U),
                     SETTINGS_CELL_LABEL_W, 16U, SETTINGS_FONT_SMALL,
                     SETTINGS_BLACK, SETTINGS_PALE_BLUE, 0U, "WIFI LINK");
  settings_send_text((uint16_t)(x + SETTINGS_CELL_VALUE_X),
                     (uint16_t)(y + 6U), SETTINGS_CELL_VALUE_W, 16U,
                     SETTINGS_FONT_SMALL, color, background, 0U, link);
  settings_draw_cell_rule(x, y);
}

static void settings_draw_main_status(void)
{
  uint16_t background;
  uint16_t foreground;

  if(settings_status_color == SETTINGS_GREEN ||
     settings_status_color == SETTINGS_WIFI_PASS_GREEN) {
    background = settings_status_color;
    foreground = SETTINGS_WHITE;
  } else if(settings_status_color == SETTINGS_RED) {
    background = SETTINGS_RED;
    foreground = SETTINGS_WHITE;
  } else {
    background = SETTINGS_ROW_BG;
    foreground = settings_status_color;
  }

  if(settings_main_status_cache_valid != 0U &&
     settings_main_status_color_cache == settings_status_color &&
     strcmp(settings_main_status_cache, settings_status) == 0) {
    return;
  }
  settings_main_status_cache_valid = 1U;
  settings_main_status_color_cache = settings_status_color;
  (void)snprintf(settings_main_status_cache,
                 sizeof(settings_main_status_cache), "%s", settings_status);
  settings_send_fill(0U, SETTINGS_STATUS_Y, SETTINGS_W, SETTINGS_STATUS_H,
                     background);
  settings_send_text(8U, (uint16_t)(SETTINGS_STATUS_Y + 6U),
                     (uint16_t)(SETTINGS_W - 16U), 16U, SETTINGS_FONT_STATUS,
                     foreground, background, 1U, settings_status);
  settings_send_fill(0U,
                     (uint16_t)(SETTINGS_STATUS_Y + SETTINGS_STATUS_H - 2U),
                     SETTINGS_W,
                     2U,
                     SETTINGS_NAVY);
}

static void settings_draw_button_at(uint16_t x,
                                    uint16_t width,
                                    uint16_t y,
                                    uint16_t height,
                                    uint16_t color,
                                    const char *top,
                                    const char *bottom)
{
  settings_send_fill(x, y, width, height, SETTINGS_WHITE);
  settings_send_round_rect((uint16_t)(x + 2U),
                            (uint16_t)(y + 2U),
                            (uint16_t)(width - 4U),
                            (uint16_t)(height - 4U),
                            7U,
                            color);
  settings_send_text(x,
                     (uint16_t)(y + 5U),
                     width,
                     31U,
                     SETTINGS_FONT_TITLE,
                     SETTINGS_WHITE,
                     color,
                     1U,
                     top);
  settings_send_text(x,
                     (uint16_t)(y + 34U),
                     width,
                     19U,
                     SETTINGS_FONT_SMALL,
                     SETTINGS_WHITE,
                     color,
                     1U,
                     bottom);
}

static void settings_draw_button(uint16_t x,
                                 uint16_t width,
                                 uint16_t color,
                                 const char *top,
                                 const char *bottom)
{
  settings_draw_button_at(x, width, SETTINGS_MAIN_KEY_Y, SETTINGS_MAIN_KEY_H,
                          color, top, bottom);
}

static void settings_draw_main_buttons(void)
{
  /* Keep the original settings-page font resources: K1/K2/K3/K4 use the
   * title font for the key number and the small font for the caption. */
  settings_draw_button(0U, 120U, SETTINGS_BLUE, "K1", "SELF/LEARN");
  settings_draw_button(120U, 120U, SETTINGS_SOFT_PINK, "K2", "WIFI TEST");
  settings_draw_button(240U, 120U, SETTINGS_MAGENTA, "K3", "CANCEL/RESET");
  settings_draw_button(360U, 120U, SETTINGS_ORANGE, "K4", "OK/SAVE");
}

static void settings_draw_keyboard_buttons(void)
{
  settings_draw_button_at(0U, 120U, SETTINGS_KEYBOARD_KEY_Y,
                          SETTINGS_KEYBOARD_KEY_H, SETTINGS_BLUE, "K1", "SHIFT");
  settings_draw_button_at(120U, 120U, SETTINGS_KEYBOARD_KEY_Y,
                          SETTINGS_KEYBOARD_KEY_H, SETTINGS_SOFT_PINK, "K2", "ABC/123");
  settings_draw_button_at(240U, 120U, SETTINGS_KEYBOARD_KEY_Y,
                          SETTINGS_KEYBOARD_KEY_H, SETTINGS_MAGENTA, "K3", "CANCEL");
  settings_draw_button_at(360U, 120U, SETTINGS_KEYBOARD_KEY_Y,
                          SETTINGS_KEYBOARD_KEY_H, SETTINGS_ORANGE, "K4", "OK");
}

static void settings_draw_main_full(void)
{
  uint8_t field;

  /* The page is intentionally redrawn only on entry/return.  All subsequent
   * calls go through the per-cell caches below. */
  settings_main_page_drawn = 1U;
  memset(settings_main_row_cache_valid, 0, sizeof(settings_main_row_cache_valid));
  memset(settings_main_row_background_cache, 0, sizeof(settings_main_row_background_cache));
  settings_main_identity_cache_valid = 0U;
  settings_main_link_cache_valid = 0U;
  settings_main_link_background_cache = 0U;
  settings_main_status_cache_valid = 0U;
  settings_send_fill(0U, 0U, SETTINGS_W, SETTINGS_H, SETTINGS_WHITE);
  settings_send_fill(0U, 0U, SETTINGS_W, SETTINGS_TITLE_H, SETTINGS_NAVY);
  settings_send_text(0U,
                     1U,
                     SETTINGS_W,
                     30U,
                     SETTINGS_FONT_TITLE,
                     SETTINGS_WHITE,
                     SETTINGS_NAVY,
                     1U,
                     "WIFI SETUP");
  settings_draw_main_identity();
  for(field = 0U; field < SETTINGS_FIELD_COUNT; field++) {
    settings_draw_main_field((settings_field_t)field);
  }
  settings_draw_main_link();
  settings_draw_main_status();
  settings_draw_main_buttons();
}

static void settings_draw(void)
{
  uint8_t field;

  if(g_tester_settings_active == 0U || settings_input_active != 0U) {
    return;
  }
  if(settings_main_page_drawn == 0U) {
    settings_draw_main_full();
    return;
  }
  settings_draw_main_identity();
  for(field = 0U; field < SETTINGS_FIELD_COUNT; field++) {
    settings_draw_main_field((settings_field_t)field);
  }
  settings_draw_main_link();
  settings_draw_main_status();
}

/* The normal per-cell cache avoids repainting unchanged text while typing.
 * A completed WiFi transaction changes several dependent styles at once
 * (SSID/PASSWORD, link cell, status and sometimes MAC), so invalidate only
 * those cells before the one final result draw. */
static void settings_invalidate_wifi_presentation(void)
{
  settings_main_row_cache_valid[SETTINGS_FIELD_SSID] = 0U;
  settings_main_row_cache_valid[SETTINGS_FIELD_PASSWORD] = 0U;
  settings_main_row_cache_valid[SETTINGS_FIELD_SERVICE_HOST] = 0U;
  settings_main_row_cache_valid[SETTINGS_FIELD_SERVICE_PORT] = 0U;
  settings_main_identity_cache_valid = 0U;
  settings_main_link_cache_valid = 0U;
  settings_main_status_cache_valid = 0U;
}

static uint8_t settings_copy_text(char *destination, uint8_t destination_size, const char *value)
{
  uint8_t length = 0U;

  if(destination == 0 || destination_size == 0U || value == 0) {
    return 0U;
  }

  /* Validate the complete input before touching the draft.  A too-long LCDM
   * value must never leave a partially overwritten SSID or password in RAM. */
  while(value[length] != '\0') {
    unsigned char character = (unsigned char)value[length];

    if(character < 0x20U || character == 0x7FU) {
      return 0U;
    }
    length++;
    if(length >= destination_size) {
      return 0U;
    }
  }

  memcpy(destination, value, length);
  destination[length] = '\0';
  return 1U;
}

static uint8_t settings_parse_port(const char *text, uint16_t *out_port)
{
  uint32_t value = 0U;
  uint8_t digits = 0U;

  if(text == 0 || out_port == 0) {
    return 0U;
  }
  if(text[0] == '\0') {
    *out_port = 0U;
    return 1U;
  }

  while(*text != '\0') {
    if(*text < '0' || *text > '9') {
      return 0U;
    }
    value = (value * 10U) + (uint32_t)(*text - '0');
    if(value > 65535U) {
      return 0U;
    }
    text++;
    digits++;
    if(digits > 5U) {
      return 0U;
    }
  }

  if(digits == 0U) {
    return 0U;
  }
  *out_port = (uint16_t)value;
  return 1U;
}

static uint8_t settings_set_field(settings_field_t field, const char *value)
{
  char *destination = 0;
  uint8_t destination_size = 0U;
  uint16_t port;
  char status[SETTINGS_STATUS_TEXT_MAX];

  /* An external cfg.* command is an edit commit just like the keyboard's OK.
   * Leave the keyboard page before drawing the updated main cell. */
  if(settings_input_active != 0U) {
    settings_input_active = 0U;
    settings_keyboard_page_drawn = 0U;
    settings_main_page_drawn = 0U;
  }

  switch(field) {
  case SETTINGS_FIELD_MACHINE:
    destination = settings_draft.machine_id;
    destination_size = sizeof(settings_draft.machine_id);
    break;
  case SETTINGS_FIELD_LINE:
    destination = settings_draft.line_id;
    destination_size = sizeof(settings_draft.line_id);
    break;
  case SETTINGS_FIELD_STATION:
    destination = settings_draft.station_id;
    destination_size = sizeof(settings_draft.station_id);
    break;
  case SETTINGS_FIELD_SSID:
    destination = settings_draft.wifi_ssid;
    destination_size = sizeof(settings_draft.wifi_ssid);
    break;
  case SETTINGS_FIELD_PASSWORD:
    destination = settings_draft.wifi_password;
    destination_size = sizeof(settings_draft.wifi_password);
    break;
  case SETTINGS_FIELD_SERVICE_HOST:
    destination = settings_draft.service_host;
    destination_size = sizeof(settings_draft.service_host);
    break;
  case SETTINGS_FIELD_SERVICE_PORT:
    if(settings_parse_port(value, &port) == 0U) {
      (void)snprintf(status, sizeof(status), "INVALID %s INPUT", settings_field_name(field));
      settings_set_status(status, SETTINGS_RED);
      settings_draw();
      return 0U;
    }
    settings_draft.service_port = port;
    settings_wifi_changed = 1U;
    g_tester_settings_wifi_test_passed = 0U;
    settings_print_host_connected = 0U;
    (void)snprintf(status, sizeof(status), "%s UPDATED - K2 TEST, K4 SAVE",
                   settings_field_name(field));
    settings_set_status(status, SETTINGS_BLUE);
    settings_draw();
    return 1U;
  default:
    return 0U;
  }

  if(settings_copy_text(destination, destination_size, value) == 0U ||
     (field == SETTINGS_FIELD_MACHINE && destination[0] == '\0')) {
    (void)snprintf(status, sizeof(status), "INVALID %s INPUT", settings_field_name(field));
    settings_set_status(status, SETTINGS_RED);
    settings_draw();
    return 0U;
  }

  if(field == SETTINGS_FIELD_SSID || field == SETTINGS_FIELD_PASSWORD ||
     field == SETTINGS_FIELD_SERVICE_HOST) {
    settings_wifi_changed = 1U;
    g_tester_settings_wifi_test_passed = 0U;
    if(field == SETTINGS_FIELD_SSID || field == SETTINGS_FIELD_PASSWORD) {
      settings_wifi_got_ip = 0U;
      settings_wifi_link_ok = 0U;
      settings_wifi_link_failed = 0U;
      settings_print_host_connected = 0U;
      settings_ip[0] = '\0';
      /* The ESP module MAC identifies the module, not the AP.  Keep the last
       * validated MAC visible when an operator replaces SSID/password; it is
       * refreshed only by a successful MAC query. */
    } else {
      settings_print_host_connected = 0U;
    }
  }
  (void)snprintf(status, sizeof(status), "%s UPDATED - K4 TO SAVE", settings_field_name(field));
  settings_set_status(status, SETTINGS_BLUE);
  settings_draw();
  return 1U;
}

static void settings_select_field(settings_field_t field)
{
  char status[SETTINGS_STATUS_TEXT_MAX];

  if(field >= SETTINGS_FIELD_COUNT) {
    return;
  }
  if(settings_input_active != 0U) {
    settings_input_active = 0U;
    settings_keyboard_page_drawn = 0U;
    settings_main_page_drawn = 0U;
  }
  settings_field = field;
  (void)snprintf(status,
                 sizeof(status),
                 "%s SELECTED - TAP OR SEND cfg.value=...",
                 settings_field_name(field));
  settings_set_status(status, SETTINGS_BLUE);
  settings_draw();
}

static uint8_t settings_field_capacity(settings_field_t field)
{
  switch(field) {
  case SETTINGS_FIELD_MACHINE: return sizeof(settings_draft.machine_id);
  case SETTINGS_FIELD_LINE: return sizeof(settings_draft.line_id);
  case SETTINGS_FIELD_STATION: return sizeof(settings_draft.station_id);
  case SETTINGS_FIELD_SSID: return sizeof(settings_draft.wifi_ssid);
  case SETTINGS_FIELD_PASSWORD: return sizeof(settings_draft.wifi_password);
  case SETTINGS_FIELD_SERVICE_HOST: return sizeof(settings_draft.service_host);
  case SETTINGS_FIELD_SERVICE_PORT: return sizeof(settings_port_text);
  default: return 0U;
  }
}

static const char *settings_field_value(settings_field_t field)
{
  switch(field) {
  case SETTINGS_FIELD_MACHINE: return settings_draft.machine_id;
  case SETTINGS_FIELD_LINE: return settings_draft.line_id;
  case SETTINGS_FIELD_STATION: return settings_draft.station_id;
  case SETTINGS_FIELD_SSID: return settings_draft.wifi_ssid;
  case SETTINGS_FIELD_PASSWORD: return settings_draft.wifi_password;
  case SETTINGS_FIELD_SERVICE_HOST: return settings_draft.service_host;
  case SETTINGS_FIELD_SERVICE_PORT:
    if(settings_draft.service_port == 0U) {
      return "";
    }
    (void)snprintf(settings_port_text,
                   sizeof(settings_port_text),
                   "%u",
                   (unsigned int)settings_draft.service_port);
    return settings_port_text;
  default: return "";
  }
}

static void settings_draw_keyboard_row(const char *keys,
                                       uint8_t count,
                                       uint16_t y,
                                       uint16_t start_x,
                                       uint16_t key_width)
{
  uint8_t index;
  char label[2];

  for(index = 0U; index < count; index++) {
    uint16_t x = (uint16_t)(start_x + ((uint16_t)index * key_width));

    label[0] = keys[index];
    label[1] = '\0';
    settings_send_fill((uint16_t)(x + 1U), y, (uint16_t)(key_width - 2U),
                       SETTINGS_KEYBOARD_ROW_H, SETTINGS_PALE_BLUE);
    settings_send_text((uint16_t)(x + 1U),
                       (uint16_t)(y + 4U),
                       (uint16_t)(key_width - 2U),
                       17U,
                       SETTINGS_FONT_SMALL,
                       SETTINGS_NAVY,
                       SETTINGS_PALE_BLUE,
                       1U,
                       label);
  }
}

static void settings_draw_keyboard_key_rows(void)
{
  const char *row1;
  const char *row2;
  const char *row3;
  uint8_t row3_count;
  uint16_t row3_start;
  uint16_t row3_width;
  if(settings_input_active == 0U) {
    return;
  }

  if(settings_keyboard_symbols != 0U) {
    row1 = "1234567890";
    row2 = "!@#$%^&*";
    row3 = "-_=+().?";
    row3_count = 8U;
    row3_start = 24U;
    row3_width = 54U;
  } else {
    row1 = (settings_keyboard_upper != 0U) ? "QWERTYUIOP" : "qwertyuiop";
    row2 = (settings_keyboard_upper != 0U) ? "ASDFGHJKL" : "asdfghjkl";
    row3 = (settings_keyboard_upper != 0U) ? "ZXCVBNM" : "zxcvbnm";
    row3_count = 7U;
    row3_start = 30U;
    row3_width = 60U;
  }

  /* Only the keyboard rows are touched when ABC/123 or SHIFT changes. */
  settings_send_fill(0U, 68U, SETTINGS_W, 88U, SETTINGS_WHITE);
  settings_draw_keyboard_row(row1, 10U, SETTINGS_KEYBOARD_ROW1_Y, 0U, 48U);
  settings_draw_keyboard_row(row2, 9U, SETTINGS_KEYBOARD_ROW2_Y, 24U, 48U);
  settings_draw_keyboard_row(row3, row3_count, SETTINGS_KEYBOARD_ROW3_Y,
                             row3_start, row3_width);
}

static void settings_draw_keyboard_input(void)
{
  uint8_t common = 0U;
  uint8_t old_length = 0U;
  uint8_t new_length;
  uint16_t clear_x;
  uint16_t clear_w;
  uint8_t simple_append = 0U;
  uint8_t simple_backspace = 0U;
  char suffix[SETTINGS_INPUT_MAX];

  if(settings_input_active == 0U) {
    return;
  }
  if(settings_keyboard_input_cache_valid != 0U &&
     strcmp(settings_keyboard_input_cache, settings_input) == 0) {
    return;
  }

  new_length = (uint8_t)strlen(settings_input);
  if(settings_keyboard_input_cache_valid != 0U) {
    old_length = (uint8_t)strlen(settings_keyboard_input_cache);
    while(common < old_length && common < new_length &&
          settings_keyboard_input_cache[common] == settings_input[common]) {
      common++;
    }
    simple_append = (common == old_length && new_length == (uint8_t)(old_length + 1U)) ? 1U : 0U;
    simple_backspace = (common == new_length && old_length == (uint8_t)(new_length + 1U)) ? 1U : 0U;
  }
  clear_x = (uint16_t)(SETTINGS_EDIT_VALUE_X +
                       ((uint16_t)common * SETTINGS_EDIT_CHAR_W));
  if(clear_x > (uint16_t)(SETTINGS_EDIT_VALUE_X + SETTINGS_EDIT_VALUE_W)) {
    clear_x = (uint16_t)(SETTINGS_EDIT_VALUE_X + SETTINGS_EDIT_VALUE_W);
  }
  clear_w = (uint16_t)((SETTINGS_EDIT_VALUE_X + SETTINGS_EDIT_VALUE_W) - clear_x);
  if(settings_keyboard_input_cache_valid == 0U) {
    clear_x = SETTINGS_EDIT_VALUE_X;
    clear_w = SETTINGS_EDIT_VALUE_W;
  } else if(simple_append != 0U || simple_backspace != 0U) {
    clear_w = SETTINGS_EDIT_CHAR_W;
    if(clear_x + clear_w > (uint16_t)(SETTINGS_EDIT_VALUE_X + SETTINGS_EDIT_VALUE_W)) {
      clear_w = (uint16_t)((SETTINGS_EDIT_VALUE_X + SETTINGS_EDIT_VALUE_W) - clear_x);
    }
  }
  settings_keyboard_input_cache_valid = 1U;
  (void)snprintf(settings_keyboard_input_cache,
                 sizeof(settings_keyboard_input_cache), "%s", settings_input);
  /* Passwords are deliberately shown in clear text for production setup.
   * The redraw is confined to the value strip; the title, labels, keys and
   * all unchanged pixels remain untouched. */
  if(clear_w != 0U) {
    settings_send_fill(clear_x,
                       SETTINGS_EDIT_BAR_Y,
                       clear_w,
                       SETTINGS_EDIT_BAR_H,
                       SETTINGS_PALE_CYAN);
    (void)snprintf(suffix, sizeof(suffix), "%s", settings_input + common);
    settings_send_text(clear_x,
                       (uint16_t)(SETTINGS_EDIT_BAR_Y + 5U),
                       clear_w,
                       16U,
                       SETTINGS_FONT_SMALL,
                       SETTINGS_NAVY,
                       SETTINGS_PALE_CYAN,
                       0U,
                       suffix);
    settings_draw_visible_underscores(clear_x,
                                      (uint16_t)(SETTINGS_EDIT_BAR_Y + 5U),
                                      18U,
                                      SETTINGS_NAVY,
                                      suffix);
  }
}

static void settings_draw_keyboard_functions(void)
{
  static const char *const labels[] = {"CLEAR", "ABC/123", "SPACE", "BACK"};
  uint8_t index;

  for(index = 0U; index < 4U; index++) {
    uint16_t x = (uint16_t)(index * 120U);
    settings_send_fill(x, SETTINGS_KEYBOARD_FUNCTION_Y, 120U,
                       SETTINGS_KEYBOARD_FUNCTION_H, SETTINGS_ROW_BG);
    settings_send_text(x, (uint16_t)(SETTINGS_KEYBOARD_FUNCTION_Y + 6U),
                       120U, 17U, SETTINGS_FONT_SMALL, SETTINGS_NAVY,
                       SETTINGS_ROW_BG, 1U, labels[index]);
  }
}

static void settings_draw_keyboard_full(void)
{
  char title[48];

  if(settings_input_active == 0U) {
    return;
  }
  (void)snprintf(title, sizeof(title), "EDIT %s", settings_field_label(settings_field));
  settings_send_fill(0U, 0U, SETTINGS_W, SETTINGS_H, SETTINGS_WHITE);
  settings_send_fill(0U, 0U, SETTINGS_W, SETTINGS_TITLE_H, SETTINGS_NAVY);
  settings_send_text(0U, 1U, SETTINGS_W, 30U, SETTINGS_FONT_TITLE,
                     SETTINGS_WHITE, SETTINGS_NAVY, 1U, title);
  settings_send_fill(8U, SETTINGS_EDIT_BAR_Y, 464U, SETTINGS_EDIT_BAR_H,
                     SETTINGS_PALE_CYAN);
  settings_send_text(14U, (uint16_t)(SETTINGS_EDIT_BAR_Y + 5U),
                     112U, 16U, SETTINGS_FONT_SMALL, SETTINGS_BLUE,
                     SETTINGS_PALE_CYAN, 0U, settings_field_label(settings_field));
  settings_keyboard_page_drawn = 1U;
  settings_keyboard_mode_cache_valid = 1U;
  settings_keyboard_upper_cache = settings_keyboard_upper;
  settings_keyboard_symbols_cache = settings_keyboard_symbols;
  settings_keyboard_input_cache_valid = 0U;
  settings_draw_keyboard_key_rows();
  settings_draw_keyboard_functions();
  settings_draw_keyboard_buttons();
  settings_draw_keyboard_input();
}

static void settings_draw_keyboard(void)
{
  if(settings_input_active == 0U) {
    return;
  }
  if(settings_keyboard_page_drawn == 0U) {
    settings_draw_keyboard_full();
    return;
  }
  if(settings_keyboard_mode_cache_valid == 0U ||
     settings_keyboard_upper_cache != settings_keyboard_upper ||
     settings_keyboard_symbols_cache != settings_keyboard_symbols) {
    settings_keyboard_mode_cache_valid = 1U;
    settings_keyboard_upper_cache = settings_keyboard_upper;
    settings_keyboard_symbols_cache = settings_keyboard_symbols;
    settings_draw_keyboard_key_rows();
  }
  settings_draw_keyboard_input();
}

static void settings_begin_edit(settings_field_t field)
{
  const char *value;

  if(g_tester_settings_wifi_test_running != 0U || field >= SETTINGS_FIELD_COUNT) {
    return;
  }
  settings_field = field;
  value = settings_field_value(field);
  (void)snprintf(settings_input, sizeof(settings_input), "%s", value);
  settings_input_length = (uint8_t)strlen(settings_input);
  settings_keyboard_upper = 0U;
  settings_keyboard_symbols = 0U;
  settings_input_active = 1U;
  settings_keyboard_page_drawn = 0U;
  settings_keyboard_mode_cache_valid = 0U;
  settings_keyboard_input_cache_valid = 0U;
  settings_draw_keyboard();
}

static void settings_input_append(char value)
{
  uint8_t capacity = settings_field_capacity(settings_field);

  if(capacity == 0U || settings_input_length >= (uint8_t)(capacity - 1U)) {
    return;
  }
  settings_input[settings_input_length] = value;
  settings_input_length++;
  settings_input[settings_input_length] = '\0';
  settings_draw_keyboard();
}

static void settings_input_backspace(void)
{
  if(settings_input_length == 0U) {
    return;
  }
  settings_input_length--;
  settings_input[settings_input_length] = '\0';
  settings_draw_keyboard();
}

static void settings_input_clear(void)
{
  settings_input_length = 0U;
  settings_input[0] = '\0';
  settings_draw_keyboard();
}

static void settings_input_cancel(void)
{
  settings_input_active = 0U;
  settings_keyboard_page_drawn = 0U;
  settings_main_page_drawn = 0U;
  settings_set_status("EDIT CANCELLED", SETTINGS_BLUE);
  settings_draw();
}

static void settings_input_accept(void)
{
  settings_input_active = 0U;
  settings_keyboard_page_drawn = 0U;
  settings_main_page_drawn = 0U;
  (void)settings_set_field(settings_field, settings_input);
}

static uint8_t settings_keyboard_char(uint16_t x, uint16_t y, char *value)
{
  const char *keys;
  uint8_t count;
  uint16_t start_x;
  uint16_t key_width;
  uint8_t index;

  if(value == 0) {
    return 0U;
  }
  if(y >= SETTINGS_KEYBOARD_ROW1_Y && y < (uint16_t)(SETTINGS_KEYBOARD_ROW1_Y + SETTINGS_KEYBOARD_ROW_H)) {
    keys = (settings_keyboard_symbols != 0U) ? "1234567890" :
           ((settings_keyboard_upper != 0U) ? "QWERTYUIOP" : "qwertyuiop");
    count = 10U;
    start_x = 0U;
    key_width = 48U;
  } else if(y >= SETTINGS_KEYBOARD_ROW2_Y && y < (uint16_t)(SETTINGS_KEYBOARD_ROW2_Y + SETTINGS_KEYBOARD_ROW_H)) {
    keys = (settings_keyboard_symbols != 0U) ? "!@#$%^&*" :
           ((settings_keyboard_upper != 0U) ? "ASDFGHJKL" : "asdfghjkl");
    count = 9U;
    start_x = 24U;
    key_width = 48U;
  } else if(y >= SETTINGS_KEYBOARD_ROW3_Y && y < (uint16_t)(SETTINGS_KEYBOARD_ROW3_Y + SETTINGS_KEYBOARD_ROW_H)) {
    keys = (settings_keyboard_symbols != 0U) ? "-_=+().?" :
           ((settings_keyboard_upper != 0U) ? "ZXCVBNM" : "zxcvbnm");
    count = (settings_keyboard_symbols != 0U) ? 8U : 7U;
    start_x = (settings_keyboard_symbols != 0U) ? 24U : 30U;
    key_width = (settings_keyboard_symbols != 0U) ? 54U : 60U;
  } else {
    return 0U;
  }

  if(x < start_x) {
    return 0U;
  }
  index = (uint8_t)((x - start_x) / key_width);
  if(index >= count) {
    return 0U;
  }
  *value = keys[index];
  return 1U;
}

static void settings_handle_keyboard_coordinate(uint16_t x, uint16_t y, uint8_t event)
{
  char value;

  if(event == 0U) {
    return;
  }
  if(settings_keyboard_char(x, y, &value) != 0U) {
    settings_input_append(value);
    return;
  }
  if(y >= SETTINGS_KEYBOARD_FUNCTION_Y &&
     y < (uint16_t)(SETTINGS_KEYBOARD_FUNCTION_Y + SETTINGS_KEYBOARD_FUNCTION_H)) {
    if(x < 120U) {
      settings_input_clear();
    } else if(x < 240U) {
      settings_keyboard_symbols ^= 1U;
      settings_draw_keyboard();
    } else if(x < 360U) {
      settings_input_append(' ');
    } else {
      settings_input_backspace();
    }
    return;
  }
  if(y >= SETTINGS_KEYBOARD_KEY_Y &&
     y < (uint16_t)(SETTINGS_KEYBOARD_KEY_Y + SETTINGS_KEYBOARD_KEY_H)) {
    if(x < 120U) {
      settings_keyboard_upper ^= 1U;
      settings_draw_keyboard();
    } else if(x < 240U) {
      settings_keyboard_symbols ^= 1U;
      settings_draw_keyboard();
    } else if(x < 360U) {
      settings_input_cancel();
    } else {
      settings_input_accept();
    }
  }
}

static uint8_t settings_append_char(char *text, uint16_t text_size, uint16_t *length, char value)
{
  if(text == 0 || length == 0 || (*length + 1U) >= text_size) {
    return 0U;
  }
  text[*length] = value;
  *length = (uint16_t)(*length + 1U);
  text[*length] = '\0';
  return 1U;
}

static uint8_t settings_append_escaped(char *text,
                                       uint16_t text_size,
                                       uint16_t *length,
                                       const char *value)
{
  if(value == 0) {
    return 0U;
  }

  while(*value != '\0') {
    if(*value == '\r' || *value == '\n') {
      return 0U;
    }
    if(*value == '"' || *value == '\\') {
      if(settings_append_char(text, text_size, length, '\\') == 0U) {
        return 0U;
      }
    }
    if(settings_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }
  return 1U;
}

static uint8_t settings_build_join_command(char command[SETTINGS_WIFI_COMMAND_MAX])
{
  uint16_t length = 0U;
  static const char prefix[] = "AT+CWJAP=\"";
  static const char separator[] = "\",\"";
  static const char suffix[] = "\"";

  command[0] = '\0';
  for(uint8_t i = 0U; i < (uint8_t)(sizeof(prefix) - 1U); i++) {
    if(settings_append_char(command, SETTINGS_WIFI_COMMAND_MAX, &length, prefix[i]) == 0U) {
      return 0U;
    }
  }
  if(settings_append_escaped(command, SETTINGS_WIFI_COMMAND_MAX, &length, settings_draft.wifi_ssid) == 0U) {
    return 0U;
  }
  for(uint8_t i = 0U; i < (uint8_t)(sizeof(separator) - 1U); i++) {
    if(settings_append_char(command, SETTINGS_WIFI_COMMAND_MAX, &length, separator[i]) == 0U) {
      return 0U;
    }
  }
  if(settings_append_escaped(command, SETTINGS_WIFI_COMMAND_MAX, &length, settings_draft.wifi_password) == 0U) {
    return 0U;
  }
  for(uint8_t i = 0U; i < (uint8_t)(sizeof(suffix) - 1U); i++) {
    if(settings_append_char(command, SETTINGS_WIFI_COMMAND_MAX, &length, suffix[i]) == 0U) {
      return 0U;
    }
  }
  return 1U;
}

static uint8_t settings_append_uint32(char *text,
                                      uint16_t text_size,
                                      uint16_t *length,
                                      uint32_t value)
{
  char number[12];
  int written;
  uint8_t index;

  written = snprintf(number, sizeof(number), "%lu", (unsigned long)value);
  if(written <= 0 || (uint32_t)written >= sizeof(number)) {
    return 0U;
  }
  for(index = 0U; index < (uint8_t)written; index++) {
    if(settings_append_char(text, text_size, length, number[index]) == 0U) {
      return 0U;
    }
  }
  return 1U;
}

static uint8_t settings_has_print_host(void)
{
  return (settings_draft.service_host[0] != '\0' &&
          settings_draft.service_port != 0U) ? 1U : 0U;
}

static void settings_load_saved_mac(void)
{
  if(device_config_get_wifi_mac(settings_mac, sizeof(settings_mac)) == 0U) {
    settings_mac[0] = '\0';
  }
}

static uint8_t settings_persist_read_mac(void)
{
  char saved_mac[DEVICE_CONFIG_WIFI_MAC_TEXT_MAX];

  if(settings_wifi_got_mac == 0U || settings_mac[0] == '\0') {
    return 1U;
  }

  /* Avoid another sector erase when the module reports the same MAC that is
   * already stored.  A newly read/replaced module is committed immediately
   * after the test sequence completes, independently of K4's editable-field
   * save decision. */
  if(device_config_get_wifi_mac(saved_mac, sizeof(saved_mac)) != 0U &&
     strcmp(saved_mac, settings_mac) == 0) {
    return 1U;
  }

  return device_config_save_wifi_mac(settings_mac);
}

static uint8_t settings_print_host_pair_is_valid(void)
{
  return ((settings_draft.service_host[0] == '\0' &&
           settings_draft.service_port == 0U) ||
          settings_has_print_host() != 0U) ? 1U : 0U;
}

static uint8_t settings_build_tcp_start_command(char command[SETTINGS_WIFI_COMMAND_MAX])
{
  uint16_t length = 0U;
  static const char prefix[] = "AT+CIPSTART=\"TCP\",\"";
  static const char separator[] = "\",";
  uint8_t index;

  if(settings_has_print_host() == 0U) {
    return 0U;
  }

  command[0] = '\0';
  for(index = 0U; index < (uint8_t)(sizeof(prefix) - 1U); index++) {
    if(settings_append_char(command, SETTINGS_WIFI_COMMAND_MAX, &length, prefix[index]) == 0U) {
      return 0U;
    }
  }
  if(settings_append_escaped(command,
                             SETTINGS_WIFI_COMMAND_MAX,
                             &length,
                             settings_draft.service_host) == 0U) {
    return 0U;
  }
  for(index = 0U; index < (uint8_t)(sizeof(separator) - 1U); index++) {
    if(settings_append_char(command, SETTINGS_WIFI_COMMAND_MAX, &length, separator[index]) == 0U) {
      return 0U;
    }
  }
  return settings_append_uint32(command,
                                SETTINGS_WIFI_COMMAND_MAX,
                                &length,
                                settings_draft.service_port);
}

static void settings_finish_wifi_test(uint8_t passed, const char *detail)
{
  char status[SETTINGS_STATUS_TEXT_MAX];
  uint8_t mac_save_ok;

  /* The final MAC line has already been received by the time this function is
   * entered.  Commit it while raw AT ownership is still held, so the next K3
   * entry can show it even if the operator does not press K4. */
  mac_save_ok = settings_persist_read_mac();

  /* Do not hand the UART back to the production reconnect state machine while
   * this page is still visible.  That state machine used to immediately send
   * AT+CWMODE/AT+CWJAP after the final MAC line, racing the next K2 press and
   * making the green result appear only after leaving/re-entering the page. */
  if(settings_wifi_raw_owned != 0U) {
    tester_wifi_print_at_end_hold();
  }
  settings_wifi_ip_retry_pending = 0U;
  settings_wifi_command_deadline_ms = 0U;
  settings_wifi_overall_deadline_ms = 0U;
  g_tester_settings_wifi_test_running = 0U;
  settings_wifi_state = SETTINGS_WIFI_TEST_IDLE;
  if(passed != 0U) {
    g_tester_settings_wifi_test_passed = 1U;
    settings_wifi_link_ok = 1U;
    settings_wifi_link_failed = 0U;
    if(settings_has_print_host() != 0U) {
      settings_print_host_connected = 1U;
    }
    (void)snprintf(status, sizeof(status), "NETWORK PASS %s - K4 SAVE", detail == 0 ? "" : detail);
    settings_set_status(status, SETTINGS_WIFI_PASS_GREEN);
  } else {
    g_tester_settings_wifi_test_passed = 0U;
    /* A failed final step must clear any provisional IP/MAC-stage green
     * styling.  Otherwise the red status and a stale green cell coexist until
     * the page is opened again. */
    settings_wifi_link_ok = 0U;
    settings_wifi_link_failed = 1U;
    settings_print_host_connected = 0U;
    (void)snprintf(status, sizeof(status), "NETWORK FAIL %s - K2 RETRY", detail == 0 ? "" : detail);
    settings_set_status(status, SETTINGS_RED);
  }
  if(mac_save_ok == 0U) {
    g_tester_settings_wifi_test_passed = 0U;
    settings_wifi_link_ok = 0U;
    settings_wifi_link_failed = 1U;
    settings_print_host_connected = 0U;
    settings_set_status("MAC SAVE FAIL - K3 RETRY", SETTINGS_RED);
  }
  settings_invalidate_wifi_presentation();
  settings_draw();
}

static void settings_issue_wifi_command(settings_wifi_test_state_t state)
{
  const char *command = 0;
  char join_command[SETTINGS_WIFI_COMMAND_MAX];
  char tcp_start_command[SETTINGS_WIFI_COMMAND_MAX];
  uint32_t timeout_ms;

  switch(state) {
  case SETTINGS_WIFI_TEST_ECHO_OFF:
    /* ATE0 first: with the ESP echo disabled, no received byte can overlap
     * the software-UART TX window, so every later response decodes cleanly
     * and the test no longer needs repeated presses to pass. */
    command = "ATE0";
    settings_set_status("NETWORK TEST: DISABLE ECHO", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_CLOSE_OLD:
    command = "AT+CIPCLOSE";
    settings_set_status("NETWORK TEST: CLOSE OLD SESSION", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_AT:
    command = "AT";
    settings_set_status("NETWORK TEST: CHECK ESP-AT", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_STA_MODE:
    command = "AT+CWMODE=1";
    settings_set_status("NETWORK TEST: SET STATION MODE", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_JOIN:
    if(settings_build_join_command(join_command) == 0U) {
      settings_finish_wifi_test(0U, "SSID/PASSWORD INVALID");
      return;
    }
    command = join_command;
    settings_set_status("NETWORK TEST: JOINING AP", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_IP:
    command = "AT+CIFSR";
    settings_set_status("NETWORK TEST: READ IP", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_MAC:
    command = "AT+CIPSTAMAC?";
    settings_set_status("NETWORK TEST: READ MAC", SETTINGS_BLUE);
    break;
  case SETTINGS_WIFI_TEST_TCP_START:
    if(settings_build_tcp_start_command(tcp_start_command) == 0U) {
      settings_finish_wifi_test(0U, "PRINT HOST/PORT INVALID");
      return;
    }
    command = tcp_start_command;
    settings_set_status("NETWORK TEST: CONNECT PRINT HOST", SETTINGS_BLUE);
    break;
  default:
    settings_finish_wifi_test(0U, "INTERNAL STATE");
    return;
  }

  timeout_ms = (state == SETTINGS_WIFI_TEST_JOIN) ? SETTINGS_WIFI_JOIN_TIMEOUT_MS :
               ((state == SETTINGS_WIFI_TEST_IP) ? SETTINGS_WIFI_IP_TIMEOUT_MS :
                SETTINGS_WIFI_COMMAND_TIMEOUT_MS);
  settings_wifi_state = state;
  if(state == SETTINGS_WIFI_TEST_IP) {
    settings_wifi_ip_retry_pending = 0U;
  }
  settings_draw();
  if(tester_wifi_print_at_send(command) == 0U) {
    settings_finish_wifi_test(0U, "ESP UART NOT READY");
    return;
  }
  settings_wifi_command_deadline_ms = tester_wifi_print_now_ms() + timeout_ms;
}

static void settings_start_wifi_test(void)
{
  if(g_tester_settings_wifi_test_running != 0U) {
    return;
  }
  if(settings_draft.wifi_ssid[0] == '\0') {
    settings_set_status("ENTER WIFI SSID FIRST", SETTINGS_RED);
    settings_draw();
    return;
  }
  if(settings_print_host_pair_is_valid() == 0U) {
    settings_set_status("ENTER BOTH PRINT HOST AND PORT", SETTINGS_RED);
    settings_draw();
    return;
  }

  g_tester_settings_wifi_test_passed = 0U;
  settings_wifi_got_ip = 0U;
  settings_wifi_got_mac = 0U;
  settings_wifi_link_ok = 0U;
  settings_wifi_link_failed = 0U;
  settings_print_host_connected = 0U;
  settings_ip[0] = '\0';
  settings_invalidate_wifi_presentation();
  tester_wifi_print_at_begin();
  settings_wifi_raw_owned = 1U;
  g_tester_settings_wifi_test_running = 1U;
  settings_wifi_ip_retry_pending = 0U;
  settings_wifi_command_deadline_ms = 0U;
  settings_wifi_overall_deadline_ms = tester_wifi_print_now_ms() +
                                      SETTINGS_WIFI_TEST_OVERALL_TIMEOUT_MS;
  settings_issue_wifi_command(SETTINGS_WIFI_TEST_ECHO_OFF);
}

static void settings_copy_quoted(const char *line, char *out, uint8_t out_size)
{
  const char *start;
  const char *end;
  uint8_t length;

  if(line == 0 || out == 0 || out_size == 0U) {
    return;
  }
  start = strchr(line, '"');
  if(start == 0) {
    return;
  }
  end = strchr(start + 1, '"');
  if(end == 0) {
    return;
  }
  length = (uint8_t)(end - (start + 1));
  if(length >= out_size) {
    length = (uint8_t)(out_size - 1U);
  }
  memcpy(out, start + 1, length);
  out[length] = '\0';
}

static void settings_process_wifi_ok(void)
{
  switch(settings_wifi_state) {
  case SETTINGS_WIFI_TEST_ECHO_OFF:
    settings_issue_wifi_command(SETTINGS_WIFI_TEST_CLOSE_OLD);
    break;
  case SETTINGS_WIFI_TEST_CLOSE_OLD:
    settings_issue_wifi_command(SETTINGS_WIFI_TEST_AT);
    break;
  case SETTINGS_WIFI_TEST_AT:
    settings_issue_wifi_command(SETTINGS_WIFI_TEST_STA_MODE);
    break;
  case SETTINGS_WIFI_TEST_STA_MODE:
    settings_issue_wifi_command(SETTINGS_WIFI_TEST_JOIN);
    break;
  case SETTINGS_WIFI_TEST_JOIN:
    settings_issue_wifi_command(SETTINGS_WIFI_TEST_IP);
    break;
  case SETTINGS_WIFI_TEST_IP:
    if(settings_wifi_got_ip != 0U && settings_ip[0] != '\0' &&
       strcmp(settings_ip, "0.0.0.0") != 0) {
      settings_issue_wifi_command(SETTINGS_WIFI_TEST_MAC);
    } else {
      /* ESP-AT can acknowledge CWJAP before the DHCP lease is visible to
       * CIFSR.  Do not reject a valid AP merely because the first query is
       * early; service() reissues CIFSR after a short, bounded wait. */
      settings_wifi_ip_retry_pending = 1U;
      settings_wifi_ip_retry_due_ms = tester_wifi_print_now_ms() +
                                      SETTINGS_WIFI_IP_RETRY_MS;
      settings_set_status("NETWORK TEST: WAIT DHCP / RETRY IP", SETTINGS_BLUE);
      settings_draw();
    }
    break;
  case SETTINGS_WIFI_TEST_MAC:
    if(settings_has_print_host() != 0U) {
      settings_issue_wifi_command(SETTINGS_WIFI_TEST_TCP_START);
    } else {
      settings_finish_wifi_test(1U, settings_ip);
    }
    break;
  case SETTINGS_WIFI_TEST_TCP_START:
    settings_finish_wifi_test(1U, "PRINT HOST OK");
    break;
  default:
    settings_finish_wifi_test(0U, "UNEXPECTED OK");
    break;
  }
}

static void settings_process_wifi_line(const char *line)
{
  if(line == 0 || line[0] == '\0') {
    return;
  }
  if(strstr(line, "invalid header: 0xffffffff") != 0) {
    settings_finish_wifi_test(0U, "ESP FLASH EMPTY");
    return;
  }
  if(strstr(line, "+CIFSR:STAIP") != 0 ||
     strstr(line, "+CIPSTA:ip") != 0) {
    settings_copy_quoted(line, settings_ip, sizeof(settings_ip));
    settings_wifi_got_ip = (settings_ip[0] != '\0' &&
                            strcmp(settings_ip, "0.0.0.0") != 0) ? 1U : 0U;
    if(settings_wifi_got_ip != 0U &&
       settings_wifi_state == SETTINGS_WIFI_TEST_IP &&
       settings_wifi_ip_retry_pending != 0U) {
      /* The IP line may arrive immediately after the preceding OK.  Let the
       * pending retry run now, so its next OK advances cleanly to MAC. */
      settings_wifi_ip_retry_due_ms = tester_wifi_print_now_ms();
    }
    return;
  }
  if(strstr(line, "+CIPSTAMAC") != 0 ||
     strstr(line, "+CIFSR:STAMAC") != 0 ||
     strstr(line, "+CIPSTA:mac") != 0) {
    settings_copy_quoted(line, settings_mac, sizeof(settings_mac));
    settings_wifi_got_mac = (settings_mac[0] != '\0') ? 1U : 0U;
    /* The final PASS draw updates this one read-only cell once.  Redrawing
     * midway through an ESP response was the source of the visible flash. */
    if(settings_wifi_got_mac != 0U &&
       settings_wifi_state == SETTINGS_WIFI_TEST_MAC) {
      /* The MAC response itself proves that STA is usable.  Completing here
       * avoids a false timeout when a noisy 8 MHz software-UART capture loses
       * only the trailing OK line. */
      if(settings_has_print_host() == 0U) {
        settings_finish_wifi_test(1U, settings_ip);
      }
    }
    return;
  }
  if(strstr(line, "WIFI GOT IP") != 0) {
    settings_wifi_got_ip = 1U;
    if(settings_wifi_state == SETTINGS_WIFI_TEST_JOIN) {
      /* ESP-AT normally follows this indication with OK.  If that one small
       * line is lost, fall through to CIFSR shortly instead of waiting the
       * entire join timeout and declaring a healthy AP dead. */
      settings_wifi_command_deadline_ms = tester_wifi_print_now_ms() +
                                          SETTINGS_WIFI_IP_RETRY_MS;
    }
    if(settings_wifi_state == SETTINGS_WIFI_TEST_IP &&
       settings_wifi_ip_retry_pending != 0U) {
      settings_wifi_ip_retry_due_ms = tester_wifi_print_now_ms();
    }
    return;
  }
  if(strncmp(line, "+CWJAP:", 7U) == 0 &&
     settings_wifi_state == SETTINGS_WIFI_TEST_JOIN) {
    /* ESP-AT's +CWJAP:<status> indications are terminal association errors
     * (wrong key, AP not found, timeout, ...), unlike WIFI DISCONNECT which
     * can be a transient pre-join notification. */
    settings_finish_wifi_test(0U, "CHECK SSID/PASSWORD");
    return;
  }
  if(strcmp(line, "ERROR") == 0 || strstr(line, "FAIL") != 0) {
    if(settings_wifi_state == SETTINGS_WIFI_TEST_CLOSE_OLD ||
       settings_wifi_state == SETTINGS_WIFI_TEST_ECHO_OFF) {
      settings_process_wifi_ok();
      return;
    }
    /* Older ESP-AT builds may not implement the MAC query.  The AP/IP test
     * is still valid, so continue to the optional print-host connection. */
    if(settings_wifi_state == SETTINGS_WIFI_TEST_MAC) {
      settings_process_wifi_ok();
      return;
    }
    if(settings_wifi_state == SETTINGS_WIFI_TEST_IP) {
      settings_wifi_ip_retry_pending = 1U;
      settings_wifi_ip_retry_due_ms = tester_wifi_print_now_ms() +
                                      SETTINGS_WIFI_IP_RETRY_MS;
      settings_set_status("NETWORK TEST: WAIT DHCP / RETRY IP", SETTINGS_BLUE);
      settings_draw();
      return;
    }
    settings_finish_wifi_test(0U,
                              (settings_wifi_state == SETTINGS_WIFI_TEST_JOIN) ?
                              "CHECK SSID/PASSWORD" :
                              ((settings_wifi_state == SETTINGS_WIFI_TEST_TCP_START) ?
                               "CHECK PRINT HOST/PORT" : "ESP-AT ERROR"));
    return;
  }
  if(strcmp(line, "OK") == 0) {
    settings_process_wifi_ok();
  }
}

static void settings_save(void)
{
  if(g_tester_settings_wifi_test_running != 0U) {
    return;
  }
  if(device_config_save(&settings_draft) == 0U) {
    g_tester_settings_save_error_count++;
    settings_set_status("CHECK HOST/PORT THEN SAVE", SETTINGS_RED);
    settings_draw();
    return;
  }

  g_tester_settings_save_count++;
  /* Editable WiFi parameters are allowed to be replaced even when the AP is
   * currently out of range.  The new record is the operator's source of
   * truth; K2 remains the explicit immediate verification action. */
  settings_wifi_changed = 0U;
  /* restart() records the intent while raw AT ownership is held; the actual
   * saved-session reconnect begins only at K3 exit. */
  tester_wifi_print_restart();
  if(g_tester_settings_wifi_test_passed != 0U) {
    settings_set_status("SAVED - K3 RETURNS TO TESTER", SETTINGS_WIFI_PASS_GREEN);
  } else {
    settings_set_status("SAVED - K2 TEST OR K3 EXIT", SETTINGS_BLUE);
  }
  settings_draw();
}

static void settings_request_exit(void)
{
  if(settings_wifi_raw_owned != 0U) {
    /* Release the one ESP-AT owner only after the page is gone.  This starts
     * the saved production session from Flash and prevents a test-page retry
     * from interleaving with it. */
    tester_wifi_print_at_resume();
    settings_wifi_raw_owned = 0U;
  }
  settings_wifi_ip_retry_pending = 0U;
  settings_wifi_command_deadline_ms = 0U;
  settings_wifi_overall_deadline_ms = 0U;
  g_tester_settings_wifi_test_running = 0U;
  settings_wifi_state = SETTINGS_WIFI_TEST_IDLE;
  /* Cancelling after a successful trial join must restore the AP held in
   * Flash, rather than silently keeping an unsaved test network alive. */
  if(settings_wifi_changed != 0U) {
    g_tester_settings_wifi_test_passed = 0U;
  }
  settings_input_active = 0U;
  settings_touch_latched = 0U;
  g_tester_settings_active = 0U;
  settings_reset_requested = 1U;
}

static void settings_handle_coordinate(uint16_t x, uint16_t y, uint8_t event)
{
  settings_field_t touched_field = SETTINGS_FIELD_COUNT;

  if(event == 0U) {
    settings_touch_latched = 0U;
    return;
  }
  if(x >= SETTINGS_W || y >= SETTINGS_H || settings_touch_latched != 0U) {
    return;
  }
  settings_touch_latched = 1U;

  if(settings_input_active != 0U) {
    settings_handle_keyboard_coordinate(x, y, event);
    return;
  }

  if(y >= SETTINGS_MACHINE_Y &&
     y < (uint16_t)(SETTINGS_MACHINE_Y + SETTINGS_ROW_H)) {
    touched_field = (x < SETTINGS_RIGHT_X) ? SETTINGS_FIELD_MACHINE : SETTINGS_FIELD_LINE;
  } else if(y >= SETTINGS_STATION_Y &&
            y < (uint16_t)(SETTINGS_STATION_Y + SETTINGS_ROW_H)) {
    /* The right cell is a read-only live WiFi result; only STATION is
     * editable on this row. */
    if(x < SETTINGS_RIGHT_X) {
      touched_field = SETTINGS_FIELD_STATION;
    }
  } else if(y >= SETTINGS_SSID_Y &&
            y < (uint16_t)(SETTINGS_SSID_Y + SETTINGS_ROW_H)) {
    touched_field = (x < SETTINGS_RIGHT_X) ?
                     SETTINGS_FIELD_SSID : SETTINGS_FIELD_SERVICE_HOST;
  } else if(y >= SETTINGS_PASSWORD_Y &&
            y < (uint16_t)(SETTINGS_PASSWORD_Y + SETTINGS_ROW_H)) {
    touched_field = (x < SETTINGS_RIGHT_X) ?
                     SETTINGS_FIELD_PASSWORD : SETTINGS_FIELD_SERVICE_PORT;
  }

  if(touched_field < SETTINGS_FIELD_COUNT) {
    settings_begin_edit(touched_field);
  } else if(y >= SETTINGS_MAIN_KEY_Y &&
            y < (uint16_t)(SETTINGS_MAIN_KEY_Y + SETTINGS_MAIN_KEY_H)) {
    if(x < 120U) {
      /* K1 keeps the familiar SELF/LEARN appearance, but it is deliberately
       * an edit shortcut here.  Learning can only be started from the normal
       * tester page, so a settings touch can never erase the recipe. */
      settings_begin_edit(settings_field);
    } else if(x < 240U) {
      settings_start_wifi_test();
    } else if(x < 360U) {
      settings_request_exit();
    } else {
      settings_save();
    }
  }
}

static void settings_handle_component(uint8_t component_id, uint8_t event)
{
  if(event == 0U) {
    settings_touch_latched = 0U;
    return;
  }
  if(settings_touch_latched != 0U) {
    return;
  }
  settings_touch_latched = 1U;

  switch(component_id) {
  case 21U: settings_begin_edit(SETTINGS_FIELD_MACHINE); break;
  case 22U: settings_begin_edit(SETTINGS_FIELD_LINE); break;
  case 23U: settings_begin_edit(SETTINGS_FIELD_STATION); break;
  case 24U: settings_begin_edit(SETTINGS_FIELD_SSID); break;
  case 25U: settings_begin_edit(SETTINGS_FIELD_PASSWORD); break;
  case 26U: settings_begin_edit(SETTINGS_FIELD_SERVICE_HOST); break;
  case 27U: settings_begin_edit(SETTINGS_FIELD_SERVICE_PORT); break;
  case 31U: settings_start_wifi_test(); break;
  case 32U: settings_save(); break;
  case 33U: settings_request_exit(); break;
  default: break;
  }
}

static uint8_t settings_starts_with(const char *text, const char *prefix)
{
  if(text == 0 || prefix == 0) {
    return 0U;
  }
  while(*prefix != '\0') {
    if(*text != *prefix) {
      return 0U;
    }
    text++;
    prefix++;
  }
  return 1U;
}

static void settings_handle_ascii(const char *text)
{
  const char *value = 0;

  if(text == 0) {
    return;
  }
  if(strcmp(text, "cfg.test") == 0 || strcmp(text, "wifi.test") == 0) {
    settings_start_wifi_test();
    return;
  }
  if(strcmp(text, "cfg.save") == 0) {
    settings_save();
    return;
  }
  if(strcmp(text, "cfg.cancel") == 0 || strcmp(text, "cfg.reset") == 0) {
    settings_request_exit();
    return;
  }
  if(strcmp(text, "cfg.refresh") == 0) {
    if(settings_input_active != 0U) {
      settings_draw_keyboard();
    } else {
      settings_draw();
    }
    return;
  }
  if(settings_starts_with(text, "cfg.machine=") != 0U ||
     settings_starts_with(text, "machine=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_MACHINE, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.line=") != 0U ||
            settings_starts_with(text, "line=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_LINE, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.station=") != 0U ||
            settings_starts_with(text, "station=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_STATION, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.ssid=") != 0U ||
            settings_starts_with(text, "ssid=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_SSID, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.password=") != 0U ||
            settings_starts_with(text, "password=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_PASSWORD, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.host=") != 0U ||
            settings_starts_with(text, "host=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_SERVICE_HOST, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.port=") != 0U ||
            settings_starts_with(text, "port=") != 0U) {
    value = strchr(text, '=');
    (void)settings_set_field(SETTINGS_FIELD_SERVICE_PORT, value == 0 ? "" : value + 1);
  } else if(settings_starts_with(text, "cfg.value=") != 0U) {
    (void)settings_set_field(settings_field, text + 10U);
  } else if(settings_starts_with(text, "cfg.field=")) {
    value = text + 10U;
    if(strcmp(value, "machine") == 0) {
      settings_select_field(SETTINGS_FIELD_MACHINE);
    } else if(strcmp(value, "line") == 0) {
      settings_select_field(SETTINGS_FIELD_LINE);
    } else if(strcmp(value, "station") == 0) {
      settings_select_field(SETTINGS_FIELD_STATION);
    } else if(strcmp(value, "ssid") == 0) {
      settings_select_field(SETTINGS_FIELD_SSID);
    } else if(strcmp(value, "password") == 0) {
      settings_select_field(SETTINGS_FIELD_PASSWORD);
    } else if(strcmp(value, "host") == 0) {
      settings_select_field(SETTINGS_FIELD_SERVICE_HOST);
    } else if(strcmp(value, "port") == 0) {
      settings_select_field(SETTINGS_FIELD_SERVICE_PORT);
    }
  }
}

void tester_settings_init(void)
{
  memset(&settings_draft, 0, sizeof(settings_draft));
  settings_field = SETTINGS_FIELD_MACHINE;
  settings_wifi_state = SETTINGS_WIFI_TEST_IDLE;
  settings_wifi_command_deadline_ms = 0U;
  settings_wifi_overall_deadline_ms = 0U;
  settings_wifi_ip_retry_due_ms = 0U;
  settings_wifi_ip_retry_pending = 0U;
  settings_wifi_raw_owned = 0U;
  settings_wifi_got_ip = 0U;
  settings_wifi_got_mac = 0U;
  settings_wifi_link_ok = 0U;
  settings_print_host_connected = 0U;
  settings_wifi_link_failed = 0U;
  settings_wifi_changed = 0U;
  settings_reset_requested = 0U;
  settings_input_active = 0U;
  settings_touch_latched = 0U;
  settings_keyboard_upper = 0U;
  settings_keyboard_symbols = 0U;
  settings_input[0] = '\0';
  settings_input_length = 0U;
  settings_ip[0] = '\0';
  settings_load_saved_mac();
  settings_main_page_drawn = 0U;
  memset(settings_main_row_cache_valid, 0, sizeof(settings_main_row_cache_valid));
  memset(settings_main_row_background_cache, 0, sizeof(settings_main_row_background_cache));
  settings_main_identity_cache_valid = 0U;
  settings_main_link_cache_valid = 0U;
  settings_main_link_background_cache = 0U;
  settings_main_status_cache_valid = 0U;
  settings_keyboard_page_drawn = 0U;
  settings_keyboard_mode_cache_valid = 0U;
  settings_keyboard_input_cache_valid = 0U;
  g_tester_settings_active = 0U;
  g_tester_settings_wifi_test_running = 0U;
  g_tester_settings_wifi_test_passed = 0U;
  g_tester_settings_save_count = 0U;
  g_tester_settings_save_error_count = 0U;
  settings_set_status("K3 LONG PRESS FROM READY", SETTINGS_BLUE);
}

uint8_t tester_settings_begin(void)
{
  if(first_gen_display_is_lcdm() == 0U || g_tester_settings_active != 0U) {
    return 0U;
  }

  /* Take exclusive raw ownership as soon as the setup page opens.  A
   * production reconnect may otherwise emit a command while the operator is
   * editing or pressing K2, which is indistinguishable from an AP failure on
   * the shared PC3/PB9 software UART. */
  tester_wifi_print_at_begin();
  settings_wifi_raw_owned = 1U;
  g_tester_settings_wifi_test_running = 0U;

  device_config_copy(&settings_draft);
  settings_field = SETTINGS_FIELD_MACHINE;
  settings_wifi_state = SETTINGS_WIFI_TEST_IDLE;
  settings_wifi_command_deadline_ms = 0U;
  settings_wifi_overall_deadline_ms = 0U;
  settings_wifi_ip_retry_due_ms = 0U;
  settings_wifi_ip_retry_pending = 0U;
  settings_wifi_got_ip = 0U;
  settings_wifi_got_mac = 0U;
  settings_wifi_link_ok = 0U;
  settings_print_host_connected = 0U;
  settings_wifi_link_failed = 0U;
  settings_wifi_changed = 0U;
  settings_reset_requested = 0U;
  settings_input_active = 0U;
  settings_touch_latched = 1U;
  settings_keyboard_upper = 0U;
  settings_keyboard_symbols = 0U;
  settings_input[0] = '\0';
  settings_input_length = 0U;
  settings_ip[0] = '\0';
  settings_load_saved_mac();
  settings_main_page_drawn = 0U;
  memset(settings_main_row_cache_valid, 0, sizeof(settings_main_row_cache_valid));
  memset(settings_main_row_background_cache, 0, sizeof(settings_main_row_background_cache));
  settings_main_identity_cache_valid = 0U;
  settings_main_link_cache_valid = 0U;
  settings_main_link_background_cache = 0U;
  settings_main_status_cache_valid = 0U;
  settings_keyboard_page_drawn = 0U;
  settings_keyboard_mode_cache_valid = 0U;
  settings_keyboard_input_cache_valid = 0U;
  g_tester_settings_wifi_test_running = 0U;
  g_tester_settings_wifi_test_passed = 0U;
  g_tester_settings_active = 1U;
  settings_set_status("SELECT FIELD, ENTER TEXT, TEST, THEN SAVE", SETTINGS_BLUE);

  /* Normal tester pages ignore every non-K1..K4 touch.  Only this explicit
   * maintenance mode enables the rest of the LCDM touch surface. */
  /* The normal tester keeps command acknowledgements enabled for page
   * construction.  Do the same here; disabling bkcmd was the source of the
   * missing/concatenated glyphs seen on the first maintenance-page build. */
  lcdm_tjc_set_command_ack(1U);
  lcdm_tjc_send_cmd("tsw 255,1");
  lcdm_tjc_send_cmd("sendxy=1");
  settings_draw();
  return 1U;
}

uint8_t tester_settings_is_active(void)
{
  return g_tester_settings_active;
}

uint8_t tester_settings_wifi_is_busy(void)
{
  return g_tester_settings_wifi_test_running;
}

uint8_t tester_settings_take_reset_request(void)
{
  uint8_t requested = settings_reset_requested;
  settings_reset_requested = 0U;
  return requested;
}

void tester_settings_start_saved_wifi(void)
{
  if(g_tester_settings_active == 0U &&
     g_tester_settings_wifi_test_running == 0U) {
    tester_wifi_print_restart();
  }
}

void tester_settings_network_service(uint16_t elapsed_ms)
{
  char line[TESTER_WIFI_AT_LINE_MAX];
  uint32_t now_ms;

  /* Keep the formal ESP-AT TCP state machine alive while the maintenance page
   * is open.  During a manual test raw AT ownership makes this call decode
   * only the test's response queue. */
  tester_wifi_print_service();
  while(tester_wifi_print_at_poll_line(line, sizeof(line)) != 0U) {
    if(g_tester_settings_wifi_test_running != 0U) {
      settings_process_wifi_line(line);
    }
    /* Late asynchronous lines are deliberately discarded while idle.  This
     * keeps the raw queue clean for the next K2 attempt. */
  }

  if(g_tester_settings_wifi_test_running == 0U) {
    return;
  }

  (void)elapsed_ms;
  now_ms = tester_wifi_print_now_ms();

  if(settings_wifi_state == SETTINGS_WIFI_TEST_IP &&
     settings_wifi_ip_retry_pending != 0U &&
     (int32_t)(now_ms - settings_wifi_ip_retry_due_ms) >= 0) {
    if(settings_wifi_overall_deadline_ms != 0U &&
       (int32_t)(now_ms - settings_wifi_overall_deadline_ms) >= 0) {
      settings_finish_wifi_test(0U, "NO STA IP");
    } else {
      settings_issue_wifi_command(SETTINGS_WIFI_TEST_IP);
    }
    return;
  }

  if(settings_wifi_overall_deadline_ms != 0U &&
     (int32_t)(now_ms - settings_wifi_overall_deadline_ms) >= 0) {
    settings_finish_wifi_test(0U,
                              (settings_wifi_state == SETTINGS_WIFI_TEST_IP) ?
                              "NO STA IP" : "TIMEOUT - PRESS K3");
    return;
  }

  if(settings_wifi_command_deadline_ms != 0U &&
     (int32_t)(now_ms - settings_wifi_command_deadline_ms) >= 0) {
    if(settings_wifi_state == SETTINGS_WIFI_TEST_JOIN &&
       settings_wifi_got_ip != 0U) {
      settings_issue_wifi_command(SETTINGS_WIFI_TEST_IP);
      return;
    }
    if(settings_wifi_state == SETTINGS_WIFI_TEST_MAC &&
       settings_wifi_got_mac != 0U) {
      if(settings_has_print_host() != 0U) {
        settings_issue_wifi_command(SETTINGS_WIFI_TEST_TCP_START);
      } else {
        settings_finish_wifi_test(1U, settings_ip);
      }
      return;
    }
    if(settings_wifi_state == SETTINGS_WIFI_TEST_IP) {
      settings_wifi_ip_retry_pending = 1U;
      settings_wifi_ip_retry_due_ms = now_ms;
      return;
    }
    settings_finish_wifi_test(0U, "TIMEOUT - PRESS K3");
  }
}

void tester_settings_service(void)
{
  lcdm_tjc_event_t event;

  if(g_tester_settings_active == 0U) {
    return;
  }

  tester_settings_network_service(1U);

  while(g_tester_settings_active != 0U && lcdm_tjc_poll_event(&event) != 0U) {
    if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      settings_handle_coordinate(event.x, event.y, event.touch_event);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH) {
      settings_handle_component(event.component_id, event.touch_event);
    } else if(event.type == LCDM_TJC_EVENT_ASCII) {
      settings_handle_ascii(event.ascii);
    }
  }

  delay_ms(1U);
}
