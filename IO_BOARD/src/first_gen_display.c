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

#ifndef LCDM_TOUCH_DEBUG_TEXT
#define LCDM_TOUCH_DEBUG_TEXT 0
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
#define LCDM_FONT_SCROLL            3U
#define LCDM_FONT_TABLE             0U
#define LCDM_BLACK                  0U
#define LCDM_BLUE                   31U
#define LCDM_RED                    63488U
#define LCDM_GREEN                  2016U
#define LCDM_DARK_GREEN             992U
#define LCDM_DONE_GREEN             1504U
#define LCDM_DONE_DARK_GREEN        736U
#define LCDM_DARK_RED               32768U
#define LCDM_MAGENTA                63519U
#define LCDM_PURPLE                 30735U
#define LCDM_SOFT_PINK              64593U
#define LCDM_DEEP_PINK              63488U
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
#define LCDM_BANNER_Y               122U
#define LCDM_BANNER_H                34U
#define LCDM_TOTAL_PAIRS            47U
#define LCDM_TOTAL_POINTS           94U
#define LCDM_PAIR_PAGE_SIZE         24U
#define LCDM_AUTO_LIST_PAGE_SIZE     4U
#define LCDM_AUTO_LIST_ROW_H        28U
#define LCDM_AUTO_LIST_Y0           72U
#define LCDM_AUTO_LIST_H            (LCDM_AUTO_LIST_PAGE_SIZE * LCDM_AUTO_LIST_ROW_H)
#define LCDM_AUTO_LINE_TEXT_MAX     72U
#define LCDM_AUTO_LINE_CHAR_W        9U
#define LCDM_AUTO_LINE_PAD_W        10U
#define LCDM_AUTO_LINE_TOTAL_W     440U
#define LCDM_AUTO_LINE_OUT_MIN_W    72U
#define LCDM_AUTO_LIST_MAX_PAGE     (((LCDM_TOTAL_POINTS - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U)
#define LCDM_TABLE_NG_WORDS         3U
#define LCDM_PASS_BLINK_READS       50U
#define LCDM_IDLE_SCROLL_READS      1920U
#define LCDM_IDLE_BANNER_STEP_X     12
#define LCDM_IDLE_BANNER_VIEW_X     16
#define LCDM_IDLE_BANNER_VIEW_W     448
#define LCDM_IDLE_BANNER_COLS       22
#define LCDM_IDLE_BANNER_CHAR_COUNT 11
#define LCDM_IDLE_BANNER_CHAR_W     20
#define LCDM_IDLE_BANNER_TEXT_W     (LCDM_IDLE_BANNER_CHAR_COUNT * LCDM_IDLE_BANNER_CHAR_W)
#define LCDM_IDLE_BANNER_POS_START  (LCDM_IDLE_BANNER_COLS - 1)
#define LCDM_IDLE_BANNER_POS_END    (-LCDM_IDLE_BANNER_CHAR_COUNT)

static uint8_t lcdm_current_key = FIRST_GEN_KEY_NONE;
static uint16_t lcdm_key_hold_reads;
static uint8_t lcdm_key_waits_release;
static uint8_t display_is_lcdm;
static char lcdm_raw_state_cache[32];
static char lcdm_raw_main_cache[24];
static char lcdm_raw_result_cache[32];
static char lcdm_raw_sub_cache[64];
static char lcdm_raw_touch_cache[64];
static char lcdm_raw_print_cache[16];
static char lcdm_learn_result_left_cache[32];
static char lcdm_learn_result_right_cache[32];
static char lcdm_idle_banner_text[16];
static uint8_t lcdm_recover_pending;
static uint8_t lcdm_pass_print_active;
static uint8_t lcdm_pass_blink_on;
static uint8_t lcdm_pass_blink_reads;
static uint8_t lcdm_print_status;
static uint8_t lcdm_idle_banner_active;
static int8_t lcdm_idle_banner_pos;
static uint16_t lcdm_idle_scroll_reads;
static uint8_t lcdm_layout_mode;
static uint8_t lcdm_table_page_cache;
static uint16_t lcdm_table_active_cache;
static uint32_t lcdm_table_ng_in_bits[LCDM_TABLE_NG_WORDS];
static uint32_t lcdm_table_ng_out_bits[LCDM_TABLE_NG_WORDS];
static uint16_t lcdm_learn_in_bg[LCDM_TOTAL_POINTS + 1U];
static uint16_t lcdm_learn_out_bg[LCDM_TOTAL_POINTS + 1U];
static char lcdm_auto_line_cache[LCDM_TOTAL_POINTS + 1U][256];
static char lcdm_auto_drawn_line_cache[LCDM_AUTO_LIST_PAGE_SIZE][256];
static char lcdm_auto_footer_cache[80];
static char lcdm_auto_diag_a_cache[24];
static char lcdm_auto_diag_b_cache[24];
static uint8_t lcdm_auto_diag_visible;
static char lcdm_layout_top_cache[32];
static char lcdm_learn_footer_scan_cache[20];
static char lcdm_learn_footer_pairs_cache[24];
static char lcdm_learn_footer_points_cache[24];
static uint8_t lcdm_auto_page_cache;
static uint8_t lcdm_auto_drawn_page_cache;
static uint16_t lcdm_auto_active_cache;
static uint16_t lcdm_auto_line_count;
static uint32_t lcdm_auto_point_count;
static uint8_t lcdm_learn_outcome_blink_on;

volatile uint32_t g_first_gen_lcdm_touch_count;
volatile uint32_t g_first_gen_lcdm_key_press_count;
volatile uint32_t g_first_gen_lcdm_key_release_count;
volatile uint8_t g_first_gen_lcdm_last_event_type;
volatile uint8_t g_first_gen_lcdm_last_touch_event;
volatile uint8_t g_first_gen_lcdm_last_key;
volatile uint16_t g_first_gen_lcdm_last_x;
volatile uint16_t g_first_gen_lcdm_last_y;

static void lcdm_raw_update(const char *state, const char *value, uint16_t state_color);
static void lcdm_prepare_standard_page(const char *top_right);
static void lcdm_prepare_table_page(uint8_t page, uint16_t active_point);
static void lcdm_prepare_auto_test_page(uint8_t page, uint16_t active_point, uint8_t done);
static void lcdm_table_ng_clear(void);
static void lcdm_learn_table_clear(void);
static void lcdm_auto_test_clear(void);
static uint8_t lcdm_auto_test_point_visible(uint8_t page, uint16_t point);
static void lcdm_auto_test_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *y);
static void lcdm_draw_auto_test_line(uint8_t page, uint16_t point, uint16_t active_point);
static void lcdm_draw_auto_test_all(uint8_t page, uint16_t active_point);
static uint16_t lcdm_auto_test_set_line(uint16_t point, const char *line);
static void lcdm_draw_auto_footer(uint8_t done);

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

