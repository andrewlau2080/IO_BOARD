#include "first_gen_display.h"

#include "at32f45x_board.h"
#include "lcdm_tjc.h"
#include "tm1637_display.h"

#include <stdio.h>
#include <string.h>

#ifndef FIRST_GEN_DISPLAY_BACKEND_LCDM
#define FIRST_GEN_DISPLAY_BACKEND_LCDM 0
#endif

#ifndef FIRST_GEN_DISPLAY_AUTO_DETECT
#define FIRST_GEN_DISPLAY_AUTO_DETECT 1
#endif

#ifndef LCDM_FONT_PROBE_MODE
#define LCDM_FONT_PROBE_MODE 0
#endif

#define LCDM_TOUCH_K1               11U
#define LCDM_TOUCH_K2               12U
#define LCDM_TOUCH_K3               13U
#define LCDM_TOUCH_K4               14U
#define LCDM_KEY_HOLD_READS         80U
#define LCDM_W                      480U
#define LCDM_H                      272U
#define LCDM_STATUS_Y               42U
#define LCDM_STATUS_H               33U
#define LCDM_DETAIL_Y               110U
#define LCDM_DETAIL_H               68U
#define LCDM_SUB_Y                  184U
#define LCDM_KEY_Y0                 214U
#define LCDM_KEY_Y1                 271U
#define LCDM_KEY_W                  120U
#define LCDM_KEY_H                  58U
#define LCDM_FONT_SMALL             0U
#define LCDM_FONT_LARGE             0U
#define LCDM_BLACK                  0U
#define LCDM_BLUE                   31U
#define LCDM_RED                    63488U
#define LCDM_GREEN                  2016U
#define LCDM_DARK_GREEN             992U
#define LCDM_DARK_RED               32768U
#define LCDM_MAGENTA                63519U
#define LCDM_PURPLE                 30735U
#define LCDM_ORANGE                 64512U
#define LCDM_DARK_ORANGE            49664U
#define LCDM_WHITE                  65535U
#define LCDM_GRAY                   33808U
#define LCDM_DARK_GRAY              16904U
#define LCDM_NAVY                   16U
#define LCDM_ROW_BG                 61374U
#define LCDM_PALE_CYAN              49151U
#define LCDM_PALE_BLUE              50719U
#define LCDM_RESULT_X               16U
#define LCDM_RESULT_W               448U
#define LCDM_PASS_W                 149U
#define LCDM_PRINT_X                165U
#define LCDM_PRINT_W                299U
#define LCDM_PASS_BLINK_READS       50U
#define LCDM_IDLE_SCROLL_READS      12U
#define LCDM_IDLE_BANNER_COLS       32
#define LCDM_IDLE_BANNER_POS_START  28
#define LCDM_IDLE_BANNER_POS_END    -11

static uint8_t lcdm_current_key = FIRST_GEN_KEY_NONE;
static uint16_t lcdm_key_hold_reads;
static uint8_t lcdm_key_waits_release;
static uint8_t display_is_lcdm;
static char lcdm_raw_state_cache[16];
static char lcdm_raw_main_cache[24];
static char lcdm_raw_result_cache[32];
static char lcdm_raw_sub_cache[64];
static char lcdm_raw_touch_cache[64];
static char lcdm_raw_print_cache[16];
static uint8_t lcdm_recover_pending;
static uint8_t lcdm_pass_print_active;
static uint8_t lcdm_pass_blink_on;
static uint8_t lcdm_pass_blink_reads;
static uint8_t lcdm_print_status;
static uint8_t lcdm_idle_banner_active;
static int8_t lcdm_idle_banner_pos;
static uint8_t lcdm_idle_scroll_reads;

volatile uint32_t g_first_gen_lcdm_touch_count;
volatile uint32_t g_first_gen_lcdm_key_press_count;
volatile uint32_t g_first_gen_lcdm_key_release_count;
volatile uint8_t g_first_gen_lcdm_last_event_type;
volatile uint8_t g_first_gen_lcdm_last_touch_event;
volatile uint8_t g_first_gen_lcdm_last_key;
volatile uint16_t g_first_gen_lcdm_last_x;
volatile uint16_t g_first_gen_lcdm_last_y;

static void lcdm_raw_update(const char *state, const char *value, uint16_t state_color);

static void text6_to_cstr(const char text[FIRST_GEN_DISPLAY_DIGITS], char out[8])
{
  uint8_t i;
  int8_t end = (int8_t)FIRST_GEN_DISPLAY_DIGITS - 1;

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    out[i] = text[i];
  }
  out[FIRST_GEN_DISPLAY_DIGITS] = '\0';

  while(end >= 0 && out[end] == ' ') {
    out[end] = '\0';
    end--;
  }
}

