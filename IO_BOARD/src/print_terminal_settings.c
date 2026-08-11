#include "print_terminal_settings.h"

#include "first_gen_display.h"
#include "lcdm_tjc.h"
#include "print_driver.h"
#include "print_host_wifi.h"
#include "print_terminal_store.h"
#include "tester_wifi_print.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SET_W                         480U
#define SET_H                         272U
#define SET_HEADER_H                   32U
#define SET_TAB_Y                      32U
#define SET_TAB_H                      28U
#define SET_ROW_Y                      62U
#define SET_ROW_H                      30U
#define SET_KEY_Y                     214U
#define SET_KEY_H                      58U
#define SET_CELL_W                    240U
#define SET_LABEL_W                    96U
#define SET_VALUE_X                   (SET_LABEL_W + 2U)
#define SET_VALUE_W                   (SET_CELL_W - SET_VALUE_X - 2U)
#define SET_STATUS_X                  300U
#define SET_STATUS_W                  176U
#define SET_EDIT_BAR_Y                38U
#define SET_EDIT_BAR_H                30U
#define SET_EDIT_VALUE_X             132U
#define SET_EDIT_VALUE_W             332U
#define SET_EDIT_CHAR_W               10U
#define SET_KB_ROW1_Y                 72U
#define SET_KB_ROW2_Y                101U
#define SET_KB_ROW3_Y                130U
#define SET_KB_ROW_H                  25U
#define SET_KB_FUNC_Y                178U
#define SET_KB_FUNC_H                 28U
#define SET_INPUT_MAX                 PRINT_TERMINAL_WIFI_PASSWORD_MAX
#define SET_STATUS_MAX                48U
#define SET_VALUE_MAX                 22U
#define SET_READONLY_CACHE_COUNT       3U
#define SET_TOUCH_LOCK_MS            800UL

#define C_BLACK                       FIRST_GEN_DISPLAY_COLOR_BLACK
#define C_BLUE                        FIRST_GEN_DISPLAY_COLOR_BLUE
#define C_GREEN                       FIRST_GEN_DISPLAY_COLOR_GREEN
/* A verified WiFi link matches the automatic all-connection PASS panel,
 * instead of using the lighter generic success green. */
#define C_WIFI_PASS_GREEN             FIRST_GEN_DISPLAY_COLOR_AUTO_PASS_GREEN
#define C_RED                         FIRST_GEN_DISPLAY_COLOR_RED
#define C_WHITE                       FIRST_GEN_DISPLAY_COLOR_WHITE
#define C_ROW                          FIRST_GEN_DISPLAY_COLOR_ROW_BG
#define C_PALE_BLUE                   FIRST_GEN_DISPLAY_COLOR_PALE_BLUE
#define C_PALE_CYAN                   FIRST_GEN_DISPLAY_COLOR_PALE_CYAN
#define C_PALE_PINK                   FIRST_GEN_DISPLAY_COLOR_PALE_PINK
#define C_NAVY                        16U
#define C_SOFT_PINK                   64593U
#define C_MAGENTA                     63519U
#define C_ORANGE                      64512U

typedef enum {
  SET_FIELD_NONE = 0,
  SET_FIELD_TEMPLATE_NAME,
  SET_FIELD_TITLE,
  SET_FIELD_ITEM,
  SET_FIELD_CONTENT,
  SET_FIELD_CODE,
  SET_FIELD_QTY,
  SET_FIELD_COPIES,
  SET_FIELD_STATION,
  SET_FIELD_RESULT,
  SET_FIELD_CONTROLLER,
  SET_FIELD_LINE,
  SET_FIELD_SSID,
  SET_FIELD_PASSWORD,
  SET_FIELD_PORT,
  SET_FIELD_BAUD,
  SET_FIELD_IR
} set_field_t;

static print_terminal_store_config_t draft;
static print_terminal_settings_page_t page;
static uint8_t selected_template;
static set_field_t selected_field;
static uint8_t active;
static uint8_t main_drawn;
static uint8_t keyboard_drawn;
static uint8_t keyboard_upper;
static uint8_t keyboard_symbols;
static uint8_t input_active;
static char input_text[SET_INPUT_MAX];
static uint16_t input_length;
static char input_cache[SET_INPUT_MAX];
static uint8_t input_cache_valid;
static char status_text[SET_STATUS_MAX];
static uint16_t status_color;
static char network_text[SET_STATUS_MAX];
static uint16_t network_color;
static uint8_t return_pending;
static uint8_t job_pending;
static uint8_t comm_dirty;
static print_job_t pending_job;
static char cell_cache[18][SET_VALUE_MAX];
static uint8_t cell_cache_valid[18];
static uint8_t cell_selected_cache[18];
/* Read-only network cells are deliberately cached separately from editable
 * fields.  A DHCP/MAC/status update must repaint only its own value strip;
 * invalidating main_drawn here used to redraw the entire COMM page and made
 * the LCDM appear to flash continuously while the ESP-AT state machine ran. */
static char readonly_cache[SET_READONLY_CACHE_COUNT][SET_VALUE_MAX];
static uint16_t readonly_fg_cache[SET_READONLY_CACHE_COUNT];
static uint16_t readonly_bg_cache[SET_READONLY_CACHE_COUNT];
static uint8_t readonly_cache_valid[SET_READONLY_CACHE_COUNT];
static uint8_t network_online_cache;
static uint8_t network_online_cache_valid;
/* sendxy=1 can produce several coordinate packets for one held finger. */
static uint8_t touch_pressed;
static uint8_t touch_lock_active;
static uint32_t touch_lock_deadline_ms;

static void set_text(char *out, uint16_t capacity, const char *text)
{
  if(out == 0 || capacity == 0U) {
    return;
  }
  (void)snprintf(out, capacity, "%s", text == 0 ? "" : text);
}

static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char command[48];
  (void)snprintf(command, sizeof(command), "fill %u,%u,%u,%u,%u",
                 (unsigned int)x, (unsigned int)y, (unsigned int)w,
                 (unsigned int)h, (unsigned int)color);
  lcdm_tjc_send_cmd(command);
}

static void lcd_circle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
  char command[40];
  (void)snprintf(command, sizeof(command), "cirs %u,%u,%u,%u",
                 (unsigned int)x, (unsigned int)y, (unsigned int)radius,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(command);
}