static uint8_t lcdm_is_digit_pair(const char *text)
{
  uint8_t i;

  if(text == 0) {
    return 0U;
  }

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    if((text[i] < '0') || (text[i] > '9')) {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t lcdm_parse_digit_pair(const char *text, uint16_t *left, uint16_t *right)
{
  if(lcdm_is_digit_pair(text) == 0U || left == 0 || right == 0) {
    return 0U;
  }

  *left = (uint16_t)(((text[0] - '0') * 100U) + ((text[1] - '0') * 10U) + (text[2] - '0'));
  *right = (uint16_t)(((text[3] - '0') * 100U) + ((text[4] - '0') * 10U) + (text[5] - '0'));
  return 1U;
}

static void lcdm_format_pair_text(uint16_t left, uint16_t right, char out[32])
{
  if(left == right) {
    (void)snprintf(out,
                   32,
                   "%03u - %03u",
                   (unsigned int)left,
                   (unsigned int)right);
  } else {
    (void)snprintf(out,
                   32,
                   "OUT%03u IN%03u",
                   (unsigned int)left,
                   (unsigned int)right);
  }
}

static void lcdm_format_page_text(uint16_t pair, char out[64])
{
  uint16_t page;

  if(pair == 0U) {
    pair = 1U;
  }
  page = (uint16_t)(((pair - 1U) / LCDM_PAIR_PAGE_SIZE) + 1U);
  (void)snprintf(out,
                 64,
                 "PAGE %u/%u  TOTAL %03u PAIRS/%03u POINTS",
                 (unsigned int)page,
                 (unsigned int)(((LCDM_TOTAL_PAIRS - 1U) / LCDM_PAIR_PAGE_SIZE) + 1U),
                 (unsigned int)LCDM_TOTAL_PAIRS,
                 (unsigned int)LCDM_TOTAL_POINTS);
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
  char cmd[384];

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

static void lcdm_raw_write_cached_font(char *cache,
                                       uint8_t cache_len,
                                       uint16_t x,
                                       uint16_t y,
                                       uint16_t w,
                                       uint16_t h,
                                       uint16_t font,
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
  lcdm_raw_xstr_full(x, y, w, h, font, fg, bg, align, cache);
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
  (void)fg;
  lcdm_raw_xstr(x, (uint16_t)(y + 5U), w, (h > 10U) ? (uint16_t)(h - 10U) : h, LCDM_FONT_SMALL, LCDM_BLUE, bg, align, cache);
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
    bg = LCDM_SOFT_PINK;
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
      bg = LCDM_DEEP_PINK;
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

static void lcdm_raw_draw_key_press_marker(uint8_t key, uint8_t active)
{
  uint8_t index;

  if(lcdm_key_to_index(key, &index) == 0U) {
    return;
  }

  lcdm_raw_draw_key(index, active);
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
  const char *banner = lcdm_idle_banner_text;
  char text[LCDM_IDLE_BANNER_CHAR_COUNT + 1];
  int8_t col;
  int8_t first_col = 0;
  int8_t last_col = 0;
  uint8_t len = 0U;
  uint8_t i;

  lcdm_raw_fill(LCDM_IDLE_BANNER_VIEW_X, LCDM_STATUS_Y, LCDM_IDLE_BANNER_VIEW_W, LCDM_STATUS_H, LCDM_ROW_BG);

  for(i = 0U; banner[i] != '\0'; i++) {
    col = (int8_t)(lcdm_idle_banner_pos + (int8_t)i);
    if((col >= 0) && (col < (int8_t)LCDM_IDLE_BANNER_COLS)) {
      first_col = col;
      break;
    }
  }

  if(banner[i] == '\0') {
    return;
  }

  for(; banner[i] != '\0'; i++) {
    col = (int8_t)(lcdm_idle_banner_pos + (int8_t)i);
    if((col < 0) || (col >= (int8_t)LCDM_IDLE_BANNER_COLS)) {
      break;
    }
    text[len] = banner[i];
    len++;
    last_col = col;
  }
  text[len] = '\0';

  lcdm_raw_xstr_full((uint16_t)(LCDM_IDLE_BANNER_VIEW_X + ((uint16_t)first_col * LCDM_IDLE_BANNER_CHAR_W)),
                     LCDM_STATUS_Y,
                     (uint16_t)(((uint16_t)(last_col - first_col + 1) * LCDM_IDLE_BANNER_CHAR_W)),
                     LCDM_STATUS_H,
                     LCDM_FONT_SCROLL,
                     LCDM_BLUE,
                     LCDM_ROW_BG,
                     0U,
                     text);
}

static void lcdm_idle_banner_start(const char *text)
{
  if(text == 0 || text[0] == '\0') {
    text = "WIRE TESTER";
  }

  if((lcdm_idle_banner_active != 0U) && (strcmp(lcdm_idle_banner_text, text) == 0)) {
    return;
  }

  (void)snprintf(lcdm_idle_banner_text, sizeof(lcdm_idle_banner_text), "%s", text);
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
  lcdm_idle_banner_text[0] = '\0';
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

static void lcdm_idle_banner_step_now(void)
{
  if(lcdm_idle_banner_active == 0U) {
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
  lcdm_raw_xstr_full(8U, 3U, 220U, 26U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 0U, "STANDARD CABLE");
  lcdm_raw_xstr_full(300U, 3U, 172U, 26U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 2U, "WIRE TESTER");
  lcdm_raw_fill(16U, LCDM_STATUS_Y, 448U, LCDM_STATUS_H, LCDM_ROW_BG);
  lcdm_raw_draw_keys();
  lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_RESULT_W, LCDM_DETAIL_H, LCDM_WHITE);
  lcdm_raw_fill(16U, LCDM_BANNER_Y, 448U, LCDM_BANNER_H, LCDM_PALE_CYAN);
  lcdm_raw_xstr_full(24U,
                     (uint16_t)(LCDM_BANNER_Y + 4U),
                     432U,
                     (uint16_t)(LCDM_BANNER_H - 8U),
                     LCDM_FONT_SMALL,
                     LCDM_BLUE,
                     LCDM_PALE_CYAN,
                     1U,
                     "TOTAL - 047 PAIRS / 094 POINTS");
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
  lcdm_layout_mode = 0U;
  lcdm_table_page_cache = 0U;
  lcdm_table_active_cache = 0U;
  lcdm_table_ng_clear();
  lcdm_learn_table_clear();
  lcdm_layout_top_cache[0] = '\0';
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
}

static void lcdm_reset_text_caches(void)
{
  lcdm_raw_state_cache[0] = '\0';
  lcdm_raw_main_cache[0] = '\0';
  lcdm_raw_result_cache[0] = '\0';
  lcdm_raw_sub_cache[0] = '\0';
  lcdm_raw_touch_cache[0] = '\0';
  lcdm_raw_print_cache[0] = '\0';
  lcdm_learn_result_left_cache[0] = '\0';
  lcdm_learn_result_right_cache[0] = '\0';
}

static void lcdm_reset_dynamic_effects(void)
{
  lcdm_disable_pass_print_result();
  lcdm_idle_banner_stop();
  lcdm_pass_blink_on = 0U;
  lcdm_pass_blink_reads = 0U;
  lcdm_print_status = 0U;
  lcdm_learn_outcome_blink_on = 0U;
}

static void lcdm_draw_common_header(const char *top_right)
{
  if(top_right == 0) {
    top_right = "";
  }

  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  (void)top_right;
  lcdm_raw_xstr_full(0U, 1U, LCDM_W, 30U, LCDM_FONT_SCROLL, LCDM_WHITE, LCDM_NAVY, 1U, "STANDARD CABLE");
  lcdm_raw_draw_keys();
  lcdm_reset_text_caches();
}

static void lcdm_draw_auto_header(const char *title)
{
  if(title == 0) {
    title = "";
  }

  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  lcdm_raw_xstr_full(0U, 1U, LCDM_W, 30U, LCDM_FONT_SCROLL, LCDM_WHITE, LCDM_NAVY, 1U, "STANDARD CABLE");
  lcdm_raw_fill(0U, 34U, LCDM_W, 36U, LCDM_ROW_BG);
  lcdm_raw_xstr_full(0U, 35U, LCDM_W, 34U, LCDM_FONT_SCROLL, LCDM_BLUE, LCDM_ROW_BG, 1U, title);
  lcdm_raw_draw_keys();
  lcdm_reset_text_caches();
}

static void lcdm_draw_body_text(uint16_t y, uint16_t h, const char *text, uint16_t fg, uint16_t bg)
{
  lcdm_raw_fill(24U, y, 432U, h, bg);
  lcdm_raw_xstr_full(24U, y, 432U, h, LCDM_FONT_SMALL, fg, bg, 1U, text);
}

static void lcdm_draw_learning_pair_text(const char *text)
{
  lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
  lcdm_raw_xstr_full(24U, 84U, 432U, 82U, LCDM_FONT_SCROLL, LCDM_NAVY, LCDM_WHITE, 1U, text);
}

static void lcdm_draw_split_body(uint16_t left_x,
                                 uint16_t y,
                                 uint16_t left_w,
                                 uint16_t right_w,
                                 uint16_t h,
                                 const char *left_text,
                                 const char *right_text,
                                 uint16_t left_fg,
                                 uint16_t left_bg,
                                 uint16_t right_fg,
                                 uint16_t right_bg)
{
  lcdm_raw_fill(left_x, y, left_w, h, left_bg);
  lcdm_raw_fill((uint16_t)(left_x + left_w), y, right_w, h, right_bg);
  lcdm_raw_fill((uint16_t)(left_x + left_w - 1U), y, 2U, h, LCDM_BLACK);
  lcdm_raw_xstr_full(left_x, y, left_w, h, LCDM_FONT_LARGE, left_fg, left_bg, 1U, left_text);
  lcdm_raw_xstr_full((uint16_t)(left_x + left_w), y, right_w, h, LCDM_FONT_LARGE, right_fg, right_bg, 1U, right_text);
}

static void lcdm_draw_result_pass_body(const char *detail_text)
{
  char total_text[8] = "TOTAL";
  char pairs_text[24];
  char points_text[24];
  unsigned int pairs = 0U;
  unsigned int points = 0U;

  if(detail_text == 0) {
    detail_text = "";
  }

  if(sscanf(detail_text, "TOTAL - %u PAIRS/%u POINTS", &pairs, &points) == 2) {
    (void)snprintf(pairs_text, sizeof(pairs_text), "%03u PAIRS", pairs);
    (void)snprintf(points_text, sizeof(points_text), "%u POINTS", points);
  } else {
    (void)snprintf(pairs_text, sizeof(pairs_text), "%s", detail_text);
    points_text[0] = '\0';
  }

  lcdm_raw_fill(24U, 84U, 216U, 82U, LCDM_GREEN);
  lcdm_raw_fill(240U, 84U, 216U, 82U, LCDM_WHITE);
  lcdm_raw_fill(239U, 84U, 2U, 82U, LCDM_BLACK);
  lcdm_raw_xstr_full(18U, 84U, 228U, 82U, LCDM_FONT_SCROLL, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(19U, 84U, 228U, 82U, LCDM_FONT_SCROLL, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(18U, 85U, 228U, 82U, LCDM_FONT_SCROLL, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(19U, 85U, 228U, 82U, LCDM_FONT_SCROLL, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(244U, 84U, 208U, 26U, LCDM_FONT_SCROLL, LCDM_NAVY, LCDM_WHITE, 1U, total_text);
  lcdm_raw_xstr_full(244U, 111U, 208U, 26U, LCDM_FONT_SCROLL, LCDM_NAVY, LCDM_WHITE, 1U, pairs_text);
  lcdm_raw_xstr_full(244U, 138U, 208U, 28U, LCDM_FONT_SCROLL, LCDM_NAVY, LCDM_WHITE, 1U, points_text);
}

static void lcdm_draw_top_split_labels(const char *left_text, const char *right_text, uint16_t fg, uint16_t bg)
{
  (void)fg;
  lcdm_raw_fill(24U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, bg);
  lcdm_raw_fill(240U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, bg);
  lcdm_raw_fill(240U, LCDM_STATUS_Y, 2U, LCDM_STATUS_H, LCDM_BLACK);
  lcdm_raw_xstr_full(24U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, LCDM_FONT_SCROLL, LCDM_BLUE, bg, 1U, left_text);
  lcdm_raw_xstr_full(240U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, LCDM_FONT_SCROLL, LCDM_BLUE, bg, 1U, right_text);
}

static void lcdm_prepare_standard_page(const char *top_right)
{
  if(top_right == 0) {
    top_right = "";
  }

  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");

  if(lcdm_layout_mode == 1U && strcmp(lcdm_layout_top_cache, top_right) == 0) {
    return;
  }

  lcdm_layout_mode = 1U;
  lcdm_table_page_cache = 0U;
  lcdm_table_active_cache = 0U;
  (void)snprintf(lcdm_layout_top_cache, sizeof(lcdm_layout_top_cache), "%s", top_right);
  lcdm_reset_dynamic_effects();
  lcdm_draw_common_header(top_right);
  lcdm_raw_fill(16U, LCDM_STATUS_Y, 448U, LCDM_STATUS_H, LCDM_ROW_BG);
  lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_RESULT_W, LCDM_DETAIL_H, LCDM_WHITE);
  lcdm_raw_fill(16U, LCDM_SUB_Y, 448U, 28U, LCDM_WHITE);
}

static uint8_t lcdm_table_point_visible(uint8_t page, uint16_t point)
{
  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }
  if(page == 1U) {
    return (point >= 1U && point <= 47U) ? 1U : 0U;
  }
  return (point >= 48U && point <= 94U) ? 1U : 0U;
}

static void lcdm_table_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *in_y);

static void lcdm_table_ng_clear(void)
{
  uint8_t i;

  for(i = 0U; i < LCDM_TABLE_NG_WORDS; i++) {
    lcdm_table_ng_in_bits[i] = 0U;
    lcdm_table_ng_out_bits[i] = 0U;
  }
}

static void lcdm_table_ng_set_bit(uint32_t bits[LCDM_TABLE_NG_WORDS], uint16_t point)
{
  uint8_t word;
  uint8_t bit;

  if(point == 0U || point > LCDM_TOTAL_POINTS) {
    return;
  }

  word = (uint8_t)((point - 1U) >> 5);
  bit = (uint8_t)((point - 1U) & 0x1FU);
  if(word < LCDM_TABLE_NG_WORDS) {
    bits[word] |= (1UL << bit);
  }
}

static uint8_t lcdm_table_ng_has_bit(const uint32_t bits[LCDM_TABLE_NG_WORDS], uint16_t point)
{
  uint8_t word;
  uint8_t bit;

  if(point == 0U || point > LCDM_TOTAL_POINTS) {
    return 0U;
  }

  word = (uint8_t)((point - 1U) >> 5);
  bit = (uint8_t)((point - 1U) & 0x1FU);
  if(word >= LCDM_TABLE_NG_WORDS) {
    return 0U;
  }
  return ((bits[word] & (1UL << bit)) != 0U) ? 1U : 0U;
}

static void lcdm_table_ng_set_in(uint16_t point)
{
  lcdm_table_ng_set_bit(lcdm_table_ng_in_bits, point);
}

static void lcdm_table_ng_set_out(uint16_t point)
{
  lcdm_table_ng_set_bit(lcdm_table_ng_out_bits, point);
}

static uint8_t lcdm_table_ng_has_in(uint16_t point)
{
  return lcdm_table_ng_has_bit(lcdm_table_ng_in_bits, point);
}

static uint8_t lcdm_table_ng_has_out(uint16_t point)
{
  return lcdm_table_ng_has_bit(lcdm_table_ng_out_bits, point);
}

static void lcdm_learn_table_clear(void)
{
  uint16_t point;

  for(point = 0U; point <= LCDM_TOTAL_POINTS; point++) {
    lcdm_learn_in_bg[point] = 0U;
    lcdm_learn_out_bg[point] = 0U;
  }
  lcdm_learn_footer_scan_cache[0] = '\0';
  lcdm_learn_footer_pairs_cache[0] = '\0';
  lcdm_learn_footer_points_cache[0] = '\0';
}

static void lcdm_auto_drawn_cache_clear(void)
{
  uint8_t slot;

  for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
    lcdm_auto_drawn_line_cache[slot][0] = '\0';
  }
  lcdm_auto_drawn_page_cache = 0U;
  lcdm_auto_diag_a_cache[0] = '\0';
  lcdm_auto_diag_b_cache[0] = '\0';
  lcdm_auto_diag_visible = 0U;
}

static void lcdm_auto_drawn_cache_sync(uint8_t page)
{
  uint8_t slot;
  uint16_t point;
  uint16_t first;
  const char *line;

  if(page == 0U) {
    page = 1U;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
    point = (uint16_t)(first + slot);
    line = (point <= lcdm_auto_line_count) ? lcdm_auto_line_cache[point] : "";
    (void)snprintf(lcdm_auto_drawn_line_cache[slot],
                   sizeof(lcdm_auto_drawn_line_cache[slot]),
                   "%s",
                   line);
  }
  lcdm_auto_drawn_page_cache = page;
}

static void lcdm_auto_line_data_clear(void)
{
  uint16_t point;

  for(point = 0U; point <= LCDM_TOTAL_POINTS; point++) {
    lcdm_auto_line_cache[point][0] = '\0';
  }
  lcdm_auto_line_count = 0U;
  lcdm_auto_point_count = 0UL;
}

static void lcdm_auto_test_clear(void)
{
  lcdm_auto_line_data_clear();
  lcdm_auto_page_cache = 0U;
  lcdm_auto_drawn_cache_clear();
  lcdm_auto_active_cache = 0U;
  lcdm_auto_footer_cache[0] = '\0';
}

static uint8_t lcdm_auto_test_page_count(void)
{
  uint8_t page_count;

  if(lcdm_auto_line_count == 0U) {
    return 1U;
  }

  page_count = (uint8_t)(((lcdm_auto_line_count - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  if(page_count == 0U) {
    page_count = 1U;
  }
  if(page_count > LCDM_AUTO_LIST_MAX_PAGE) {
    page_count = LCDM_AUTO_LIST_MAX_PAGE;
  }
  return page_count;
}

static uint16_t lcdm_rgb565(uint16_t r5, uint16_t g6, uint16_t b5)
{
  return (uint16_t)(((r5 & 0x1FU) << 11) | ((g6 & 0x3FU) << 5) | (b5 & 0x1FU));
}

static uint16_t lcdm_learn_group_color(uint16_t group_index)
{
  static const uint16_t colors[] = {
    LCDM_GREEN,
    LCDM_ORANGE,
    LCDM_MAGENTA,
    LCDM_BLUE,
    LCDM_PURPLE,
    LCDM_PALE_CYAN,
    LCDM_DARK_ORANGE,
    LCDM_DARK_GREEN,
    LCDM_GRAY,
    65504U,
    2047U,
    63519U,
    64543U,
    34815U,
    65440U,
    32767U,
    63454U,
    47071U,
    65375U,
    38879U,
    63423U,
    22527U,
    65519U,
    45055U
  };
  uint16_t idx;
  uint16_t r;
  uint16_t g;
  uint16_t b;

  if(group_index == 0U) {
    group_index = 1U;
  }

  idx = (uint16_t)(group_index - 1U);
  if(idx < (sizeof(colors) / sizeof(colors[0]))) {
    return colors[idx];
  }

  r = (uint16_t)(((idx * 7U) % 25U) + 6U);
  g = (uint16_t)(((idx * 13U) % 49U) + 14U);
  b = (uint16_t)(((idx * 17U) % 25U) + 6U);

  return lcdm_rgb565(r, g, b);
}

static uint16_t lcdm_learn_text_color(uint16_t bg)
{
  uint16_t r = (uint16_t)((bg >> 11) & 0x1FU);
  uint16_t g = (uint16_t)((bg >> 5) & 0x3FU);
  uint16_t b = (uint16_t)(bg & 0x1FU);
  uint32_t luminance = ((uint32_t)r * 76UL) + ((uint32_t)g * 75UL) + ((uint32_t)b * 29UL);

  return (luminance > 3600UL) ? LCDM_NAVY : LCDM_WHITE;
}

static uint16_t lcdm_finish_bg_color(uint16_t bg)
{
  if(bg == LCDM_GREEN) {
    return LCDM_DONE_GREEN;
  }
  if(bg == LCDM_DARK_GREEN) {
    return LCDM_DONE_DARK_GREEN;
  }
  if(bg == LCDM_ORANGE) {
    return LCDM_DARK_ORANGE;
  }
  if(bg == LCDM_PALE_CYAN) {
    return LCDM_PALE_BLUE;
  }
  if(bg == LCDM_GRAY) {
    return LCDM_DARK_GRAY;
  }
  return bg;
}

static uint8_t lcdm_auto_test_point_visible(uint8_t page, uint16_t point)
{
  uint16_t first;
  uint16_t last;

  if(page == 0U) {
    page = 1U;
  }
  if(page > LCDM_AUTO_LIST_MAX_PAGE) {
    page = LCDM_AUTO_LIST_MAX_PAGE;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);
  if(last > LCDM_TOTAL_POINTS) {
    last = LCDM_TOTAL_POINTS;
  }
  return (point >= first && point <= last) ? 1U : 0U;
}

static void lcdm_auto_test_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *y)
{
  uint16_t base;
  uint16_t offset;

  if(page == 0U) {
    page = 1U;
  }
  if(page > LCDM_AUTO_LIST_MAX_PAGE) {
    page = LCDM_AUTO_LIST_MAX_PAGE;
  }

  base = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  offset = (uint16_t)(point - base);
  *x = 16U;
  *y = (uint16_t)(LCDM_AUTO_LIST_Y0 + (offset * LCDM_AUTO_LIST_ROW_H));
}

static uint16_t lcdm_auto_count_line_points(const char *line)
{
  uint16_t count = 0U;

  if(line == 0 || line[0] == '\0') {
    return 0U;
  }

  while(*line != '\0' && *line != ';') {
    if(((*line == 'I') || (*line == 'O')) &&
       (line[1] >= '0' && line[1] <= '9') &&
       (line[2] >= '0' && line[2] <= '9') &&
       (line[3] >= '0' && line[3] <= '9')) {
      count++;
    }
    line++;
  }

  return count;
}

static uint16_t lcdm_auto_line_first_in(const char *line)
{
  uint16_t value;

  if(line == 0) {
    return 0xFFFFU;
  }

  while(*line != '\0') {
    if((*line == 'I') &&
       (line[1] >= '0' && line[1] <= '9') &&
       (line[2] >= '0' && line[2] <= '9') &&
       (line[3] >= '0' && line[3] <= '9')) {
      value = (uint16_t)(((line[1] - '0') * 100U) +
                         ((line[2] - '0') * 10U) +
                         (line[3] - '0'));
      return value;
    }
    line++;
  }

  return 0xFFFFU;
}

static uint8_t lcdm_auto_line_has_token(const char *line, char token_type, uint16_t token_value)
{
  uint16_t value;

  if(line == 0) {
    return 0U;
  }

  while(*line != '\0') {
    if((*line == token_type) &&
       (line[1] >= '0' && line[1] <= '9') &&
       (line[2] >= '0' && line[2] <= '9') &&
       (line[3] >= '0' && line[3] <= '9')) {
      value = (uint16_t)(((line[1] - '0') * 100U) +
                         ((line[2] - '0') * 10U) +
                         (line[3] - '0'));
      if(value == token_value) {
        return 1U;
      }
    }
    line++;
  }

  return 0U;
}

static uint8_t lcdm_auto_lines_share_token(const char *left, const char *right)
{
  char token_type;
  uint16_t token_value;

  if(left == 0 || right == 0) {
    return 0U;
  }

  while(*left != '\0') {
    token_type = *left;
    if(((token_type == 'I') || (token_type == 'O')) &&
       (left[1] >= '0' && left[1] <= '9') &&
       (left[2] >= '0' && left[2] <= '9') &&
       (left[3] >= '0' && left[3] <= '9')) {
      token_value = (uint16_t)(((left[1] - '0') * 100U) +
                               ((left[2] - '0') * 10U) +
                               (left[3] - '0'));
      if(lcdm_auto_line_has_token(right, token_type, token_value) != 0U) {
        return 1U;
      }
    }
    left++;
  }

  return 0U;
}

static void lcdm_auto_collect_line_tokens(const char *line,
                                          uint8_t in_seen[LCDM_TOTAL_POINTS + 1U],
                                          uint8_t out_seen[LCDM_TOTAL_POINTS + 1U])
{
  char token_type;
  uint16_t token_value;

  if(line == 0) {
    return;
  }

  while(*line != '\0') {
    token_type = *line;
    if(((token_type == 'I') || (token_type == 'O')) &&
       (line[1] >= '0' && line[1] <= '9') &&
       (line[2] >= '0' && line[2] <= '9') &&
       (line[3] >= '0' && line[3] <= '9')) {
      token_value = (uint16_t)(((line[1] - '0') * 100U) +
                               ((line[2] - '0') * 10U) +
                               (line[3] - '0'));
      if(token_value <= LCDM_TOTAL_POINTS) {
        if(token_type == 'I') {
          in_seen[token_value] = 1U;
        } else {
          out_seen[token_value] = 1U;
        }
      }
    }
    line++;
  }
}

static void lcdm_auto_append_token(char *out,
                                   uint16_t out_len,
                                   char token_type,
                                   uint16_t token_value,
                                   uint8_t *first)
{
  uint16_t used;

  if(out == 0 || first == 0 || out_len == 0U) {
    return;
  }

  used = (uint16_t)strlen(out);
  if(used >= out_len || (uint16_t)(out_len - used) <= 6U) {
    return;
  }

  if(*first == 0U) {
    out[used] = ',';
    used++;
    out[used] = '\0';
  }

  (void)snprintf(&out[used], (uint16_t)(out_len - used), "%c%03u", token_type, (unsigned int)token_value);
  *first = 0U;
}

static void lcdm_auto_merge_lines(const char *existing_line, const char *new_line, char *out, uint16_t out_len)
{
  uint8_t in_seen[LCDM_TOTAL_POINTS + 1U];
  uint8_t out_seen[LCDM_TOTAL_POINTS + 1U];
  uint16_t point;
  uint8_t first;
  uint16_t used;

  if(out == 0 || out_len == 0U) {
    return;
  }

  for(point = 0U; point <= LCDM_TOTAL_POINTS; point++) {
    in_seen[point] = 0U;
    out_seen[point] = 0U;
  }

  lcdm_auto_collect_line_tokens(existing_line, in_seen, out_seen);
  lcdm_auto_collect_line_tokens(new_line, in_seen, out_seen);

  out[0] = '\0';
  first = 1U;
  for(point = 1U; point <= LCDM_TOTAL_POINTS; point++) {
    if(in_seen[point] != 0U) {
      lcdm_auto_append_token(out, out_len, 'I', point, &first);
    }
  }

  used = (uint16_t)strlen(out);
  if(used < out_len && (uint16_t)(out_len - used) > 1U) {
    out[used] = '-';
    used++;
    out[used] = '\0';
  }

  first = 1U;
  for(point = 1U; point <= LCDM_TOTAL_POINTS; point++) {
    if(out_seen[point] != 0U) {
      lcdm_auto_append_token(out, out_len, 'O', point, &first);
    }
  }

  used = (uint16_t)strlen(out);
  if(used < out_len && (uint16_t)(out_len - used) > 1U) {
    out[used] = ';';
    used++;
    out[used] = '\0';
  }
}

static uint16_t lcdm_auto_test_set_line(uint16_t point, const char *line)
{
  uint16_t existing;
  uint16_t old_points;
  uint16_t new_points;
  uint16_t insert;
  uint16_t key;
  char merged_line[256];

  if(point == 0U || point > LCDM_TOTAL_POINTS) {
    return 0U;
  }
  if(line == 0) {
    line = "";
  }
  if(line[0] == '\0') {
    return 0U;
  }

  for(existing = 1U; existing <= lcdm_auto_line_count; existing++) {
    if(strcmp(lcdm_auto_line_cache[existing], line) == 0) {
      return existing;
    }
    if(lcdm_auto_lines_share_token(lcdm_auto_line_cache[existing], line) != 0U) {
      old_points = lcdm_auto_count_line_points(lcdm_auto_line_cache[existing]);
      lcdm_auto_merge_lines(lcdm_auto_line_cache[existing], line, merged_line, sizeof(merged_line));
      new_points = lcdm_auto_count_line_points(merged_line);
      (void)snprintf(lcdm_auto_line_cache[existing],
                     sizeof(lcdm_auto_line_cache[existing]),
                     "%s",
                     merged_line);
      if(lcdm_auto_point_count >= old_points) {
        lcdm_auto_point_count -= old_points;
      } else {
        lcdm_auto_point_count = 0UL;
      }
      lcdm_auto_point_count += new_points;

      key = lcdm_auto_line_first_in(lcdm_auto_line_cache[existing]);
      while(existing > 1U && lcdm_auto_line_first_in(lcdm_auto_line_cache[existing - 1U]) > key) {
        memcpy(merged_line, lcdm_auto_line_cache[existing - 1U], sizeof(merged_line));
        memcpy(lcdm_auto_line_cache[existing - 1U],
               lcdm_auto_line_cache[existing],
               sizeof(lcdm_auto_line_cache[existing - 1U]));
        memcpy(lcdm_auto_line_cache[existing],
               merged_line,
               sizeof(lcdm_auto_line_cache[existing]));
        existing--;
      }
      return existing;
    }
  }

  if(lcdm_auto_line_count >= LCDM_TOTAL_POINTS) {
    return 0U;
  }

  key = lcdm_auto_line_first_in(line);
  insert = (uint16_t)(lcdm_auto_line_count + 1U);
  for(existing = 1U; existing <= lcdm_auto_line_count; existing++) {
    if(lcdm_auto_line_first_in(lcdm_auto_line_cache[existing]) > key) {
      insert = existing;
      break;
    }
  }

  lcdm_auto_line_count++;
  for(existing = lcdm_auto_line_count; existing > insert; existing--) {
    memcpy(lcdm_auto_line_cache[existing],
           lcdm_auto_line_cache[existing - 1U],
           sizeof(lcdm_auto_line_cache[existing]));
  }
  (void)snprintf(lcdm_auto_line_cache[insert],
                 sizeof(lcdm_auto_line_cache[insert]),
                 "%s",
                 line);
  lcdm_auto_point_count += (uint32_t)lcdm_auto_count_line_points(line);
  return insert;
}

static void lcdm_draw_auto_test_line(uint8_t page, uint16_t point, uint16_t active_point)
{
  uint16_t x;
  uint16_t y;
  uint16_t text_x;
  uint16_t text_y;
  uint16_t text_h;
  uint16_t split_w;
  uint16_t bg = LCDM_WHITE;
  uint16_t fg = LCDM_NAVY;
  uint16_t in_bg;
  uint16_t out_bg;
  char text[LCDM_AUTO_LINE_TEXT_MAX];
  char in_text[LCDM_AUTO_LINE_TEXT_MAX];
  char out_text[LCDM_AUTO_LINE_TEXT_MAX];
  char *dash;
  const char *line;

  if(lcdm_auto_test_point_visible(page, point) == 0U) {
    return;
  }

  line = lcdm_auto_line_cache[point];
  strncpy(text, line, sizeof(text) - 1U);
  text[sizeof(text) - 1U] = '\0';
  bg = ((point & 1U) == 0U) ? LCDM_ROW_BG : LCDM_WHITE;
  if(active_point != 0U && point == active_point) {
    bg = LCDM_BLUE;
    fg = LCDM_WHITE;
  }
  lcdm_auto_test_point_rect(page, point, &x, &y);
  lcdm_raw_fill(x, y, 448U, LCDM_AUTO_LIST_ROW_H, bg);
  if(text[0] == '\0') {
    return;
  }

  text_x = (uint16_t)(x + 4U);
  text_y = (uint16_t)(y + 6U);
  text_h = 16U;
  dash = strchr(text, '-');
  if(active_point == 0U && dash != 0 && dash > text) {
    *dash = '\0';
    strncpy(in_text, text, sizeof(in_text) - 2U);
    in_text[sizeof(in_text) - 2U] = '\0';
    strncat(in_text, "-", sizeof(in_text) - strlen(in_text) - 1U);
    (void)snprintf(out_text, sizeof(out_text), "%s", dash + 1);
    split_w = (uint16_t)((strlen(in_text) * LCDM_AUTO_LINE_CHAR_W) + LCDM_AUTO_LINE_PAD_W);
    if(split_w < 56U) {
      split_w = 56U;
    }
    if(split_w > (LCDM_AUTO_LINE_TOTAL_W - LCDM_AUTO_LINE_OUT_MIN_W)) {
      split_w = (uint16_t)(LCDM_AUTO_LINE_TOTAL_W - LCDM_AUTO_LINE_OUT_MIN_W);
    }
    in_bg = LCDM_GREEN;
    out_bg = LCDM_DARK_GREEN;
    lcdm_raw_xstr_full(text_x,
                       text_y,
                       split_w,
                       text_h,
                       LCDM_FONT_SMALL,
                       lcdm_learn_text_color(in_bg),
                       in_bg,
                       0U,
                       in_text);
    lcdm_raw_xstr_full((uint16_t)(text_x + split_w),
                       text_y,
                       (uint16_t)(LCDM_AUTO_LINE_TOTAL_W - split_w),
                       text_h,
                       LCDM_FONT_SMALL,
                       lcdm_learn_text_color(out_bg),
                       out_bg,
                       0U,
                       out_text);
  } else {
    lcdm_raw_xstr_full(text_x,
                       text_y,
                       LCDM_AUTO_LINE_TOTAL_W,
                       text_h,
                       LCDM_FONT_SMALL,
                       fg,
                       bg,
                       0U,
                       text);
  }
}

static void lcdm_draw_auto_test_all(uint8_t page, uint16_t active_point)
{
  uint16_t point;
  uint16_t first;
  uint16_t last;

  if(page == 0U) {
    page = 1U;
  }
  if(page > LCDM_AUTO_LIST_MAX_PAGE) {
    page = LCDM_AUTO_LIST_MAX_PAGE;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);

  if(first > lcdm_auto_line_count) {
    return;
  }
  if(last > lcdm_auto_line_count) {
    last = lcdm_auto_line_count;
  }

  for(point = first; point <= last; point++) {
    lcdm_draw_auto_test_line(page, point, active_point);
  }
}

static void lcdm_draw_auto_footer(uint8_t done)
{
  char status_text[20];
  char pairs_text[24];
  char points_text[24];
  char cache_text[80];

  if(done != 0U) {
    (void)snprintf(status_text, sizeof(status_text), "RESULT");
  } else {
    (void)snprintf(status_text, sizeof(status_text), "AUTO");
  }
  (void)snprintf(pairs_text, sizeof(pairs_text), "TOTAL-%03u PAIRS", (unsigned int)lcdm_auto_line_count);
  (void)snprintf(points_text, sizeof(points_text), "TOTAL-%03lu POINTS", (unsigned long)lcdm_auto_point_count);

  (void)snprintf(cache_text, sizeof(cache_text), "%s|%s|%s", status_text, pairs_text, points_text);
  if(strcmp(lcdm_auto_footer_cache, cache_text) == 0) {
    return;
  }

  (void)snprintf(lcdm_auto_footer_cache, sizeof(lcdm_auto_footer_cache), "%s", cache_text);
  lcdm_raw_fill(0U, 188U, LCDM_W, 24U, LCDM_WHITE);
  lcdm_raw_xstr_full(0U, 188U, 140U, 24U, LCDM_FONT_TABLE, LCDM_BLUE, LCDM_WHITE, 1U, status_text);
  lcdm_raw_xstr_full(140U, 188U, 170U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, pairs_text);
  lcdm_raw_xstr_full(310U, 188U, 170U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, points_text);
}

static void lcdm_prepare_auto_test_page(uint8_t page, uint16_t active_point, uint8_t done)
{
  if(page == 0U) {
    page = 1U;
  }
  if(page > LCDM_AUTO_LIST_MAX_PAGE) {
    page = LCDM_AUTO_LIST_MAX_PAGE;
  }

  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");

  if(done == 0U && active_point == 0U && lcdm_auto_page_cache == 0U) {
    lcdm_auto_test_clear();
  }

  if(lcdm_layout_mode != 4U || lcdm_auto_page_cache != page) {
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 4U;
    lcdm_auto_page_cache = page;
    lcdm_auto_active_cache = active_point;
    lcdm_layout_top_cache[0] = '\0';
    lcdm_draw_auto_header("AUTO TESTING");
    lcdm_draw_auto_test_all(page, active_point);
    lcdm_raw_draw_keys();
    return;
  }

  if(lcdm_auto_active_cache != active_point) {
    if(lcdm_auto_test_point_visible(page, lcdm_auto_active_cache) != 0U) {
      lcdm_draw_auto_test_line(page, lcdm_auto_active_cache, active_point);
    }
    if(lcdm_auto_test_point_visible(page, active_point) != 0U) {
      lcdm_draw_auto_test_line(page, active_point, active_point);
    }
    lcdm_auto_active_cache = active_point;
  }

  if(done != 0U) {
    lcdm_draw_auto_footer(done);
  }
}

static void lcdm_draw_table_legend(uint8_t page)
{
  uint16_t x;
  uint16_t y;

  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  lcdm_table_point_rect(page, (page == 1U) ? 48U : 95U, &x, &y);
  lcdm_raw_fill(x, y, 40U, 40U, LCDM_BLACK);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, LCDM_GREEN);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_GREEN, 1U, "IN");
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 21U), 38U, 18U, LCDM_DARK_GREEN);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 21U), 38U, 18U, LCDM_FONT_TABLE, LCDM_WHITE, LCDM_DARK_GREEN, 1U, "OUT");
}

static void lcdm_table_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *in_y)
{
  uint16_t base = (page == 1U) ? 1U : 48U;
  uint16_t offset = (uint16_t)(point - base);
  uint16_t group = (uint16_t)(offset / 12U);
  uint16_t col = (uint16_t)(offset % 12U);

  *x = (uint16_t)(col * 40U);
  *in_y = (uint16_t)(38U + (group * 42U));
}