static void lcdm_raw_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char cmd[48];

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "fill %u,%u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_raw_cirs(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
  char cmd[48];

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "cirs %u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)r,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_raw_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color)
{
  if((w <= (uint16_t)(r * 2U)) || (h <= (uint16_t)(r * 2U))) {
    lcdm_raw_fill(x, y, w, h, color);
    return;
  }

  lcdm_raw_fill((uint16_t)(x + r), y, (uint16_t)(w - (uint16_t)(r * 2U)), h, color);
  lcdm_raw_fill(x, (uint16_t)(y + r), w, (uint16_t)(h - (uint16_t)(r * 2U)), color);
  lcdm_raw_cirs((uint16_t)(x + r), (uint16_t)(y + r), r, color);
  lcdm_raw_cirs((uint16_t)(x + w - r - 1U), (uint16_t)(y + r), r, color);
  lcdm_raw_cirs((uint16_t)(x + r), (uint16_t)(y + h - r - 1U), r, color);
  lcdm_raw_cirs((uint16_t)(x + w - r - 1U), (uint16_t)(y + h - r - 1U), r, color);
}

static void lcdm_raw_xstr_style(uint16_t x,
                                uint16_t y,
                                uint16_t w,
                                uint16_t h,
                                uint16_t font,
                                uint16_t fg,
                                uint16_t bg,
                                uint8_t align,
                                uint8_t style,
                                const char *text)
{
  char cmd[192];

  if(text == 0) {
    text = "";
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,%u,\"%s\"",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)font,
                 (unsigned int)fg,
                 (unsigned int)bg,
                 (unsigned int)align,
                 (unsigned int)style,
                 text);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_raw_xstr(uint16_t x,
                          uint16_t y,
                          uint16_t w,
                          uint16_t h,
                          uint16_t font,
                          uint16_t fg,
                          uint16_t bg,
                          uint8_t align,
                          const char *text)
{
  lcdm_raw_xstr_style(x, y, w, h, font, fg, bg, align, 1U, text);
}

static void lcdm_raw_xstr_full(uint16_t x,
                               uint16_t y,
                               uint16_t w,
                               uint16_t h,
                               uint16_t font,
                               uint16_t fg,
                               uint16_t bg,
                               uint8_t align,
                               const char *text)
{
  lcdm_raw_xstr(x, y, w, h, font, fg, bg, align, text);
}

static void lcdm_raw_write_cached(char *cache,
                                  uint8_t cache_len,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t w,
                                  uint16_t h,
                                  uint16_t fg,
                                  uint16_t bg,
                                  uint8_t align,
                                  const char *text)
{
  if(text == 0) {
    text = "";
  }

  if(strncmp(cache, text, cache_len) == 0) {
    return;
  }

  (void)snprintf(cache, cache_len, "%s", text);
  lcdm_raw_fill(x, y, w, h, bg);
  lcdm_raw_xstr_full(x, y, w, h, LCDM_FONT_SMALL, fg, bg, align, cache);
}

static void lcdm_raw_write_status_cached(char *cache,
                                         uint8_t cache_len,
                                         uint16_t x,
                                         uint16_t y,
                                         uint16_t w,
                                         uint16_t h,
                                         uint16_t fg,
                                         uint16_t bg,
                                         uint8_t align,
                                         const char *text)
{
  if(text == 0) {
    text = "";
  }

  if(strncmp(cache, text, cache_len) == 0) {
    return;
  }

  (void)snprintf(cache, cache_len, "%s", text);
  lcdm_raw_fill(x, y, w, h, bg);
  lcdm_raw_xstr(x, (uint16_t)(y + 5U), w, (h > 10U) ? (uint16_t)(h - 10U) : h, LCDM_FONT_SMALL, fg, bg, align, cache);
}