static void lcd_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           uint16_t radius, uint16_t color)
{
  if(w <= (uint16_t)(radius * 2U) || h <= (uint16_t)(radius * 2U)) {
    lcd_fill(x, y, w, h, color);
    return;
  }
  lcd_fill((uint16_t)(x + radius), y, (uint16_t)(w - radius * 2U), h, color);
  lcd_fill(x, (uint16_t)(y + radius), w, (uint16_t)(h - radius * 2U), color);
  lcd_circle((uint16_t)(x + radius), (uint16_t)(y + radius), radius, color);
  lcd_circle((uint16_t)(x + w - radius - 1U), (uint16_t)(y + radius), radius, color);
  lcd_circle((uint16_t)(x + radius), (uint16_t)(y + h - radius - 1U), radius, color);
  lcd_circle((uint16_t)(x + w - radius - 1U),
             (uint16_t)(y + h - radius - 1U), radius, color);
}

static void lcd_escape(const char *text, char *out, uint16_t capacity)
{
  uint16_t index = 0U;

  if(out == 0 || capacity == 0U) {
    return;
  }
  if(text == 0) {
    text = "";
  }
  while(*text != '\0' && (uint16_t)(index + 1U) < capacity) {
    unsigned char value = (unsigned char)*text++;
    if(value < 0x20U || value == 0x7FU) {
      continue;
    }
    if(value == '"' || value == '\\') {
      if((uint16_t)(index + 2U) >= capacity) {
        break;
      }
      out[index++] = '\\';
    }
    out[index++] = (char)value;
  }
  out[index] = '\0';
}

static void lcd_text(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     uint16_t font, uint16_t fg, uint16_t bg, uint8_t align,
                     const char *text)
{
  char safe[128];
  char command[220];
  lcd_escape(text, safe, sizeof(safe));
  (void)snprintf(command, sizeof(command),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 (unsigned int)x, (unsigned int)y, (unsigned int)w,
                 (unsigned int)h, (unsigned int)font, (unsigned int)fg,
                 (unsigned int)bg, (unsigned int)align, safe);
  lcdm_tjc_send_cmd(command);
}

static void draw_underlines(uint16_t x, uint16_t y, uint16_t color, const char *text)
{
  uint16_t index;
  if(text == 0) {
    return;
  }
  for(index = 0U; text[index] != '\0'; index++) {
    if(text[index] == '_') {
      lcd_fill((uint16_t)(x + index * SET_EDIT_CHAR_W),
               (uint16_t)(y + 15U), 8U, 2U, color);
    }
  }
}

static void set_status(const char *text, uint16_t color)
{
  set_text(status_text, sizeof(status_text), text);
  status_color = color;
  if(active != 0U && input_active == 0U) {
    lcd_fill(SET_STATUS_X, 1U, SET_STATUS_W, 29U, C_NAVY);
    lcd_text(SET_STATUS_X, 7U, SET_STATUS_W, 17U, 4U,
             color == C_RED ? C_RED : C_WHITE, C_NAVY, 2U, status_text);
  }
}

static const char *field_label(set_field_t field)
{
  switch(field) {
  case SET_FIELD_TEMPLATE_NAME: return "NAME";
  case SET_FIELD_TITLE: return "TITLE";
  case SET_FIELD_ITEM: return "ITEM";
  case SET_FIELD_CONTENT: return "CONTENT";
  case SET_FIELD_CODE: return "CODE";
  case SET_FIELD_QTY: return "QTY";
  case SET_FIELD_COPIES: return "COPIES";
  case SET_FIELD_STATION: return "STATION";
  case SET_FIELD_RESULT: return "RESULT";
  case SET_FIELD_CONTROLLER: return "HOST NAME";
  case SET_FIELD_LINE: return "PD LINE";
  case SET_FIELD_SSID: return "WIFI SSID";
  case SET_FIELD_PASSWORD: return "WIFI PWD";
  case SET_FIELD_PORT: return "LISTEN PORT";
  case SET_FIELD_BAUD: return "PRN BAUD";
  case SET_FIELD_IR: return "IR FALLBACK";
  default: return "";
  }
}

static uint8_t field_index(set_field_t field)
{
  return (uint8_t)field;
}

static void short_value(const char *source, char *out, uint16_t size)
{
  uint16_t length;
  if(out == 0 || size == 0U) {
    return;
  }
  set_text(out, size, source);
  length = (uint16_t)strlen(out);
  if(length >= size - 1U && size >= 4U) {
    out[size - 4U] = '.';
    out[size - 3U] = '.';
    out[size - 2U] = '.';
    out[size - 1U] = '\0';
  }
}

static void number_text(char *out, uint16_t size, uint32_t value)
{
  (void)snprintf(out, size, "%lu", (unsigned long)value);
}

static const char *field_value(set_field_t field, char *scratch, uint16_t scratch_size)
{
  print_job_t *job = &draft.templates[selected_template].job;

  if(scratch == 0 || scratch_size == 0U) {
    return "";
  }
  scratch[0] = '\0';
  switch(field) {
  case SET_FIELD_TEMPLATE_NAME: return draft.templates[selected_template].name;
  case SET_FIELD_TITLE: return job->title;
  case SET_FIELD_ITEM: return job->item;
  case SET_FIELD_CONTENT: return job->content;
  case SET_FIELD_CODE: return job->code;
  case SET_FIELD_QTY: number_text(scratch, scratch_size, job->quantity); return scratch;
  case SET_FIELD_COPIES: number_text(scratch, scratch_size, job->copies); return scratch;
  case SET_FIELD_STATION: number_text(scratch, scratch_size, job->station_id); return scratch;
  case SET_FIELD_RESULT: return job->pass ? "PASS" : "NG";
  case SET_FIELD_CONTROLLER: return draft.controller_name;
  case SET_FIELD_LINE: return draft.line_id;
  case SET_FIELD_SSID: return draft.wifi_ssid;
  case SET_FIELD_PASSWORD: return draft.wifi_password;
  case SET_FIELD_PORT: number_text(scratch, scratch_size, draft.wifi_listen_port); return scratch;
  case SET_FIELD_BAUD:
    number_text(scratch, scratch_size, draft.printer_config.baudrate);
    return scratch;
  case SET_FIELD_IR: return draft.ir_fallback_enabled ? "ON" : "OFF";
  default: return "";
  }
}

static void cell_geometry(uint8_t row, uint8_t column, uint16_t *x, uint16_t *y)
{
  if(x != 0) *x = (uint16_t)(column * SET_CELL_W);
  if(y != 0) *y = (uint16_t)(SET_ROW_Y + row * SET_ROW_H);
}