static void lcdm_draw_table_pair(uint8_t page, uint16_t point, uint16_t active_point)
{
  uint16_t x;
  uint16_t y;
  uint16_t top_bg = LCDM_ROW_BG;
  uint16_t bottom_bg = LCDM_ROW_BG;
  uint16_t top_fg = LCDM_DARK_GRAY;
  uint16_t bottom_fg = LCDM_DARK_GRAY;
  char text[8];

  if(lcdm_table_point_visible(page, point) == 0U) {
    return;
  }

  if(point <= active_point) {
    top_bg = LCDM_DONE_GREEN;
    bottom_bg = LCDM_DONE_DARK_GREEN;
    top_fg = LCDM_NAVY;
    bottom_fg = LCDM_WHITE;
  }
  if(point == active_point) {
    top_bg = LCDM_BLUE;
    bottom_bg = LCDM_NAVY;
    top_fg = LCDM_WHITE;
    bottom_fg = LCDM_WHITE;
  }
  if(lcdm_table_ng_has_in(point) != 0U) {
    top_bg = LCDM_RED;
    top_fg = LCDM_WHITE;
  }
  if(lcdm_table_ng_has_out(point) != 0U) {
    bottom_bg = LCDM_RED;
    bottom_fg = LCDM_WHITE;
  }

  lcdm_table_point_rect(page, point, &x, &y);
  lcdm_raw_fill(x, y, 40U, 40U, LCDM_BLACK);
  (void)snprintf(text, sizeof(text), "%03u", (unsigned int)point);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, top_bg);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, LCDM_FONT_TABLE, top_fg, top_bg, 1U, text);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 21U), 38U, 18U, bottom_bg);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 21U), 38U, 18U, LCDM_FONT_TABLE, bottom_fg, bottom_bg, 1U, text);
}