static void lcdm_raw_draw_key(uint8_t index, uint8_t active)
{
  uint16_t x = (uint16_t)(index * LCDM_KEY_W);
  uint16_t bg = LCDM_BLUE;
  const char *top = "K1";
  const char *bottom = "SELF/LEARN";

  if(index == 1U) {
    top = "K2";
    bottom = "AUTO";
    bg = LCDM_RED;
  } else if(index == 2U) {
    top = "K3";
    bottom = "RESET";
    bg = LCDM_MAGENTA;
  } else if(index == 3U) {
    top = "K4";
    bottom = "OK/SAVE";
    bg = LCDM_ORANGE;
  }

  if(active != 0U) {
    if(index == 0U) {
      bg = LCDM_NAVY;
    } else if(index == 1U) {
      bg = LCDM_DARK_RED;
    } else if(index == 2U) {
      bg = LCDM_PURPLE;
    } else {
      bg = LCDM_DARK_ORANGE;
    }
  }

  lcdm_raw_fill(x, LCDM_KEY_Y0, LCDM_KEY_W, LCDM_KEY_H, LCDM_WHITE);
  lcdm_raw_round_rect((uint16_t)(x + 2U), (uint16_t)(LCDM_KEY_Y0 + 2U), (uint16_t)(LCDM_KEY_W - 4U), (uint16_t)(LCDM_KEY_H - 4U), 7U, bg);
  lcdm_raw_xstr_full((uint16_t)(x + 4U), (uint16_t)(LCDM_KEY_Y0 + 5U), 112U, 31U, LCDM_FONT_LARGE, LCDM_WHITE, bg, 1U, top);
  lcdm_raw_xstr_full((uint16_t)(x + 4U), (uint16_t)(LCDM_KEY_Y0 + 34U), 112U, 19U, LCDM_FONT_SMALL, LCDM_WHITE, bg, 1U, bottom);
}

static void lcdm_raw_draw_keys(void)
{
  lcdm_raw_draw_key(0U, lcdm_current_key == FIRST_GEN_KEY_SET);
  lcdm_raw_draw_key(1U, lcdm_current_key == FIRST_GEN_KEY_CLEAR);
  lcdm_raw_draw_key(2U, lcdm_current_key == FIRST_GEN_KEY_PLUS);
  lcdm_raw_draw_key(3U, lcdm_current_key == FIRST_GEN_KEY_MINUS);
}

static uint8_t lcdm_key_to_index(uint8_t key, uint8_t *index)
{
  switch(key) {
  case FIRST_GEN_KEY_SET:
    *index = 0U;
    return 1U;
  case FIRST_GEN_KEY_CLEAR:
    *index = 1U;
    return 1U;
  case FIRST_GEN_KEY_PLUS:
    *index = 2U;
    return 1U;
  case FIRST_GEN_KEY_MINUS:
    *index = 3U;
    return 1U;
  default:
    break;
  }
  return 0U;
}

static uint16_t lcdm_key_normal_bg(uint8_t index)
{
  if(index == 1U) {
    return LCDM_RED;
  }
  if(index == 2U) {
    return LCDM_MAGENTA;
  }
  if(index == 3U) {
    return LCDM_ORANGE;
  }
  return LCDM_BLUE;
}

static void lcdm_raw_draw_key_press_marker(uint8_t key, uint8_t active)
{
  uint8_t index;
  uint16_t x;
  uint16_t color;

  if(lcdm_key_to_index(key, &index) == 0U) {
    return;
  }

  x = (uint16_t)(index * LCDM_KEY_W);
  color = (active != 0U) ? LCDM_WHITE : lcdm_key_normal_bg(index);
  lcdm_raw_fill((uint16_t)(x + 6U), (uint16_t)(LCDM_KEY_Y0 + 2U), (uint16_t)(LCDM_KEY_W - 12U), 4U, color);
}

static const char *lcdm_print_status_text(uint8_t status)
{
  if(status == 2U) {
    return "PRINTING";
  }
  if(status == 3U) {
    return "PRINTED";
  }
  return "PRINT READY";
}

static void lcdm_raw_draw_pass_cell(void)
{
  uint16_t bg = (lcdm_pass_blink_on != 0U) ? LCDM_GREEN : LCDM_BLACK;

  lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_PASS_W, LCDM_DETAIL_H, LCDM_WHITE);
  lcdm_raw_round_rect(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_PASS_W, LCDM_DETAIL_H, 8U, bg);
  lcdm_raw_xstr_full((uint16_t)(LCDM_RESULT_X + 4U), (uint16_t)(LCDM_DETAIL_Y + 4U), (uint16_t)(LCDM_PASS_W - 8U), (uint16_t)(LCDM_DETAIL_H - 8U), LCDM_FONT_LARGE, LCDM_WHITE, bg, 1U, "PASS");
}