static set_field_t field_at(uint16_t x, uint16_t y)
{
  uint8_t row;
  uint8_t column;
  if(y < SET_ROW_Y || y >= (uint16_t)(SET_ROW_Y + 5U * SET_ROW_H) ||
     x >= SET_W) {
    return SET_FIELD_NONE;
  }
  row = (uint8_t)((y - SET_ROW_Y) / SET_ROW_H);
  column = (x >= SET_CELL_W) ? 1U : 0U;
  if(page == PRINT_TERMINAL_SETTINGS_LABEL) {
    static const set_field_t map[5][2] = {
      {SET_FIELD_NONE, SET_FIELD_TEMPLATE_NAME},
      {SET_FIELD_TITLE, SET_FIELD_ITEM},
      {SET_FIELD_CONTENT, SET_FIELD_CODE},
      {SET_FIELD_QTY, SET_FIELD_COPIES},
      {SET_FIELD_STATION, SET_FIELD_RESULT}
    };
    return map[row][column];
  }
  {
    static const set_field_t map[5][2] = {
      {SET_FIELD_CONTROLLER, SET_FIELD_LINE},
      {SET_FIELD_SSID, SET_FIELD_PASSWORD},
      {SET_FIELD_PORT, SET_FIELD_BAUD},
      {SET_FIELD_IR, SET_FIELD_NONE},
      {SET_FIELD_NONE, SET_FIELD_NONE}
    };
    return map[row][column];
  }
}

static void draw_tab(void)
{
  lcd_fill(0U, SET_TAB_Y, SET_W, SET_TAB_H, C_WHITE);
  lcd_fill(0U, SET_TAB_Y, 240U, SET_TAB_H,
           page == PRINT_TERMINAL_SETTINGS_LABEL ? C_PALE_CYAN : C_PALE_BLUE);
  lcd_fill(240U, SET_TAB_Y, 240U, SET_TAB_H,
           page == PRINT_TERMINAL_SETTINGS_COMM ? C_PALE_CYAN : C_PALE_BLUE);
  lcd_text(0U, 37U, 240U, 17U, 4U, C_BLACK,
           page == PRINT_TERMINAL_SETTINGS_LABEL ? C_PALE_CYAN : C_PALE_BLUE,
           1U, "LABEL DATA");
  lcd_text(240U, 37U, 240U, 17U, 4U, C_BLACK,
           page == PRINT_TERMINAL_SETTINGS_COMM ? C_PALE_CYAN : C_PALE_BLUE,
           1U, "COMM/WIFI");
}

static void draw_key(uint8_t index, const char *caption, uint16_t color)
{
  uint16_t x = (uint16_t)(index * 120U);
  char key[8];
  (void)snprintf(key, sizeof(key), "K%u", (unsigned int)(index + 1U));
  lcd_fill(x, SET_KEY_Y, 120U, SET_KEY_H, C_WHITE);
  lcd_round_rect((uint16_t)(x + 2U), (uint16_t)(SET_KEY_Y + 2U),
                 116U, 54U, 7U, color);
  lcd_text(x, (uint16_t)(SET_KEY_Y + 5U), 120U, 31U, 2U, C_WHITE, color, 1U, key);
  lcd_text(x, (uint16_t)(SET_KEY_Y + 34U), 120U, 19U, 4U, C_WHITE, color, 1U, caption);
}

static void draw_cell(uint8_t row, uint8_t column, set_field_t field)
{
  uint16_t x;
  uint16_t y;
  uint16_t fg = C_BLUE;
  uint16_t value_bg = C_WHITE;
  uint16_t label_bg;
  char scratch[32];
  char value[SET_VALUE_MAX];
  uint8_t index = field_index(field);
  uint8_t selected = (selected_field == field) ? 1U : 0U;

  if(field == SET_FIELD_NONE) {
    return;
  }
  cell_geometry(row, column, &x, &y);
  short_value(field_value(field, scratch, sizeof(scratch)), value, sizeof(value));
  label_bg = selected ? C_PALE_CYAN : C_PALE_BLUE;
  if(field == SET_FIELD_NONE) {
    return;
  }
  if((page == PRINT_TERMINAL_SETTINGS_COMM) &&
     (field == SET_FIELD_SSID || field == SET_FIELD_PASSWORD) &&
     print_host_wifi_is_online() != 0U) {
    fg = C_WHITE;
    value_bg = C_WIFI_PASS_GREEN;
  }
  if(index < sizeof(cell_cache_valid) && cell_cache_valid[index] != 0U &&
     cell_selected_cache[index] == selected && strcmp(cell_cache[index], value) == 0) {
    return;
  }
  if(index < sizeof(cell_cache_valid)) {
    cell_cache_valid[index] = 1U;
    cell_selected_cache[index] = selected;
    set_text(cell_cache[index], sizeof(cell_cache[index]), value);
  }
  lcd_fill(x, y, SET_CELL_W, SET_ROW_H, C_WHITE);
  lcd_fill(x, y, SET_LABEL_W, SET_ROW_H, label_bg);
  lcd_fill((uint16_t)(x + SET_VALUE_X), y, SET_VALUE_W, SET_ROW_H, value_bg);
  lcd_text((uint16_t)(x + 3U), (uint16_t)(y + 7U), SET_LABEL_W, 17U, 4U,
           C_BLACK, label_bg, 0U, field_label(field));
  lcd_text((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), SET_VALUE_W, 17U,
           4U, fg, value_bg, 0U, value);
  draw_underlines((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), fg, value);
  lcd_fill(x, (uint16_t)(y + SET_ROW_H - 2U), SET_CELL_W, 2U, C_NAVY);
}