static void lcdm_draw_table_all(uint8_t page, uint16_t active_point)
{
  uint16_t point;
  uint16_t first = (page == 1U) ? 1U : 48U;
  uint16_t last = (page == 1U) ? 47U : 94U;

  for(point = first; point <= last; point++) {
    lcdm_draw_table_pair(page, point, active_point);
  }
}

static void lcdm_prepare_table_page(uint8_t page, uint16_t active_point)
{
  uint16_t old_active = lcdm_table_active_cache;

  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");

  if(active_point == 0U) {
    lcdm_table_ng_clear();
  }

  if(lcdm_layout_mode != 2U || lcdm_table_page_cache != page) {
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 2U;
    lcdm_table_page_cache = page;
    lcdm_table_active_cache = active_point;
    lcdm_layout_top_cache[0] = '\0';
    lcdm_draw_common_header("");
    lcdm_draw_table_all(page, active_point);
    lcdm_draw_table_legend(page);
    return;
  }

  if(active_point > LCDM_TOTAL_POINTS) {
    lcdm_draw_table_all(page, active_point);
    lcdm_draw_table_legend(page);
    lcdm_table_active_cache = active_point;
    return;
  }

  if(active_point == 0U && old_active != 0U) {
    lcdm_draw_table_all(page, 0U);
    lcdm_draw_table_legend(page);
    lcdm_table_active_cache = 0U;
    return;
  }

  if(old_active != active_point) {
    if(lcdm_table_point_visible(page, old_active) != 0U) {
      lcdm_draw_table_pair(page, old_active, active_point);
    }
    if(lcdm_table_point_visible(page, active_point) != 0U) {
      lcdm_draw_table_pair(page, active_point, active_point);
    }
    if(active_point > old_active) {
      uint16_t point;
      for(point = (uint16_t)(old_active + 1U); point < active_point; point++) {
        if(lcdm_table_point_visible(page, point) != 0U) {
          lcdm_draw_table_pair(page, point, active_point);
        }
      }
    }
    lcdm_table_active_cache = active_point;
  }
}