static void lcdm_raw_draw_print_cell(uint8_t force)
{
  const char *text = lcdm_print_status_text(lcdm_print_status);
  uint16_t bg = LCDM_PALE_CYAN;
  uint16_t fg = LCDM_NAVY;

  if((force == 0U) && (strcmp(lcdm_raw_print_cache, text) == 0)) {
    return;
  }

  if(lcdm_print_status == 2U) {
    bg = LCDM_ORANGE;
    fg = LCDM_WHITE;
  } else if(lcdm_print_status == 3U) {
    bg = LCDM_PALE_BLUE;
    fg = LCDM_NAVY;
  }

  (void)snprintf(lcdm_raw_print_cache, sizeof(lcdm_raw_print_cache), "%s", text);
  lcdm_raw_fill(LCDM_PRINT_X, LCDM_DETAIL_Y, LCDM_PRINT_W, LCDM_DETAIL_H, LCDM_WHITE);
  lcdm_raw_round_rect(LCDM_PRINT_X, LCDM_DETAIL_Y, LCDM_PRINT_W, LCDM_DETAIL_H, 8U, bg);
  lcdm_raw_xstr_full((uint16_t)(LCDM_PRINT_X + 6U), (uint16_t)(LCDM_DETAIL_Y + 4U), (uint16_t)(LCDM_PRINT_W - 12U), (uint16_t)(LCDM_DETAIL_H - 8U), LCDM_FONT_LARGE, fg, bg, 1U, lcdm_raw_print_cache);
}

static void lcdm_raw_draw_pass_print_result(uint8_t status)
{
  lcdm_pass_print_active = 1U;
  lcdm_pass_blink_on = 1U;
  lcdm_pass_blink_reads = 0U;
  lcdm_print_status = status;
  lcdm_raw_result_cache[0] = '\0';
  lcdm_raw_draw_pass_cell();
  lcdm_raw_draw_print_cell(1U);
}

static void lcdm_disable_pass_print_result(void)
{
  if(lcdm_pass_print_active == 0U) {
    return;
  }

  lcdm_pass_print_active = 0U;
  lcdm_pass_blink_on = 0U;
  lcdm_pass_blink_reads = 0U;
  lcdm_print_status = 0U;
  lcdm_raw_print_cache[0] = '\0';
  lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_RESULT_W, LCDM_DETAIL_H, LCDM_WHITE);
}

static void lcdm_pass_blink_service(void)
{
  if(lcdm_pass_print_active == 0U) {
    return;
  }

  lcdm_pass_blink_reads++;
  if(lcdm_pass_blink_reads < LCDM_PASS_BLINK_READS) {
    return;
  }

  lcdm_pass_blink_reads = 0U;
  lcdm_pass_blink_on ^= 1U;
  lcdm_raw_draw_pass_cell();
}

static void lcdm_raw_draw_idle_banner(void)
{
  static const char banner[] = "WIRE TESTER";
  char line[LCDM_IDLE_BANNER_COLS + 1];
  int8_t col;
  uint8_t i;

  for(i = 0U; i < LCDM_IDLE_BANNER_COLS; i++) {
    line[i] = ' ';
  }
  line[LCDM_IDLE_BANNER_COLS] = '\0';

  for(i = 0U; banner[i] != '\0'; i++) {
    col = (int8_t)(lcdm_idle_banner_pos + (int8_t)i);
    if((col >= 0) && (col < LCDM_IDLE_BANNER_COLS)) {
      line[(uint8_t)col] = banner[i];
    }
  }

  lcdm_raw_fill(24U, (uint16_t)(LCDM_DETAIL_Y + 18U), 432U, 34U, LCDM_WHITE);
  lcdm_raw_xstr_full(24U,
                     (uint16_t)(LCDM_DETAIL_Y + 18U),
                     432U,
                     34U,
                     LCDM_FONT_SMALL,
                     LCDM_BLUE,
                     LCDM_WHITE,
                     0U,
                     line);
}

static void lcdm_idle_banner_start(void)
{
  lcdm_idle_banner_active = 1U;
  lcdm_idle_banner_pos = LCDM_IDLE_BANNER_POS_START;
  lcdm_idle_scroll_reads = 0U;
  lcdm_raw_result_cache[0] = '\0';
  lcdm_raw_draw_idle_banner();
}

static void lcdm_idle_banner_stop(void)
{
  lcdm_idle_banner_active = 0U;
  lcdm_idle_scroll_reads = 0U;
}

static void lcdm_idle_banner_service(void)
{
  if(lcdm_idle_banner_active == 0U) {
    return;
  }

  lcdm_idle_scroll_reads++;
  if(lcdm_idle_scroll_reads < LCDM_IDLE_SCROLL_READS) {
    return;
  }

  lcdm_idle_scroll_reads = 0U;
  if(lcdm_idle_banner_pos <= LCDM_IDLE_BANNER_POS_END) {
    lcdm_idle_banner_pos = LCDM_IDLE_BANNER_POS_START;
  } else {
    lcdm_idle_banner_pos--;
  }
  lcdm_raw_draw_idle_banner();
}