static void draw_readonly_cell(uint8_t row, uint8_t column, const char *label,
                               const char *source, uint16_t foreground,
                               uint16_t background, uint8_t cache_slot,
                               uint8_t force)
{
  uint16_t x;
  uint16_t y;
  char value[SET_VALUE_MAX];
  uint8_t full_redraw;

  if(cache_slot >= SET_READONLY_CACHE_COUNT) {
    return;
  }
  cell_geometry(row, column, &x, &y);
  short_value(source, value, sizeof(value));
  full_redraw = (force != 0U || readonly_cache_valid[cache_slot] == 0U) ? 1U : 0U;
  if(force == 0U && readonly_cache_valid[cache_slot] != 0U &&
     readonly_fg_cache[cache_slot] == foreground &&
     readonly_bg_cache[cache_slot] == background &&
     strcmp(readonly_cache[cache_slot], value) == 0) {
    return;
  }
  readonly_cache_valid[cache_slot] = 1U;
  readonly_fg_cache[cache_slot] = foreground;
  readonly_bg_cache[cache_slot] = background;
  set_text(readonly_cache[cache_slot], sizeof(readonly_cache[cache_slot]), value);
  if(full_redraw == 0U) {
    /* The label, cell border, and lower separator are static.  Once the
     * initial frame exists, repaint only the value strip; this is important
     * for WIFI state/IP/MAC changes arriving while the page is open. */
    lcd_fill((uint16_t)(x + SET_VALUE_X), y, SET_VALUE_W, SET_ROW_H, background);
    lcd_text((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), SET_VALUE_W, 17U,
             4U, foreground, background, 0U, value);
    draw_underlines((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), foreground, value);
    return;
  }
  lcd_fill(x, y, SET_CELL_W, SET_ROW_H, C_WHITE);
  lcd_fill(x, y, SET_LABEL_W, SET_ROW_H, C_PALE_BLUE);
  lcd_fill((uint16_t)(x + SET_VALUE_X), y, SET_VALUE_W, SET_ROW_H, background);
  lcd_text((uint16_t)(x + 3U), (uint16_t)(y + 7U), SET_LABEL_W, 17U, 4U,
           C_BLACK, C_PALE_BLUE, 0U, label);
  lcd_text((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), SET_VALUE_W, 17U,
           4U, foreground, background, 0U, value);
  draw_underlines((uint16_t)(x + SET_VALUE_X), (uint16_t)(y + 7U), foreground, value);
  lcd_fill(x, (uint16_t)(y + SET_ROW_H - 2U), SET_CELL_W, 2U, C_NAVY);
}

static const char *network_mac_text(void)
{
  const char *mac = print_host_wifi_mac_text();

  if(mac != 0 && mac[0] != '\0') {
    return mac;
  }
  if(draft.wifi_mac[0] != '\0') {
    return draft.wifi_mac;
  }
  return "--";
}

/* Update the three network-only rows and the online colour of SSID/password.
 * This is called after the one-time page frame has been drawn.  All drawing
 * helpers compare their cached value/colours, so a stable network produces no
 * LCDM commands at all. */
static void draw_network_cells(uint8_t force)
{
  uint8_t online;
  uint8_t online_changed;
  uint16_t wifi_bg;
  uint16_t wifi_fg;
  const char *ip;

  if(page != PRINT_TERMINAL_SETTINGS_COMM || input_active != 0U) {
    return;
  }

  online = print_host_wifi_is_online() != 0U ? 1U : 0U;
  online_changed = (network_online_cache_valid == 0U ||
                    network_online_cache != online) ? 1U : 0U;
  if(online_changed != 0U) {
    network_online_cache = online;
    network_online_cache_valid = 1U;
    /* draw_cell() owns the editable SSID/password cache.  Invalidate only
     * those two entries when their green/white colour changes. */
    cell_cache_valid[field_index(SET_FIELD_SSID)] = 0U;
    cell_cache_valid[field_index(SET_FIELD_PASSWORD)] = 0U;
    draw_cell(1U, 0U, SET_FIELD_SSID);
    draw_cell(1U, 1U, SET_FIELD_PASSWORD);
  }

  wifi_bg = online != 0U ? C_WIFI_PASS_GREEN : C_WHITE;
  wifi_fg = online != 0U ? C_WHITE : network_color;
  draw_readonly_cell(3U, 1U, "WIFI LINK", network_text,
                     wifi_fg, wifi_bg, 0U, force);

  ip = print_host_wifi_ip_text();
  draw_readonly_cell(4U, 0U, "IP", (ip != 0 && ip[0] != '\0') ? ip : "--",
                     C_BLUE, C_WHITE, 1U, force);
  draw_readonly_cell(4U, 1U, "MAC", network_mac_text(),
                     C_BLUE, C_WHITE, 2U, force);
}

static void draw_template_selector(void)
{
  char value[SET_VALUE_MAX];
  uint16_t y = SET_ROW_Y;
  (void)snprintf(value, sizeof(value), "T%u", (unsigned int)(selected_template + 1U));
  lcd_fill(0U, y, SET_CELL_W, SET_ROW_H, C_WHITE);
  lcd_fill(0U, y, SET_LABEL_W, SET_ROW_H, C_PALE_BLUE);
  lcd_fill(SET_VALUE_X, y, SET_VALUE_W, SET_ROW_H, C_WHITE);
  lcd_text(3U, (uint16_t)(y + 7U), SET_LABEL_W, 17U, 4U,
           C_BLACK, C_PALE_BLUE, 0U, "TEMPLATE");
  lcd_text(SET_VALUE_X, (uint16_t)(y + 7U), SET_VALUE_W, 17U, 4U,
           C_BLUE, C_WHITE, 0U, value);
  lcd_fill(0U, (uint16_t)(y + SET_ROW_H - 2U), SET_CELL_W, 2U, C_NAVY);
}