static void lcdm_learn_table_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *in_y)
{
  uint16_t base = (page == 1U) ? 1U : 48U;
  uint16_t offset = (uint16_t)(point - base);
  uint16_t group = (uint16_t)(offset / 12U);
  uint16_t col = (uint16_t)(offset % 12U);

  *x = (uint16_t)(col * 40U);
  *in_y = (uint16_t)(36U + (group * 38U));
}

static void lcdm_draw_learn_footer(uint16_t scan_point, uint16_t pair_count, uint32_t point_count, uint8_t done)
{
  char scan_text[20];
  char pairs_text[24];
  char points_text[24];

  if(done == 2U) {
    lcdm_learn_outcome_blink_on ^= 1U;
    if(lcdm_learn_outcome_blink_on != 0U) {
      (void)snprintf(scan_text, sizeof(scan_text), "CONFIRMED");
    } else {
      scan_text[0] = '\0';
    }
  } else if(done != 0U) {
    (void)snprintf(scan_text, sizeof(scan_text), "OUTCOME");
  } else if(scan_point == 0U) {
    (void)snprintf(scan_text, sizeof(scan_text), "SCAN 000");
  } else {
    (void)snprintf(scan_text, sizeof(scan_text), "SCAN %03u-%03u", (unsigned int)scan_point, (unsigned int)scan_point);
  }

  if(pair_count == 0U && point_count == 0UL && done == 0U) {
    pairs_text[0] = '\0';
    points_text[0] = '\0';
  } else {
    (void)snprintf(pairs_text, sizeof(pairs_text), "TOTAL-%03u PAIRS", (unsigned int)pair_count);
    (void)snprintf(points_text, sizeof(points_text), "TOTAL-%03u POINTS", (unsigned int)point_count);
  }

  if(strncmp(lcdm_learn_footer_scan_cache, scan_text, sizeof(lcdm_learn_footer_scan_cache)) == 0 &&
     strncmp(lcdm_learn_footer_pairs_cache, pairs_text, sizeof(lcdm_learn_footer_pairs_cache)) == 0 &&
     strncmp(lcdm_learn_footer_points_cache, points_text, sizeof(lcdm_learn_footer_points_cache)) == 0) {
    return;
  }

  (void)snprintf(lcdm_learn_footer_scan_cache, sizeof(lcdm_learn_footer_scan_cache), "%s", scan_text);
  (void)snprintf(lcdm_learn_footer_pairs_cache, sizeof(lcdm_learn_footer_pairs_cache), "%s", pairs_text);
  (void)snprintf(lcdm_learn_footer_points_cache, sizeof(lcdm_learn_footer_points_cache), "%s", points_text);
  lcdm_raw_fill(0U, 188U, LCDM_W, 24U, LCDM_WHITE);
  lcdm_raw_xstr_full(0U, 188U, 140U, 24U, LCDM_FONT_TABLE, LCDM_BLUE, LCDM_WHITE, 1U, scan_text);
  lcdm_raw_xstr_full(140U, 188U, 170U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, pairs_text);
  lcdm_raw_xstr_full(310U, 188U, 170U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, points_text);
}

static void lcdm_draw_learn_table_legend(uint8_t page)
{
  uint16_t x;
  uint16_t y;

  lcdm_learn_table_point_rect(page, (page == 1U) ? 48U : 95U, &x, &y);
  lcdm_raw_fill(x, y, 40U, 36U, LCDM_BLACK);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, LCDM_ROW_BG);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_ROW_BG, 1U, "IN");
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 19U), 38U, 16U, LCDM_ROW_BG);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 19U), 38U, 16U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_ROW_BG, 1U, "OUT");
}