static void lcdm_raw_draw_test_frame(void)
{
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_tjc_send_cmd("cls 65535");
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  lcdm_raw_xstr_full(8U, 3U, 320U, 26U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 0U, "WIRE TESTER LCDM");
  lcdm_raw_xstr_full(360U, 3U, 110U, 26U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 2U, "TESTER");
  lcdm_raw_fill(16U, LCDM_STATUS_Y, 448U, LCDM_STATUS_H, LCDM_ROW_BG);
  lcdm_raw_draw_keys();
  lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_RESULT_W, LCDM_DETAIL_H, LCDM_WHITE);
  lcdm_raw_fill(16U, LCDM_SUB_Y, 448U, 28U, LCDM_WHITE);
  lcdm_raw_state_cache[0] = '\0';
  lcdm_raw_main_cache[0] = '\0';
  lcdm_raw_result_cache[0] = '\0';
  lcdm_raw_sub_cache[0] = '\0';
  lcdm_raw_touch_cache[0] = '\0';
  lcdm_raw_print_cache[0] = '\0';
  lcdm_pass_print_active = 0U;
  lcdm_pass_blink_on = 0U;
  lcdm_pass_blink_reads = 0U;
  lcdm_print_status = 0U;
  lcdm_idle_banner_active = 0U;
  lcdm_idle_banner_pos = LCDM_IDLE_BANNER_POS_START;
  lcdm_idle_scroll_reads = 0U;
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
}

#if LCDM_FONT_PROBE_MODE
static void lcdm_raw_draw_font_probe(void)
{
  uint8_t font;
  uint16_t col;
  uint16_t row;
  uint16_t x;
  uint16_t y;
  uint16_t bg;
  char label[8];

  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_tjc_send_cmd("cls 65535");
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 30U, LCDM_NAVY);
  lcdm_raw_xstr(8U, 3U, 464U, 24U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 1U, "LCDM LARGE FONT SELECT");

  for(font = 0U; font < 16U; font++) {
    col = (uint16_t)(font & 3U);
    row = (uint16_t)(font >> 2U);
    x = (uint16_t)(col * 120U);
    y = (uint16_t)(32U + (row * 60U));
    bg = ((font & 1U) != 0U) ? LCDM_ROW_BG : LCDM_WHITE;
    (void)snprintf(label, sizeof(label), "F%u", (unsigned int)font);
    lcdm_raw_fill(x, y, 120U, 60U, bg);
    lcdm_raw_xstr((uint16_t)(x + 4U), (uint16_t)(y + 2U), 112U, 16U, LCDM_FONT_SMALL, LCDM_NAVY, bg, 0U, label);
    lcdm_raw_xstr((uint16_t)(x + 4U), (uint16_t)(y + 18U), 112U, 39U, font, LCDM_NAVY, bg, 1U, "PASS");
  }
}
#endif

static void lcdm_request_recover(void)
{
  lcdm_recover_pending = 1U;
}

static void lcdm_recover_if_requested(void)
{
  if(lcdm_recover_pending == 0U) {
    return;
  }

  lcdm_recover_pending = 0U;
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
}