static void draw_main_full(void)
{
  uint8_t row;
  uint8_t column;
  char title[48];
  static const set_field_t label_map[5][2] = {
    {SET_FIELD_NONE, SET_FIELD_TEMPLATE_NAME},
    {SET_FIELD_TITLE, SET_FIELD_ITEM},
    {SET_FIELD_CONTENT, SET_FIELD_CODE},
    {SET_FIELD_QTY, SET_FIELD_COPIES},
    {SET_FIELD_STATION, SET_FIELD_RESULT}
  };
  static const set_field_t comm_map[5][2] = {
    {SET_FIELD_CONTROLLER, SET_FIELD_LINE},
    {SET_FIELD_SSID, SET_FIELD_PASSWORD},
    {SET_FIELD_PORT, SET_FIELD_BAUD},
    {SET_FIELD_IR, SET_FIELD_NONE},
    {SET_FIELD_NONE, SET_FIELD_NONE}
  };

  (void)snprintf(title, sizeof(title), "%s SETUP",
                 page == PRINT_TERMINAL_SETTINGS_LABEL ? "LABEL" : "COMM");
  lcd_fill(0U, 0U, SET_W, SET_H, C_WHITE);
  lcd_fill(0U, 0U, SET_W, SET_HEADER_H, C_NAVY);
  lcd_text(0U, 1U, 296U, 30U, 2U, C_WHITE, C_NAVY, 1U, title);
  lcd_text(SET_STATUS_X, 7U, SET_STATUS_W, 17U, 4U,
           status_color == C_RED ? C_RED : C_WHITE, C_NAVY, 2U, status_text);
  draw_tab();
  memset(cell_cache_valid, 0, sizeof(cell_cache_valid));
  memset(readonly_cache_valid, 0, sizeof(readonly_cache_valid));
  /* The editable cells use the current online state while the frame is
   * painted.  Keeping this value here prevents draw_network_cells() from
   * immediately repainting SSID/PWD a second time on the same frame. */
  network_online_cache = print_host_wifi_is_online() != 0U ? 1U : 0U;
  network_online_cache_valid = 1U;
  for(row = 0U; row < 5U; row++) {
    for(column = 0U; column < 2U; column++) {
      set_field_t field = page == PRINT_TERMINAL_SETTINGS_LABEL ?
                           label_map[row][column] : comm_map[row][column];
      if(page == PRINT_TERMINAL_SETTINGS_LABEL && row == 0U && column == 0U) {
        draw_template_selector();
      } else {
        draw_cell(row, column, field);
      }
    }
  }
  if(page == PRINT_TERMINAL_SETTINGS_COMM) {
    draw_network_cells(1U);
  }
  if(page == PRINT_TERMINAL_SETTINGS_LABEL) {
    draw_key(0U, "PREV", C_BLUE);
    draw_key(1U, "NEXT", C_SOFT_PINK);
  } else {
    draw_key(0U, "WIFI TEST", C_BLUE);
    draw_key(1U, "PRN TEST", C_SOFT_PINK);
  }
  draw_key(2U, "CANCEL", C_MAGENTA);
  draw_key(3U, "SAVE", C_ORANGE);
  main_drawn = 1U;
}

static void draw_keyboard_row(const char *keys, uint8_t count,
                              uint16_t y, uint16_t start_x, uint16_t width)
{
  uint8_t index;
  char text[2];
  for(index = 0U; index < count; index++) {
    uint16_t x = (uint16_t)(start_x + index * width);
    text[0] = keys[index];
    text[1] = '\0';
    lcd_fill((uint16_t)(x + 1U), y, (uint16_t)(width - 2U), SET_KB_ROW_H, C_PALE_BLUE);
    lcd_text((uint16_t)(x + 1U), (uint16_t)(y + 4U), (uint16_t)(width - 2U),
             17U, 4U, C_NAVY, C_PALE_BLUE, 1U, text);
  }
}

static void draw_keyboard_rows(void)
{
  const char *row1;
  const char *row2;
  const char *row3;
  uint8_t row3_count;
  uint16_t row3_x;
  uint16_t row3_w;
  lcd_fill(0U, 68U, SET_W, 88U, C_WHITE);
  if(keyboard_symbols != 0U) {
    row1 = "1234567890";
    row2 = "!@#$%^&*";
    row3 = "-_=+().?";
    row3_count = 8U;
    row3_x = 24U;
    row3_w = 54U;
  } else {
    row1 = keyboard_upper ? "QWERTYUIOP" : "qwertyuiop";
    row2 = keyboard_upper ? "ASDFGHJKL" : "asdfghjkl";
    row3 = keyboard_upper ? "ZXCVBNM" : "zxcvbnm";
    row3_count = 7U;
    row3_x = 30U;
    row3_w = 60U;
  }
  draw_keyboard_row(row1, 10U, SET_KB_ROW1_Y, 0U, 48U);
  draw_keyboard_row(row2, 9U, SET_KB_ROW2_Y, 24U, 48U);
  draw_keyboard_row(row3, row3_count, SET_KB_ROW3_Y, row3_x, row3_w);
}

static void draw_keyboard_input(void)
{
  uint16_t common = 0U;
  uint16_t old_length = input_cache_valid ? (uint16_t)strlen(input_cache) : 0U;
  uint16_t new_length = (uint16_t)strlen(input_text);
  uint16_t clear_x;
  uint16_t clear_w;
  char suffix[SET_INPUT_MAX];

  if(input_cache_valid != 0U && strcmp(input_cache, input_text) == 0) {
    return;
  }
  while(common < old_length && common < new_length &&
        input_cache[common] == input_text[common]) {
    common++;
  }
  clear_x = (uint16_t)(SET_EDIT_VALUE_X + common * SET_EDIT_CHAR_W);
  if(clear_x > SET_EDIT_VALUE_X + SET_EDIT_VALUE_W) {
    clear_x = SET_EDIT_VALUE_X + SET_EDIT_VALUE_W;
  }
  clear_w = (uint16_t)(SET_EDIT_VALUE_X + SET_EDIT_VALUE_W - clear_x);
  if(input_cache_valid != 0U &&
     ((common == old_length && new_length == old_length + 1U) ||
      (common == new_length && old_length == new_length + 1U))) {
    clear_w = SET_EDIT_CHAR_W;
  }
  if(clear_w != 0U) {
    lcd_fill(clear_x, SET_EDIT_BAR_Y, clear_w, SET_EDIT_BAR_H, C_PALE_CYAN);
    set_text(suffix, sizeof(suffix), input_text + common);
    lcd_text(clear_x, (uint16_t)(SET_EDIT_BAR_Y + 5U), clear_w, 17U, 4U,
             C_NAVY, C_PALE_CYAN, 0U, suffix);
    draw_underlines(clear_x, (uint16_t)(SET_EDIT_BAR_Y + 5U), C_NAVY, suffix);
  }
  set_text(input_cache, sizeof(input_cache), input_text);
  input_cache_valid = 1U;
}