static void lcdm_draw_learn_table_pair(uint8_t page, uint16_t point, uint16_t active_point)
{
  uint16_t x;
  uint16_t y;
  uint16_t top_bg = LCDM_WHITE;
  uint16_t bottom_bg = LCDM_WHITE;
  uint16_t top_fg = LCDM_DARK_GRAY;
  uint16_t bottom_fg = LCDM_DARK_GRAY;
  char text[8];

  if(lcdm_table_point_visible(page, point) == 0U) {
    return;
  }

  if(point <= active_point) {
    top_bg = LCDM_ROW_BG;
    bottom_bg = LCDM_ROW_BG;
  }
  if(point == active_point) {
    top_bg = LCDM_BLUE;
    bottom_bg = LCDM_NAVY;
    top_fg = LCDM_WHITE;
    bottom_fg = LCDM_WHITE;
  }
  if(lcdm_learn_in_bg[point] != 0U) {
    top_bg = lcdm_learn_in_bg[point];
    top_fg = lcdm_learn_text_color(top_bg);
  }
  if(lcdm_learn_out_bg[point] != 0U) {
    bottom_bg = lcdm_learn_out_bg[point];
    bottom_fg = lcdm_learn_text_color(bottom_bg);
  }
  if((active_point == 0U || active_point > LCDM_TOTAL_POINTS) && lcdm_learn_in_bg[point] != 0U) {
    top_bg = lcdm_finish_bg_color(top_bg);
    top_fg = lcdm_learn_text_color(top_bg);
  }
  if((active_point == 0U || active_point > LCDM_TOTAL_POINTS) && lcdm_learn_out_bg[point] != 0U) {
    bottom_bg = lcdm_finish_bg_color(bottom_bg);
    bottom_fg = lcdm_learn_text_color(bottom_bg);
  }

  lcdm_learn_table_point_rect(page, point, &x, &y);
  lcdm_raw_fill(x, y, 40U, 36U, LCDM_BLACK);
  (void)snprintf(text, sizeof(text), "%03u", (unsigned int)point);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, top_bg);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, LCDM_FONT_TABLE, top_fg, top_bg, 1U, text);
  lcdm_raw_fill((uint16_t)(x + 1U), (uint16_t)(y + 19U), 38U, 16U, bottom_bg);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 19U), 38U, 16U, LCDM_FONT_TABLE, bottom_fg, bottom_bg, 1U, text);
}

static void lcdm_draw_learn_table_all(uint8_t page, uint16_t active_point)
{
  uint16_t point;
  uint16_t first = (page == 1U) ? 1U : 48U;
  uint16_t last = (page == 1U) ? 47U : 94U;

  for(point = first; point <= last; point++) {
    lcdm_draw_learn_table_pair(page, point, active_point);
  }
}

static void lcdm_prepare_learn_table_page(uint8_t page,
                                          uint16_t active_point,
                                          uint16_t scan_point,
                                          uint16_t pair_count,
                                          uint32_t point_count,
                                          uint8_t done)
{
  uint16_t old_active = lcdm_table_active_cache;

  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");

  if(active_point == 0U && done == 0U) {
    lcdm_learn_table_clear();
  }

  if(lcdm_layout_mode != 3U || lcdm_table_page_cache != page) {
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 3U;
    lcdm_table_page_cache = page;
    lcdm_table_active_cache = active_point;
    lcdm_layout_top_cache[0] = '\0';
    lcdm_draw_common_header("");
    lcdm_draw_learn_table_all(page, active_point);
    lcdm_draw_learn_table_legend(page);
    lcdm_learn_footer_scan_cache[0] = '\0';
    lcdm_learn_footer_pairs_cache[0] = '\0';
    lcdm_learn_footer_points_cache[0] = '\0';
    lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
    return;
  }

  if(active_point > LCDM_TOTAL_POINTS) {
    if(old_active != active_point) {
      lcdm_draw_learn_table_all(page, active_point);
      lcdm_draw_learn_table_legend(page);
    }
    lcdm_table_active_cache = active_point;
    lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
    return;
  }

  if(active_point == 0U && old_active != 0U) {
    lcdm_draw_learn_table_all(page, 0U);
    lcdm_draw_learn_table_legend(page);
    lcdm_table_active_cache = 0U;
    lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
    return;
  }

  if(old_active != active_point) {
    if(lcdm_table_point_visible(page, old_active) != 0U) {
      lcdm_draw_learn_table_pair(page, old_active, active_point);
    }
    if(lcdm_table_point_visible(page, active_point) != 0U) {
      lcdm_draw_learn_table_pair(page, active_point, active_point);
    }
    if(active_point > old_active) {
      uint16_t point;
      for(point = (uint16_t)(old_active + 1U); point < active_point; point++) {
        if(lcdm_table_point_visible(page, point) != 0U) {
          lcdm_draw_learn_table_pair(page, point, active_point);
        }
      }
    }
    lcdm_table_active_cache = active_point;
  }

  lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
}

void first_gen_display_show_page(const char *top_right,
                                 const char *status_text,
                                 const char *main_text,
                                 const char *result_text,
                                 const char *sub_text,
                                 uint16_t status_color,
                                 uint16_t result_bg,
                                 uint16_t result_fg)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  if(status_text == 0) {
    status_text = "";
  }
  if(main_text == 0) {
    main_text = "";
  }
  if(result_text == 0) {
    result_text = "";
  }
  if(sub_text == 0) {
    sub_text = "";
  }
  if(status_color == 0U) {
    status_color = LCDM_BLUE;
  }
  if(result_bg == 0U) {
    result_bg = LCDM_WHITE;
  }
  if(result_fg == 0U) {
    result_fg = LCDM_BLUE;
  }

  lcdm_prepare_standard_page(top_right);
  if(strcmp(status_text, "RESULT") == 0U) {
    lcdm_draw_top_split_labels("RESULT", "DETAILES", LCDM_BLUE, LCDM_ROW_BG);
    if(strcmp(main_text, "PASS") == 0U) {
      lcdm_draw_result_pass_body(result_text);
    } else {
      lcdm_draw_split_body(24U,
                           84U,
                           216U,
                           216U,
                           82U,
                           main_text,
                           result_text,
                           result_fg,
                           result_bg,
                           result_fg,
                           result_bg);
    }
    lcdm_raw_main_cache[0] = '\0';
  } else if((strcmp(status_text, "LEARNING OUTCOME") == 0U) || (strcmp(status_text, "CONFIRMED") == 0U)) {
    const char *title_text = status_text;

    lcdm_idle_banner_stop();
    if(strcmp(status_text, "LEARNING OUTCOME") == 0U) {
      lcdm_learn_outcome_blink_on ^= 1U;
      title_text = (lcdm_learn_outcome_blink_on != 0U) ? "LEARNING OUTCOME" : "";
    }
    (void)status_color;
    lcdm_raw_write_cached_font(lcdm_raw_state_cache,
                               sizeof(lcdm_raw_state_cache),
                               24U,
                               LCDM_STATUS_Y,
                               432U,
                               LCDM_STATUS_H,
                               LCDM_FONT_SCROLL,
                               LCDM_BLUE,
                               LCDM_ROW_BG,
                               1U,
                               title_text);
    if(strncmp(lcdm_learn_result_left_cache, main_text, sizeof(lcdm_learn_result_left_cache)) != 0 ||
       strncmp(lcdm_learn_result_right_cache, result_text, sizeof(lcdm_learn_result_right_cache)) != 0) {
      (void)snprintf(lcdm_learn_result_left_cache, sizeof(lcdm_learn_result_left_cache), "%s", main_text);
      (void)snprintf(lcdm_learn_result_right_cache, sizeof(lcdm_learn_result_right_cache), "%s", result_text);
      lcdm_draw_split_body(24U,
                           84U,
                           216U,
                           216U,
                           82U,
                           main_text,
                           result_text,
                           LCDM_NAVY,
                           LCDM_WHITE,
                           LCDM_NAVY,
                           LCDM_WHITE);
      lcdm_raw_main_cache[0] = '\0';
    }
  } else if(strcmp(status_text, "WAITING FOR PRINTING") == 0U) {
    lcdm_raw_write_status_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache),
                                 24U, LCDM_STATUS_Y, 432U, LCDM_STATUS_H,
                                 status_color, LCDM_ROW_BG, 1U, status_text);
    if(main_text[0] != '\0') {
      lcdm_draw_body_text(84U, 82U, main_text, LCDM_WHITE, LCDM_GREEN);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
    }
  } else if(strcmp(status_text, "AUTO TESTING") == 0U) {
    lcdm_idle_banner_stop();
    lcdm_raw_write_cached_font(lcdm_raw_state_cache,
                               sizeof(lcdm_raw_state_cache),
                               24U,
                               LCDM_STATUS_Y,
                               432U,
                               LCDM_STATUS_H,
                               LCDM_FONT_SCROLL,
                               LCDM_BLUE,
                               LCDM_ROW_BG,
                               1U,
                               status_text);
    if(main_text[0] != '\0') {
      lcdm_raw_write_cached_font(lcdm_raw_main_cache,
                                 sizeof(lcdm_raw_main_cache),
                                 24U,
                                 84U,
                                 432U,
                                 82U,
                                 LCDM_FONT_SCROLL,
                                 LCDM_NAVY,
                                 LCDM_WHITE,
                                 1U,
                                 main_text);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
    }
  } else if(strcmp(status_text, "RESET") == 0U) {
    lcdm_raw_write_cached_font(lcdm_raw_state_cache,
                               sizeof(lcdm_raw_state_cache),
                               24U,
                               LCDM_STATUS_Y,
                               432U,
                               LCDM_STATUS_H,
                               LCDM_FONT_SCROLL,
                               status_color,
                               LCDM_ROW_BG,
                               1U,
                               status_text);
    lcdm_draw_body_text(84U, 82U, " ", LCDM_BLUE, LCDM_WHITE);
  } else if(strcmp(status_text, "LEARNING") == 0U) {
    lcdm_idle_banner_start("LEARNING");
    if(main_text[0] != '\0') {
      lcdm_draw_learning_pair_text(main_text);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
    }
  } else if(strcmp(status_text, "WIRE TESTER") == 0U) {
    lcdm_idle_banner_start("WIRE TESTER");
    if(main_text[0] != '\0') {
      lcdm_draw_body_text(84U, 56U, main_text, result_fg, result_bg);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 56U, LCDM_WHITE);
    }
    if(result_text[0] != '\0') {
      lcdm_draw_body_text(142U, 32U, result_text, LCDM_NAVY, LCDM_WHITE);
    }
  } else {
    lcdm_raw_write_status_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache),
                                 24U, LCDM_STATUS_Y, 432U, LCDM_STATUS_H,
                                 status_color, LCDM_ROW_BG, 1U, status_text);
    if(main_text[0] != '\0') {
      lcdm_draw_body_text(84U, 56U, main_text, result_fg, result_bg);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 56U, LCDM_WHITE);
    }
    if(result_text[0] != '\0') {
      lcdm_draw_body_text(142U, 32U, result_text, LCDM_NAVY, LCDM_WHITE);
    }
  }

  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache),
                        24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U,
                        LCDM_GRAY, LCDM_WHITE, 1U, sub_text);
}