static void lcdm_raw_update(const char *state, const char *value, uint16_t state_color)
{
#if LCDM_FONT_PROBE_MODE
  (void)state;
  (void)value;
  (void)state_color;
  return;
#else
  const char *state_text = state;
  const char *main_text = "";
  const char *result_text = value;
  const char *sub_text = "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK";
  uint16_t status_color = LCDM_NAVY;
  uint8_t idle_banner = 0U;
  uint8_t pass_print_status = 0U;

  if(state == 0) {
    state = "";
  }
  if(value == 0) {
    value = "";
  }

  if(strncmp(value, "AUTO", 4U) == 0) {
    state_text = "READY";
    main_text = "AUTO TEST";
    result_text = "";
    idle_banner = 1U;
  } else if(strncmp(state, "SELF", 4U) == 0) {
    state_text = "SELF";
    main_text = "SELF TEST";
    result_text = value;
    sub_text = "CHECKING...  K3 RESET";
  } else if(strncmp(state, "AUTO", 4U) == 0 || strncmp(state, "TEST", 4U) == 0) {
    state_text = "AUTO";
    main_text = "AUTO TEST";
    result_text = value;
    sub_text = "RUNNING  K3 RESET";
  } else if(strncmp(state, "LEARN", 5U) == 0) {
    state_text = "";
    main_text = "";
    result_text = "LEARN START";
    sub_text = "";
  } else if(strncmp(state, "PASS", 4U) == 0) {
    state_text = "RESULT";
    main_text = "TEST COMPLETE";
    result_text = "PASS";
    pass_print_status = 1U;
    sub_text = "PRINT READY  REMOVE HARNESS";
  } else if(strncmp(state, "NG", 2U) == 0) {
    state_text = "RESULT";
    main_text = "TEST COMPLETE";
    result_text = value;
    sub_text = "SHORT CIRCUIT  K3 RESET";
  } else if(strncmp(state, "PRINT", 5U) == 0) {
    state_text = "RESULT";
    main_text = "TEST COMPLETE";
    result_text = value;
    if(strncmp(value, "Printg", 6U) == 0) {
      pass_print_status = 2U;
    } else if(strncmp(value, "Printd", 6U) == 0) {
      pass_print_status = 3U;
    } else {
      pass_print_status = 1U;
    }
  }

  if((strncmp(state, "NG", 2U) == 0) || (strncmp(state, "Er", 2U) == 0)) {
    status_color = LCDM_RED;
  }
  (void)state_color;

  lcdm_raw_write_status_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache), 24U, LCDM_STATUS_Y, 160U, LCDM_STATUS_H, status_color, LCDM_ROW_BG, 0U, state_text);
  lcdm_raw_write_status_cached(lcdm_raw_main_cache, sizeof(lcdm_raw_main_cache), 184U, LCDM_STATUS_Y, 272U, LCDM_STATUS_H, status_color, LCDM_ROW_BG, 0U, main_text);
  if(pass_print_status != 0U) {
    lcdm_idle_banner_stop();
    lcdm_raw_draw_pass_print_result(pass_print_status);
  } else {
    lcdm_disable_pass_print_result();
    if(idle_banner != 0U) {
      lcdm_idle_banner_start();
    } else {
      lcdm_idle_banner_stop();
      lcdm_raw_write_cached(lcdm_raw_result_cache, sizeof(lcdm_raw_result_cache), 24U, (uint16_t)(LCDM_DETAIL_Y + 18U), 432U, 34U, LCDM_BLUE, LCDM_WHITE, 0U, result_text);
    }
  }
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U, LCDM_GRAY, LCDM_WHITE, 1U, sub_text);
#endif
}

static void lcdm_raw_update_learn_summary(uint16_t out_count, uint16_t in_count, uint32_t total)
{
  char result_text[32];

  if(out_count > 999U) {
    out_count = 999U;
  }
  if(in_count > 999U) {
    in_count = 999U;
  }
  if(total > 99UL) {
    total = 99UL;
  }

  (void)snprintf(result_text,
                 sizeof(result_text),
                 "%03u-%03u  TOTAL-%02u",
                 (unsigned int)out_count,
                 (unsigned int)in_count,
                 (unsigned int)total);

  lcdm_disable_pass_print_result();
  lcdm_raw_write_status_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache), 24U, LCDM_STATUS_Y, 160U, LCDM_STATUS_H, LCDM_NAVY, LCDM_ROW_BG, 0U, "");
  lcdm_raw_write_status_cached(lcdm_raw_main_cache, sizeof(lcdm_raw_main_cache), 184U, LCDM_STATUS_Y, 272U, LCDM_STATUS_H, LCDM_NAVY, LCDM_ROW_BG, 0U, "");
  lcdm_raw_write_cached(lcdm_raw_result_cache, sizeof(lcdm_raw_result_cache), 24U, (uint16_t)(LCDM_DETAIL_Y + 18U), 432U, 34U, LCDM_BLUE, LCDM_WHITE, 1U, result_text);
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U, LCDM_GRAY, LCDM_WHITE, 1U, "");
}

static uint8_t lcdm_coord_to_key_direct(uint16_t x, uint16_t y)
{
  if((x > (LCDM_W - 1U)) || (y < LCDM_KEY_Y0) || (y > LCDM_KEY_Y1)) {
    return FIRST_GEN_KEY_NONE;
  }

  if(x <= 119U) {
    return FIRST_GEN_KEY_SET;
  }
  if(x <= 239U) {
    return FIRST_GEN_KEY_CLEAR;
  }
  if(x <= 359U) {
    return FIRST_GEN_KEY_PLUS;
  }
  return FIRST_GEN_KEY_MINUS;
}

static uint8_t lcdm_coord_to_key(uint16_t x, uint16_t y)
{
  return lcdm_coord_to_key_direct(x, y);
}