static void draw_keyboard_full(void)
{
  char title[48];
  if(input_active == 0U) {
    return;
  }
  (void)snprintf(title, sizeof(title), "EDIT %s", field_label(selected_field));
  lcd_fill(0U, 0U, SET_W, SET_H, C_WHITE);
  lcd_fill(0U, 0U, SET_W, SET_HEADER_H, C_NAVY);
  lcd_text(0U, 1U, SET_W, 30U, 2U, C_WHITE, C_NAVY, 1U, title);
  lcd_fill(8U, SET_EDIT_BAR_Y, 464U, SET_EDIT_BAR_H, C_PALE_CYAN);
  lcd_text(14U, (uint16_t)(SET_EDIT_BAR_Y + 5U), 112U, 17U, 4U,
           C_BLUE, C_PALE_CYAN, 0U, field_label(selected_field));
  draw_keyboard_rows();
  lcd_fill(0U, SET_KB_FUNC_Y, SET_W, SET_KB_FUNC_H, C_ROW);
  lcd_text(0U, 184U, 120U, 17U, 4U, C_NAVY, C_ROW, 1U, "CLEAR");
  lcd_text(120U, 184U, 120U, 17U, 4U, C_NAVY, C_ROW, 1U, "ABC/123");
  lcd_text(240U, 184U, 120U, 17U, 4U, C_NAVY, C_ROW, 1U, "SPACE");
  lcd_text(360U, 184U, 120U, 17U, 4U, C_NAVY, C_ROW, 1U, "BACK");
  draw_key(0U, "SHIFT", C_BLUE);
  draw_key(1U, "ABC/123", C_SOFT_PINK);
  draw_key(2U, "CANCEL", C_MAGENTA);
  draw_key(3U, "OK", C_ORANGE);
  input_cache_valid = 0U;
  draw_keyboard_input();
  keyboard_drawn = 1U;
}

static uint8_t field_capacity(set_field_t field)
{
  switch(field) {
  case SET_FIELD_TEMPLATE_NAME: return sizeof(draft.templates[0].name);
  case SET_FIELD_TITLE: return sizeof(draft.templates[0].job.title);
  case SET_FIELD_ITEM: return sizeof(draft.templates[0].job.item);
  case SET_FIELD_CONTENT: return sizeof(draft.templates[0].job.content);
  case SET_FIELD_CODE: return sizeof(draft.templates[0].job.code);
  case SET_FIELD_CONTROLLER: return sizeof(draft.controller_name);
  case SET_FIELD_LINE: return sizeof(draft.line_id);
  case SET_FIELD_SSID: return sizeof(draft.wifi_ssid);
  case SET_FIELD_PASSWORD: return sizeof(draft.wifi_password);
  default: return 8U;
  }
}

static uint8_t parse_number(const char *text, uint32_t *value)
{
  uint32_t result = 0U;
  uint8_t digits = 0U;
  if(text == 0 || value == 0) return 0U;
  while(*text != '\0') {
    if(*text < '0' || *text > '9') return 0U;
    result = result * 10U + (uint32_t)(*text - '0');
    if(result > 65535U) return 0U;
    text++;
    digits++;
  }
  if(digits == 0U) return 0U;
  *value = result;
  return 1U;
}

static uint8_t copy_input(char *destination, uint16_t capacity)
{
  uint16_t index;
  if(destination == 0 || capacity == 0U || input_text[0] == '\0') {
    /* Empty SSID/password are allowed; all other text fields are not. */
    if(destination == draft.wifi_ssid || destination == draft.wifi_password) {
      destination[0] = '\0';
      return 1U;
    }
    return 0U;
  }
  for(index = 0U; input_text[index] != '\0'; index++) {
    unsigned char value = (unsigned char)input_text[index];
    if(value < 0x20U || value == 0x7FU || index + 1U >= capacity) return 0U;
  }
  memcpy(destination, input_text, index + 1U);
  return 1U;
}

static uint8_t apply_field(void)
{
  print_job_t *job = &draft.templates[selected_template].job;
  uint32_t number;
  char *destination = 0;
  uint16_t capacity = 0U;

  switch(selected_field) {
  case SET_FIELD_TEMPLATE_NAME: destination = draft.templates[selected_template].name; capacity = sizeof(draft.templates[0].name); break;
  case SET_FIELD_TITLE: destination = job->title; capacity = sizeof(job->title); break;
  case SET_FIELD_ITEM: destination = job->item; capacity = sizeof(job->item); break;
  case SET_FIELD_CONTENT: destination = job->content; capacity = sizeof(job->content); break;
  case SET_FIELD_CODE: destination = job->code; capacity = sizeof(job->code); break;
  case SET_FIELD_CONTROLLER: destination = draft.controller_name; capacity = sizeof(draft.controller_name); break;
  case SET_FIELD_LINE: destination = draft.line_id; capacity = sizeof(draft.line_id); break;
  case SET_FIELD_SSID: destination = draft.wifi_ssid; capacity = sizeof(draft.wifi_ssid); break;
  case SET_FIELD_PASSWORD: destination = draft.wifi_password; capacity = sizeof(draft.wifi_password); break;
  case SET_FIELD_QTY:
    if(parse_number(input_text, &number) == 0U || number == 0U) return 0U;
    job->quantity = (uint16_t)number; return 1U;
  case SET_FIELD_COPIES:
    if(parse_number(input_text, &number) == 0U || number == 0U || number > 255U) return 0U;
    job->copies = (uint8_t)number; return 1U;
  case SET_FIELD_STATION:
    if(parse_number(input_text, &number) == 0U || number == 0U || number > 10U) return 0U;
    job->station_id = (uint8_t)number; return 1U;
  case SET_FIELD_RESULT:
    if(strcmp(input_text, "PASS") == 0 || strcmp(input_text, "pass") == 0 || strcmp(input_text, "1") == 0) job->pass = 1U;
    else if(strcmp(input_text, "NG") == 0 || strcmp(input_text, "ng") == 0 || strcmp(input_text, "0") == 0) job->pass = 0U;
    else return 0U;
    return 1U;
  case SET_FIELD_PORT:
    if(parse_number(input_text, &number) == 0U || number == 0U || number > 65535U) return 0U;
    draft.wifi_listen_port = (uint16_t)number;
    comm_dirty = 1U;
    return 1U;
  case SET_FIELD_BAUD:
    if(parse_number(input_text, &number) == 0U) return 0U;
    if(number != 1200U && number != 2400U && number != 4800U && number != 9600U &&
       number != 19200U && number != 38400U && number != 57600U && number != 115200U) return 0U;
    draft.printer_config.baudrate = number;
    return 1U;
  case SET_FIELD_IR:
    if(strcmp(input_text, "ON") == 0 || strcmp(input_text, "on") == 0 || strcmp(input_text, "1") == 0) draft.ir_fallback_enabled = 1U;
    else if(strcmp(input_text, "OFF") == 0 || strcmp(input_text, "off") == 0 || strcmp(input_text, "0") == 0) draft.ir_fallback_enabled = 0U;
    else return 0U;
    return 1U;
  default: return 0U;
  }
  if(copy_input(destination, capacity) == 0U) {
    return 0U;
  }
  if(selected_field == SET_FIELD_SSID || selected_field == SET_FIELD_PASSWORD) {
    comm_dirty = 1U;
  }
  return 1U;
}