void first_gen_display_show_auto_table_page(uint8_t page, uint16_t active_point)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_prepare_table_page(page, active_point);
}

void first_gen_display_show_auto_table_completed(uint8_t page, uint16_t point)
{
  if(display_is_lcdm == 0U) {
    return;
  }
  if(lcdm_layout_mode != 2U || lcdm_table_point_visible(page, point) == 0U) {
    return;
  }

  lcdm_draw_table_pair(page, point, (uint16_t)(LCDM_TOTAL_POINTS + 1U));
  if(lcdm_table_active_cache == point) {
    lcdm_table_active_cache = (uint16_t)(LCDM_TOTAL_POINTS + 1U);
  }
}

void first_gen_display_show_auto_table_ng(uint8_t page, uint16_t point)
{
  if(display_is_lcdm == 0U) {
    return;
  }
  if(point == 0U) {
    return;
  }

  lcdm_table_ng_set_in(point);
  lcdm_table_ng_set_out(point);
  lcdm_prepare_table_page(page, point);
  if(lcdm_table_point_visible(page, point) != 0U) {
    lcdm_draw_table_pair(page, point, point);
  }
}

void first_gen_display_show_auto_table_ng_pair(uint8_t page, uint16_t out_point, uint16_t in_point)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_table_ng_set_out(out_point);
  lcdm_table_ng_set_in(in_point);
  lcdm_prepare_table_page(page, in_point);
  if(lcdm_table_point_visible(page, in_point) != 0U) {
    lcdm_draw_table_pair(page, in_point, in_point);
  }
  if(out_point != in_point && lcdm_table_point_visible(page, out_point) != 0U) {
    lcdm_draw_table_pair(page, out_point, in_point);
  }
}

void first_gen_display_show_learn_table_page(uint8_t page,
                                             uint16_t active_point,
                                             uint16_t scan_point,
                                             uint16_t pair_count,
                                             uint32_t point_count,
                                             uint8_t done)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_prepare_learn_table_page(page, active_point, scan_point, pair_count, point_count, done);
}

void first_gen_display_show_learn_table_completed(uint8_t page,
                                                  uint16_t point,
                                                  uint16_t scan_point,
                                                  uint16_t pair_count,
                                                  uint32_t point_count)
{
  if(display_is_lcdm == 0U) {
    return;
  }
  if(lcdm_layout_mode != 3U || lcdm_table_point_visible(page, point) == 0U) {
    return;
  }

  lcdm_draw_learn_table_pair(page, point, (uint16_t)(LCDM_TOTAL_POINTS + 1U));
  if(lcdm_table_active_cache == point) {
    lcdm_table_active_cache = (uint16_t)(LCDM_TOTAL_POINTS + 1U);
  }
  (void)scan_point;
  (void)pair_count;
  (void)point_count;
}

void first_gen_display_refresh_learn_table_footer(uint16_t scan_point,
                                                  uint16_t pair_count,
                                                  uint32_t point_count,
                                                  uint8_t done)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
}

void first_gen_display_clear_learn_table_groups(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_learn_table_clear();
  lcdm_layout_mode = 0U;
  lcdm_table_page_cache = 0U;
  lcdm_table_active_cache = 0U;
}

void first_gen_display_apply_learn_table_groups(const uint16_t out_groups[],
                                                const uint16_t in_groups[],
                                                uint16_t active_point)
{
  uint16_t point;
  uint16_t new_in_bg;
  uint16_t new_out_bg;
  uint8_t changed;

  if(display_is_lcdm == 0U || out_groups == 0 || in_groups == 0) {
    return;
  }

  for(point = 1U; point <= LCDM_TOTAL_POINTS; point++) {
    new_in_bg = (in_groups[point] == 0U) ? 0U : lcdm_learn_group_color(in_groups[point]);
    new_out_bg = (out_groups[point] == 0U) ? 0U : lcdm_learn_group_color(out_groups[point]);
    changed = 0U;

    if(lcdm_learn_in_bg[point] != new_in_bg) {
      lcdm_learn_in_bg[point] = new_in_bg;
      changed = 1U;
    }
    if(lcdm_learn_out_bg[point] != new_out_bg) {
      lcdm_learn_out_bg[point] = new_out_bg;
      changed = 1U;
    }

    if(changed != 0U &&
       lcdm_layout_mode == 3U &&
       lcdm_table_point_visible(lcdm_table_page_cache, point) != 0U) {
      lcdm_draw_learn_table_pair(lcdm_table_page_cache, point, active_point);
    }
  }
}

void first_gen_display_clear_auto_test_lines(void)
{
  uint8_t keep_auto_layout;
  uint8_t current_page;

  if(display_is_lcdm == 0U) {
    return;
  }

  keep_auto_layout = (lcdm_layout_mode == 4U) ? 1U : 0U;
  current_page = lcdm_auto_page_cache;
  lcdm_auto_test_clear();
  if(keep_auto_layout != 0U) {
    lcdm_auto_page_cache = current_page;
    lcdm_raw_fill(0U,
                  LCDM_AUTO_LIST_Y0,
                  LCDM_W,
                  LCDM_AUTO_LIST_H,
                  LCDM_WHITE);
  } else {
    lcdm_layout_mode = 0U;
    lcdm_auto_page_cache = 0U;
  }
  lcdm_auto_active_cache = 0U;
}

void first_gen_display_reset_auto_test_line_cache(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_auto_line_data_clear();
}

void first_gen_display_cache_auto_test_line(uint16_t line_index, const char *line)
{
  if(display_is_lcdm == 0U) {
    return;
  }
  if(line_index == 0U || line_index > LCDM_TOTAL_POINTS || line == 0 || line[0] == '\0') {
    return;
  }

  (void)snprintf(lcdm_auto_line_cache[line_index],
                 sizeof(lcdm_auto_line_cache[line_index]),
                 "%s",
                 line);
  if(line_index > lcdm_auto_line_count) {
    lcdm_auto_line_count = line_index;
  }
  lcdm_auto_point_count += (uint32_t)lcdm_auto_count_line_points(line);
}