static void lcdm_raw_show_touch(uint16_t x, uint16_t y, uint8_t key)
{
  char text[32];
  const char *key_name = "---";

  switch(key) {
  case FIRST_GEN_KEY_SET: key_name = "K1"; break;
  case FIRST_GEN_KEY_CLEAR: key_name = "K2"; break;
  case FIRST_GEN_KEY_PLUS: key_name = "K3"; break;
  case FIRST_GEN_KEY_MINUS: key_name = "K4"; break;
  default: break;
  }

  (void)snprintf(text,
                 sizeof(text),
                 "T %03u,%03u -> %s",
                 (unsigned int)x,
                 (unsigned int)y,
                 key_name);
  if(strcmp(lcdm_raw_touch_cache, text) != 0) {
    (void)snprintf(lcdm_raw_touch_cache, sizeof(lcdm_raw_touch_cache), "%s", text);
    lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U, LCDM_BLUE, LCDM_WHITE, 1U, lcdm_raw_touch_cache);
  }
}

static uint8_t lcdm_latch_key(uint8_t key)
{
  uint8_t old_key;

  if(key == FIRST_GEN_KEY_NONE) {
    return FIRST_GEN_KEY_NONE;
  }

  if(lcdm_current_key == key && lcdm_key_waits_release != 0U) {
    return key;
  }

  old_key = lcdm_current_key;
  lcdm_current_key = key;
  g_first_gen_lcdm_last_key = key;
  g_first_gen_lcdm_key_press_count++;
  lcdm_key_hold_reads = 0U;
  lcdm_key_waits_release = 1U;
  lcdm_raw_draw_key_press_marker(old_key, 0U);
  lcdm_raw_draw_key_press_marker(key, 1U);
  return key;
}

static void lcdm_release_key(uint8_t key)
{
  if((key == FIRST_GEN_KEY_NONE) || (lcdm_current_key == key)) {
    uint8_t old_key = lcdm_current_key;
    lcdm_current_key = FIRST_GEN_KEY_NONE;
    g_first_gen_lcdm_last_key = FIRST_GEN_KEY_NONE;
    g_first_gen_lcdm_key_release_count++;
    lcdm_key_hold_reads = 0U;
    lcdm_key_waits_release = 0U;
    lcdm_raw_draw_key_press_marker(old_key, 0U);
  }
}

static void lcdm_display_init(void)
{
  lcdm_current_key = FIRST_GEN_KEY_NONE;
  lcdm_key_hold_reads = 0U;
  lcdm_key_waits_release = 0U;
  g_first_gen_lcdm_touch_count = 0U;
  g_first_gen_lcdm_key_press_count = 0U;
  g_first_gen_lcdm_key_release_count = 0U;
  g_first_gen_lcdm_last_event_type = 0U;
  g_first_gen_lcdm_last_touch_event = 0U;
  g_first_gen_lcdm_last_key = FIRST_GEN_KEY_NONE;
  g_first_gen_lcdm_last_x = 0U;
  g_first_gen_lcdm_last_y = 0U;
  lcdm_recover_pending = 0U;
  lcdm_pass_print_active = 0U;
  lcdm_pass_blink_on = 0U;
  lcdm_pass_blink_reads = 0U;
  lcdm_print_status = 0U;
  lcdm_idle_banner_active = 0U;
  lcdm_idle_banner_pos = LCDM_IDLE_BANNER_POS_START;
  lcdm_idle_scroll_reads = 0U;
  lcdm_tjc_init();
  lcdm_tjc_force_baudrate(LCDM_TJC_BAUDRATE);
  delay_ms(150U);
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("dim=100");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
#if LCDM_FONT_PROBE_MODE
  lcdm_raw_draw_font_probe();
  return;
#endif
  lcdm_raw_draw_test_frame();
  lcdm_raw_update("READY", "AUTO", LCDM_BLUE);
}