static void begin_edit(set_field_t field)
{
  char scratch[32];
  const char *value;
  if(field == SET_FIELD_NONE) {
    return;
  }
  selected_field = field;
  value = field_value(field, scratch, sizeof(scratch));
  set_text(input_text, sizeof(input_text), value);
  input_length = (uint16_t)strlen(input_text);
  keyboard_upper = 0U;
  keyboard_symbols = 0U;
  input_active = 1U;
  keyboard_drawn = 0U;
  input_cache_valid = 0U;
  draw_keyboard_full();
}

static void accept_edit(void)
{
  input_active = 0U;
  keyboard_drawn = 0U;
  main_drawn = 0U;
  if(apply_field() == 0U) {
    set_status("INVALID VALUE", C_RED);
  } else {
    set_status("UPDATED - PRESS SAVE", C_BLUE);
  }
}

static void cancel_edit(void)
{
  input_active = 0U;
  keyboard_drawn = 0U;
  main_drawn = 0U;
  set_status("EDIT CANCELLED", C_BLUE);
}

static void input_append(char value)
{
  uint8_t capacity = field_capacity(selected_field);
  if(capacity == 0U || input_length + 1U >= capacity || input_length + 1U >= sizeof(input_text)) return;
  input_text[input_length++] = value;
  input_text[input_length] = '\0';
  draw_keyboard_input();
}

static void input_back(void)
{
  if(input_length == 0U) return;
  input_text[--input_length] = '\0';
  draw_keyboard_input();
}

static void input_clear(void)
{
  input_length = 0U;
  input_text[0] = '\0';
  draw_keyboard_input();
}

static uint8_t keyboard_char(uint16_t x, uint16_t y, char *value)
{
  const char *keys;
  uint8_t count;
  uint16_t start;
  uint16_t width;
  uint8_t index;
  if(y >= SET_KB_ROW1_Y && y < SET_KB_ROW1_Y + SET_KB_ROW_H) {
    keys = keyboard_symbols ? "1234567890" : (keyboard_upper ? "QWERTYUIOP" : "qwertyuiop");
    count = 10U; start = 0U; width = 48U;
  } else if(y >= SET_KB_ROW2_Y && y < SET_KB_ROW2_Y + SET_KB_ROW_H) {
    keys = keyboard_symbols ? "!@#$%^&*" : (keyboard_upper ? "ASDFGHJKL" : "asdfghjkl");
    count = 9U; start = 24U; width = 48U;
  } else if(y >= SET_KB_ROW3_Y && y < SET_KB_ROW3_Y + SET_KB_ROW_H) {
    keys = keyboard_symbols ? "-_=+().?" : (keyboard_upper ? "ZXCVBNM" : "zxcvbnm");
    count = keyboard_symbols ? 8U : 7U; start = keyboard_symbols ? 24U : 30U;
    width = keyboard_symbols ? 54U : 60U;
  } else return 0U;
  if(x < start) return 0U;
  index = (uint8_t)((x - start) / width);
  if(index >= count) return 0U;
  *value = keys[index];
  return 1U;
}

static void keyboard_touch(uint16_t x, uint16_t y)
{
  char value;
  if(keyboard_char(x, y, &value) != 0U) {
    input_append(value);
    return;
  }
  if(y >= SET_KB_FUNC_Y && y < SET_KB_FUNC_Y + SET_KB_FUNC_H) {
    if(x < 120U) input_clear();
    else if(x < 240U) { keyboard_symbols ^= 1U; draw_keyboard_rows(); }
    else if(x < 360U) input_append(' ');
    else input_back();
    return;
  }
  if(y >= SET_KEY_Y && y < SET_KEY_Y + SET_KEY_H) {
    if(x < 120U) { keyboard_upper ^= 1U; draw_keyboard_rows(); }
    else if(x < 240U) { keyboard_symbols ^= 1U; draw_keyboard_rows(); }
    else if(x < 360U) cancel_edit();
    else accept_edit();
  }
}

static void select_template(int8_t delta)
{
  int16_t value = (int16_t)selected_template + delta;
  if(value < 0) value = PRINT_TERMINAL_TEMPLATE_COUNT - 1;
  if(value >= (int16_t)PRINT_TERMINAL_TEMPLATE_COUNT) value = 0;
  selected_template = (uint8_t)value;
  draft.active_template = selected_template;
  selected_field = SET_FIELD_NONE;
  main_drawn = 0U;
  set_status("TEMPLATE SELECTED", C_BLUE);
}

static void save_draft(void)
{
  const print_terminal_store_config_t *stored = print_terminal_store_get();

  draft.active_template = selected_template;
  /* ESP-AT may learn the module MAC while this page is open.  It is
   * read-only here, so never overwrite that newer cached identity with the
   * draft that was copied on page entry. */
  if(stored != 0) {
    set_text(draft.wifi_mac, sizeof(draft.wifi_mac), stored->wifi_mac);
  }
  if(print_terminal_store_validate(&draft) == 0U ||
     print_terminal_store_save(&draft) == 0U) {
    set_status("SAVE ERROR", C_RED);
    return;
  }
  print_terminal_store_apply_runtime();
  if(comm_dirty != 0U) {
    print_host_wifi_restart();
    comm_dirty = 0U;
  }
  pending_job = draft.templates[selected_template].job;
  job_pending = 1U;
  set_status("SAVED - READY", C_GREEN);
}

static void handle_main_touch(uint16_t x, uint16_t y)
{
  if(y >= SET_TAB_Y && y < SET_TAB_Y + SET_TAB_H) {
    page = (x < 240U) ? PRINT_TERMINAL_SETTINGS_LABEL : PRINT_TERMINAL_SETTINGS_COMM;
    selected_field = SET_FIELD_NONE;
    main_drawn = 0U;
    return;
  }
  if(y >= SET_ROW_Y && y < SET_KEY_Y) {
    set_field_t field = field_at(x, y);
    if(field != SET_FIELD_NONE) begin_edit(field);
    return;
  }
  if(y < SET_KEY_Y || x >= SET_W) return;
  if(x < 120U) {
    if(page == PRINT_TERMINAL_SETTINGS_LABEL) select_template(-1);
    else { print_host_wifi_restart(); set_status("WIFI TEST", C_BLUE); }
  } else if(x < 240U) {
    if(page == PRINT_TERMINAL_SETTINGS_LABEL) select_template(1);
    else {
      if(print_driver_submit(&draft.templates[selected_template].job) == PRINT_DRIVER_OK) set_status("PRN TEST SENT", C_GREEN);
      else set_status("PRN TEST ERROR", C_RED);
    }
  } else if(x < 360U) {
    active = 0U;
    return_pending = 1U;
  } else save_draft();
}