void first_gen_display_show_auto_test_line(uint16_t out_point, const char *line, uint8_t done)
{
  uint8_t page;
  uint16_t active_point;
  uint16_t stored_point;

  if(display_is_lcdm == 0U) {
    return;
  }

  if((line != 0) && (line[0] != '\0') && out_point != 0U && out_point <= LCDM_TOTAL_POINTS) {
    stored_point = lcdm_auto_test_set_line(out_point, line);
    if(stored_point != 0U) {
      page = (uint8_t)(((stored_point - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
      lcdm_prepare_auto_test_page(page, stored_point, done);
      lcdm_draw_auto_test_all(page, stored_point);
      lcdm_auto_active_cache = stored_point;
    }
  } else {
    if(lcdm_auto_line_count == 0U) {
      page = (uint8_t)(((out_point - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
    } else {
      page = (uint8_t)(((lcdm_auto_line_count - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
    }
    active_point = (lcdm_auto_active_cache != 0U) ? lcdm_auto_active_cache : 0U;
    lcdm_prepare_auto_test_page(page, active_point, done);
  }

  if(done != 0U) {
    lcdm_draw_auto_footer(done);
    return;
  }
}

uint8_t first_gen_display_auto_test_page_count(void)
{
  if(display_is_lcdm == 0U) {
    return 1U;
  }

  return lcdm_auto_test_page_count();
}

uint8_t first_gen_display_auto_test_page_for_line(uint16_t line_index)
{
  uint8_t page;
  uint8_t page_count;

  if(display_is_lcdm == 0U || line_index == 0U) {
    return 1U;
  }

  page = (uint8_t)(((line_index - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  page_count = lcdm_auto_test_page_count();
  if(page_count == 0U) {
    page_count = 1U;
  }
  if(page > page_count) {
    page = page_count;
  }

  return page;
}

uint16_t first_gen_display_auto_test_line_count(void)
{
  if(display_is_lcdm == 0U) {
    return 0U;
  }

  return lcdm_auto_line_count;
}

void first_gen_display_refresh_auto_test_page(uint8_t page, uint8_t done)
{
  uint8_t page_count;
  uint8_t slot;
  uint8_t full_page_refresh;
  uint16_t point;
  uint16_t first;
  const char *line;

  if(display_is_lcdm == 0U) {
    return;
  }

  page_count = lcdm_auto_test_page_count();
  if(page == 0U) {
    page = 1U;
  }
  if(page > page_count) {
    page = page_count;
  }

  full_page_refresh = (lcdm_auto_drawn_page_cache != page) ? 1U : 0U;
  lcdm_auto_page_cache = page;
  lcdm_auto_active_cache = 0U;
  if(full_page_refresh != 0U) {
    lcdm_raw_fill(0U, LCDM_AUTO_LIST_Y0, LCDM_W, LCDM_AUTO_LIST_H, LCDM_WHITE);
    for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
      lcdm_auto_drawn_line_cache[slot][0] = '\0';
    }
    lcdm_auto_drawn_page_cache = page;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
    point = (uint16_t)(first + slot);
    line = (point <= lcdm_auto_line_count) ? lcdm_auto_line_cache[point] : "";
    if(full_page_refresh != 0U || strcmp(lcdm_auto_drawn_line_cache[slot], line) != 0) {
      lcdm_draw_auto_test_line(page, point, 0U);
      (void)snprintf(lcdm_auto_drawn_line_cache[slot],
                     sizeof(lcdm_auto_drawn_line_cache[slot]),
                     "%s",
                     line);
    }
  }
  lcdm_draw_auto_footer(done);
}

void first_gen_display_refresh_auto_test_page_body(uint8_t page)
{
  uint8_t page_count;
  uint8_t slot;
  uint8_t full_page_refresh;
  uint16_t point;
  uint16_t first;
  const char *line;

  if(display_is_lcdm == 0U) {
    return;
  }

  page_count = lcdm_auto_test_page_count();
  if(page == 0U) {
    page = 1U;
  }
  if(page > page_count) {
    page = page_count;
  }

  full_page_refresh = (lcdm_auto_drawn_page_cache != page) ? 1U : 0U;
  lcdm_auto_page_cache = page;
  lcdm_auto_active_cache = 0U;
  if(full_page_refresh != 0U) {
    lcdm_raw_fill(0U, LCDM_AUTO_LIST_Y0, LCDM_W, LCDM_AUTO_LIST_H, LCDM_WHITE);
    for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
      lcdm_auto_drawn_line_cache[slot][0] = '\0';
    }
    lcdm_auto_drawn_page_cache = page;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  for(slot = 0U; slot < LCDM_AUTO_LIST_PAGE_SIZE; slot++) {
    point = (uint16_t)(first + slot);
    line = (point <= lcdm_auto_line_count) ? lcdm_auto_line_cache[point] : "";
    if(full_page_refresh != 0U || strcmp(lcdm_auto_drawn_line_cache[slot], line) != 0) {
      lcdm_draw_auto_test_line(page, point, 0U);
      (void)snprintf(lcdm_auto_drawn_line_cache[slot],
                     sizeof(lcdm_auto_drawn_line_cache[slot]),
                     "%s",
                     line);
    }
  }
  lcdm_draw_auto_footer(1U);
}

void first_gen_display_refresh_auto_test_footer(uint8_t done)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_draw_auto_footer(done);
}

void first_gen_display_show_auto_test_diag(uint32_t scan_count, uint32_t change_count)
{
  char line_a[24];
  char line_b[24];
  uint8_t force;

  if(display_is_lcdm == 0U) {
    return;
  }

  (void)snprintf(line_a, sizeof(line_a), "A%lu", (unsigned long)scan_count);
  (void)snprintf(line_b, sizeof(line_b), "B%lu", (unsigned long)change_count);

  force = (lcdm_auto_diag_visible == 0U) ? 1U : 0U;
  if(force != 0U) {
    lcdm_raw_fill(0U, LCDM_AUTO_LIST_Y0, LCDM_W, LCDM_AUTO_LIST_H, LCDM_WHITE);
    lcdm_auto_diag_visible = 1U;
  }

  if(force != 0U || strcmp(lcdm_auto_diag_a_cache, line_a) != 0) {
    (void)snprintf(lcdm_auto_diag_a_cache, sizeof(lcdm_auto_diag_a_cache), "%s", line_a);
    lcdm_raw_fill(24U, (uint16_t)(LCDM_AUTO_LIST_Y0 + 8U), 432U, 42U, LCDM_WHITE);
    lcdm_raw_xstr_full(24U,
                       (uint16_t)(LCDM_AUTO_LIST_Y0 + 8U),
                       432U,
                       42U,
                       LCDM_FONT_SCROLL,
                       LCDM_BLUE,
                       LCDM_WHITE,
                       1U,
                       line_a);
  }

  if(force != 0U || strcmp(lcdm_auto_diag_b_cache, line_b) != 0) {
    (void)snprintf(lcdm_auto_diag_b_cache, sizeof(lcdm_auto_diag_b_cache), "%s", line_b);
    lcdm_raw_fill(24U, (uint16_t)(LCDM_AUTO_LIST_Y0 + 58U), 432U, 42U, LCDM_WHITE);
    lcdm_raw_xstr_full(24U,
                       (uint16_t)(LCDM_AUTO_LIST_Y0 + 58U),
                       432U,
                       42U,
                       LCDM_FONT_SCROLL,
                       LCDM_NAVY,
                       LCDM_WHITE,
                       1U,
                       line_b);
  }
}

void first_gen_display_show_auto_test_page(uint8_t page, uint8_t done)
{
  uint8_t page_count;

  if(display_is_lcdm == 0U) {
    return;
  }

  page_count = lcdm_auto_test_page_count();
  if(page == 0U) {
    page = 1U;
  }
  if(page > page_count) {
    page = page_count;
  }

  lcdm_prepare_auto_test_page(page, 0U, done);
  lcdm_raw_fill(0U, LCDM_AUTO_LIST_Y0, LCDM_W, LCDM_AUTO_LIST_H, LCDM_WHITE);
  lcdm_draw_auto_test_all(page, 0U);
  lcdm_auto_drawn_cache_sync(page);
  lcdm_draw_auto_footer(done);
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
  const char *state_text = "READY";
  const char *main_text = "WIRE TESTER";
  const char *result_text = "";
  const char *sub_text = "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK";
  uint16_t status_color = LCDM_NAVY;
  uint8_t idle_banner = 0U;
  uint8_t pass_print_status = 0U;
  uint16_t left = 0U;
  uint16_t right = 0U;
  char result_buf[32];
  char sub_buf[64];

  if(state == 0) {
    state = "";
  }
  if(value == 0) {
    value = "";
  }

  if(lcdm_layout_mode == 4U) {
    return;
  }

  if(lcdm_parse_digit_pair(value, &left, &right) != 0U) {
    lcdm_format_pair_text(left, right, result_buf);
    result_text = result_buf;
    lcdm_format_page_text(left, sub_buf);
    sub_text = sub_buf;
  }

  if(strncmp(state, "PASS", 4U) == 0) {
    state_text = "RESULT";
    main_text = "TEST COMPLETE";
    result_text = "PASS";
    pass_print_status = 1U;
    sub_text = "PRINT READY  REMOVE HARNESS";
    status_color = LCDM_GREEN;
  } else if(strncmp(state, "NG", 2U) == 0) {
    state_text = "RESULT";
    main_text = "TEST COMPLETE";
    if(lcdm_is_digit_pair(value) != 0U) {
      lcdm_format_pair_text(left, right, result_buf);
      result_text = result_buf;
    } else {
      result_text = value;
    }
    sub_text = "NG  K3 RESET";
    status_color = LCDM_RED;
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
  } else if(strncmp(state, "LEARN", 5U) == 0) {
    state_text = "LEARNING";
    main_text = "001 - 001";
    result_text = "";
    sub_text = "";
  } else if(strncmp(state, "SELF", 4U) == 0) {
    state_text = "SELF";
    main_text = "SELF TESTING";
    result_text = "001 - 001";
    sub_text = "PAIR 001/047";
  } else if((strncmp(state, "AUTO", 4U) == 0) || (strncmp(state, "TEST", 4U) == 0)) {
    if(lcdm_is_digit_pair(value) != 0U) {
      state_text = "AUTO";
      main_text = "AUTO TESTING";
      sub_text = "";
    } else if(strncmp(value, "WIRE", 4U) == 0 || strncmp(value, "AUTO", 4U) == 0 || value[0] == '\0') {
      state_text = "READY";
      main_text = "WIRE TESTER";
      result_text = "";
      idle_banner = 1U;
    } else {
      state_text = "AUTO";
      main_text = "AUTO TESTING";
      result_text = value;
      sub_text = "RUNNING  K3 RESET";
    }
  } else {
    if(lcdm_is_digit_pair(value) != 0U) {
      state_text = "AUTO";
      main_text = "AUTO TESTING";
      (void)snprintf(sub_buf, sizeof(sub_buf), "PAGE %u/2  TOTAL 047 PAIRS/094 POINTS",
                     (unsigned int)(((left == 0U ? 1U : left) - 1U) / LCDM_PAIR_PAGE_SIZE + 1U));
      sub_text = sub_buf;
    }
  }

  if((strncmp(state, "NG", 2U) == 0) || (strncmp(state, "Er", 2U) == 0)) {
    status_color = LCDM_RED;
  } else if((strncmp(state, "PASS", 4U) == 0) || (strncmp(state, "PRINT", 5U) == 0)) {
    status_color = LCDM_GREEN;
  } else if((strncmp(state, "LEARN", 5U) == 0) || (strncmp(state, "SELF", 4U) == 0) || (strncmp(state, "AUTO", 4U) == 0) || (strncmp(state, "TEST", 4U) == 0)) {
    status_color = LCDM_BLUE;
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
      lcdm_idle_banner_start("WIRE TESTER");
    } else {
      lcdm_idle_banner_stop();
      if(result_text[0] == '\0') {
        lcdm_raw_fill(LCDM_RESULT_X, LCDM_DETAIL_Y, LCDM_RESULT_W, LCDM_DETAIL_H, LCDM_WHITE);
      } else {
        lcdm_raw_write_cached(lcdm_raw_result_cache, sizeof(lcdm_raw_result_cache), 24U, (uint16_t)(LCDM_DETAIL_Y + 18U), 432U, 34U, LCDM_BLUE, LCDM_WHITE, 0U, result_text);
      }
    }
  }
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U, LCDM_GRAY, LCDM_WHITE, 1U, sub_text);
#endif
}

static void lcdm_raw_update_learn_summary(uint16_t out_count, uint16_t in_count, uint32_t total)
{
  char pairs_text[32];
  char points_text[32];

  if(out_count > 999U) {
    out_count = 999U;
  }
  if(in_count > 999U) {
    in_count = 999U;
  }
  (void)in_count;

  (void)snprintf(pairs_text,
                 sizeof(pairs_text),
                 "TOTAL - %03u PAIRS",
                 (unsigned int)out_count);
  (void)snprintf(points_text,
                 sizeof(points_text),
                 "TOTAL - %u POINTS",
                 (unsigned int)total);

  first_gen_display_show_page("",
                              "LEARNING OUTCOME",
                              pairs_text,
                              points_text,
                              "",
                              LCDM_BLUE,
                              LCDM_WHITE,
                              LCDM_BLUE);
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

#if LCDM_TOUCH_DEBUG_TEXT
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
#endif

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
  lcdm_layout_mode = 0U;
  lcdm_table_page_cache = 0U;
  lcdm_table_active_cache = 0U;
  lcdm_table_ng_clear();
  lcdm_learn_table_clear();
  lcdm_layout_top_cache[0] = '\0';
  lcdm_learn_outcome_blink_on = 0U;
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
  first_gen_display_show_page("",
                              "WIRE TESTER",
                              "",
                              "",
                              "",
                              LCDM_BLUE,
                              LCDM_WHITE,
                              LCDM_BLUE);
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
    first_gen_display_show_page("",
                                "",
                                "",
                                "",
                                "",
                                LCDM_BLUE,
                                LCDM_WHITE,
                                LCDM_BLUE);
  } else {
    tm1637_display_clear();
  }
}

uint8_t first_gen_display_is_lcdm(void)
{
  return display_is_lcdm;
}

uint8_t first_gen_display_key_read_raw(void)
{
  if(display_is_lcdm != 0U) {
    return lcdm_display_key_read_raw();
  }
  return tm1637_key_read_raw();
}

void first_gen_display_effect_step(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_recover_if_requested();
  lcdm_idle_banner_step_now();
  lcdm_pass_blink_service();
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
  } else if(strncmp(value, "LEARN", 5U) == 0) {
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