static uint8_t lcdm_display_key_read_raw(void)
{
  lcdm_tjc_event_t event;
  uint8_t key;

  while(lcdm_tjc_poll_event(&event) != 0U) {
    g_first_gen_lcdm_touch_count++;
    g_first_gen_lcdm_last_event_type = (uint8_t)event.type;
    g_first_gen_lcdm_last_touch_event = event.touch_event;
    if(event.type == LCDM_TJC_EVENT_TOUCH) {
      g_first_gen_lcdm_last_x = (uint16_t)event.component_id;
      g_first_gen_lcdm_last_y = event.page_id;
      lcdm_request_recover();
      continue;
    } else if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      key = lcdm_coord_to_key(event.x, event.y);
      g_first_gen_lcdm_last_x = event.x;
      g_first_gen_lcdm_last_y = event.y;
      if(key == FIRST_GEN_KEY_NONE) {
        lcdm_raw_show_touch(event.x, event.y, key);
      }
      if(event.touch_event == 0U) {
        lcdm_release_key(key);
        return FIRST_GEN_KEY_NONE;
      }
      if(key != FIRST_GEN_KEY_NONE) {
        return lcdm_latch_key(key);
      }
    } else if(event.type == LCDM_TJC_EVENT_ASCII) {
      lcdm_request_recover();
      continue;
    }
  }

  lcdm_recover_if_requested();
  lcdm_idle_banner_service();
  lcdm_pass_blink_service();

  if(lcdm_key_hold_reads != 0U) {
    lcdm_key_hold_reads--;
    return lcdm_current_key;
  }

  if(lcdm_key_waits_release == 0U) {
    lcdm_current_key = FIRST_GEN_KEY_NONE;
  }
  return lcdm_current_key;
}

void first_gen_display_init(void)
{
#if FIRST_GEN_DISPLAY_BACKEND_LCDM
  display_is_lcdm = 1U;
#elif FIRST_GEN_DISPLAY_AUTO_DETECT
  display_is_lcdm = lcdm_tjc_probe(0U);
#else
  display_is_lcdm = 0U;
#endif

  if(display_is_lcdm != 0U) {
    lcdm_display_init();
  } else {
    tm1637_display_init();
  }
}

void first_gen_display_clear(void)
{
  if(display_is_lcdm != 0U) {
    lcdm_raw_update("READY", "AUTO", LCDM_BLUE);
  } else {
    tm1637_display_clear();
  }
}

uint8_t first_gen_display_key_read_raw(void)
{
  if(display_is_lcdm != 0U) {
    return lcdm_display_key_read_raw();
  }
  return tm1637_key_read_raw();
}

void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS])
{
  char text[8];
  uint8_t i;

  if(display_is_lcdm == 0U) {
    tm1637_display_write_raw6(segments);
    return;
  }

  if(segments == 0) {
    return;
  }

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    text[i] = (segments[i] == 0U) ? ' ' : '-';
  }
  text[FIRST_GEN_DISPLAY_DIGITS] = '\0';
  lcdm_raw_update("TEST", text, LCDM_BLUE);
}

void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS])
{
  char value[8];

  if(display_is_lcdm == 0U) {
    tm1637_display_write_text6(text);
    return;
  }

  if(text == 0) {
    return;
  }

  text6_to_cstr(text, value);

  if(strncmp(value, "PASS", 4U) == 0) {
    lcdm_raw_update("PASS", value, LCDM_GREEN);
  } else if(strncmp(value, "NG", 2U) == 0 || strncmp(value, "Er", 2U) == 0) {
    lcdm_raw_update("NG", value, LCDM_RED);
  } else if(strncmp(value, "Prnt", 4U) == 0 || strncmp(value, "Printg", 6U) == 0 || strncmp(value, "Printd", 6U) == 0) {
    lcdm_raw_update("PRINT", value, LCDM_BLUE);
  } else if(strncmp(value, "LEArn", 5U) == 0) {
    lcdm_raw_update("LEARN", value, LCDM_BLUE);
  } else if(strncmp(value, "AUTO", 4U) == 0) {
    lcdm_raw_update("AUTO", value, LCDM_BLUE);
  } else if(strncmp(value, "SELF", 4U) == 0) {
    lcdm_raw_update("SELF", value, LCDM_BLUE);
  } else {
    lcdm_raw_update("TEST", value, LCDM_BLUE);
  }
}

void first_gen_display_write_learn_summary(uint16_t out_count, uint16_t in_count, uint32_t total)
{
  char text[FIRST_GEN_DISPLAY_DIGITS];

  if(display_is_lcdm != 0U) {
    lcdm_raw_update_learn_summary(out_count, in_count, total);
    return;
  }

  if(out_count > 999U) {
    out_count = 999U;
  }
  if(in_count > 999U) {
    in_count = 999U;
  }

  (void)total;
  text[0] = (char)('0' + ((out_count / 100U) % 10U));
  text[1] = (char)('0' + ((out_count / 10U) % 10U));
  text[2] = (char)('0' + (out_count % 10U));
  text[3] = (char)('0' + ((in_count / 100U) % 10U));
  text[4] = (char)('0' + ((in_count / 10U) % 10U));
  text[5] = (char)('0' + (in_count % 10U));
  tm1637_display_write_text6(text);
}