static void handle_ascii(const char *text)
{
  if(text == 0) return;
  if(strcmp(text, "save") == 0 || strcmp(text, "template.save") == 0) save_draft();
  else if(strcmp(text, "cancel") == 0) { active = 0U; return_pending = 1U; }
  else if(strcmp(text, "page=label") == 0) { page = PRINT_TERMINAL_SETTINGS_LABEL; main_drawn = 0U; }
  else if(strcmp(text, "page=comm") == 0) { page = PRINT_TERMINAL_SETTINGS_COMM; main_drawn = 0U; }
  else if(strncmp(text, "template=", 9U) == 0) {
    uint32_t value = 0U;
    if(parse_number(text + 9U, &value) != 0U && value >= 1U && value <= PRINT_TERMINAL_TEMPLATE_COUNT) {
      selected_template = (uint8_t)(value - 1U); draft.active_template = selected_template; main_drawn = 0U;
    }
  }
}

void print_terminal_settings_init(void)
{
  memset(&draft, 0, sizeof(draft));
  active = 0U;
  return_pending = 0U;
  job_pending = 0U;
  comm_dirty = 0U;
  network_color = C_BLUE;
  set_text(network_text, sizeof(network_text), "WAITING");
  memset(readonly_cache_valid, 0, sizeof(readonly_cache_valid));
  network_online_cache = 0U;
  network_online_cache_valid = 0U;
  touch_pressed = 0U;
  touch_lock_active = 0U;
  touch_lock_deadline_ms = 0U;
}

uint8_t print_terminal_settings_begin(print_terminal_settings_page_t first_page,
                                      const print_job_t *current_job)
{
  print_terminal_store_copy(&draft);
  selected_template = draft.active_template < PRINT_TERMINAL_TEMPLATE_COUNT ? draft.active_template : 0U;
  if(current_job != 0) draft.templates[selected_template].job = *current_job;
  page = first_page;
  selected_field = SET_FIELD_NONE;
  input_active = 0U;
  keyboard_drawn = 0U;
  main_drawn = 0U;
  return_pending = 0U;
  job_pending = 0U;
  comm_dirty = 0U;
  network_online_cache_valid = 0U;
  memset(readonly_cache_valid, 0, sizeof(readonly_cache_valid));
  touch_pressed = 0U;
  touch_lock_active = 0U;
  touch_lock_deadline_ms = 0U;
  set_status("READY", C_BLUE);
  active = 1U;
  return 1U;
}

uint8_t print_terminal_settings_begin_from_touch(
    print_terminal_settings_page_t first_page,
    const print_job_t *current_job)
{
  uint8_t result = print_terminal_settings_begin(first_page, current_job);

  if(result != 0U) {
    /* The entry press has already been consumed by print_terminal_service().
     * Keep the latch set until its matching release arrives (or the safety
     * timeout expires), so a stream of sendxy=1 packets cannot re-enter the
     * page or trigger K3 repeatedly. */
    touch_pressed = 1U;
    touch_lock_active = 1U;
    touch_lock_deadline_ms = tester_wifi_print_now_ms() + SET_TOUCH_LOCK_MS;
  }
  return result;
}

uint8_t print_terminal_settings_is_active(void)
{
  return active;
}

void print_terminal_settings_service(void)
{
  lcdm_tjc_event_t event;
  if(active == 0U) return;
  if(input_active != 0U) {
    if(keyboard_drawn == 0U) draw_keyboard_full();
  } else if(main_drawn == 0U) {
    draw_main_full();
  }
  /* Network status, DHCP IP, MAC discovery, and the online colour are all
   * independent dirty rectangles.  Never turn a status transition into a
   * full-screen redraw. */
  draw_network_cells(0U);
  if(touch_lock_active != 0U &&
     (int32_t)(tester_wifi_print_now_ms() - touch_lock_deadline_ms) >= 0) {
    /* Some TFT projects are configured to report only the press packet.  Do
     * not leave the page permanently locked in that case. */
    touch_lock_active = 0U;
    touch_pressed = 0U;
  }
  while(lcdm_tjc_poll_event(&event) != 0U) {
    if(event.type == LCDM_TJC_EVENT_ASCII) {
      handle_ascii(event.ascii);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      if(touch_lock_active != 0U) {
        if(event.touch_event == 0U) {
          touch_lock_active = 0U;
          touch_pressed = 0U;
        }
        continue;
      }
      if(event.touch_event == 0U) {
        touch_pressed = 0U;
        continue;
      }
      if(touch_pressed != 0U) {
        continue;
      }
      touch_pressed = 1U;
      if(input_active != 0U) keyboard_touch(event.x, event.y);
      else handle_main_touch(event.x, event.y);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH) {
      /* Component IDs are retained for factory test scripts: 11..14 map to
       * the same K1..K4 strip as coordinate touches. */
      if(touch_lock_active != 0U) {
        if(event.touch_event == 0U) {
          touch_lock_active = 0U;
          touch_pressed = 0U;
        }
        continue;
      }
      if(event.touch_event == 0U) {
        touch_pressed = 0U;
        continue;
      }
      if(touch_pressed != 0U) {
        continue;
      }
      touch_pressed = 1U;
      if(input_active == 0U && event.component_id >= 11U && event.component_id <= 14U) {
        handle_main_touch((uint16_t)((event.component_id - 11U) * 120U + 10U), SET_KEY_Y + 10U);
      }
    }
  }
}

uint8_t print_terminal_settings_take_return(void)
{
  uint8_t value = return_pending;
  return_pending = 0U;
  return value;
}

uint8_t print_terminal_settings_take_job(print_job_t *out_job)
{
  if(out_job == 0 || job_pending == 0U) return 0U;
  *out_job = pending_job;
  job_pending = 0U;
  return 1U;
}

void print_terminal_settings_set_network_status(const char *text, uint16_t color)
{
  set_text(network_text, sizeof(network_text), text);
  network_color = color;
}
