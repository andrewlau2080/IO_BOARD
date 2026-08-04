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
#define LCDM_TOUCH_KEYS_ONLY         1U
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
/*
 * 260728song.tft stores its font resources in this TJC ID order:
 *   0=font-4song (32 px), 1=font-3song (29 px), 2=font-2song (16 px),
 *   3=font-1song (14 px), 4=font-0song (10 px).
 * The resource-name suffix is not the TJC font ID.
 * See docs/tjc4827t143_font_plan.md.
 */
#define LCDM_FONT_SMALL             4U
#define LCDM_FONT_TABLE             4U
#define LCDM_FONT_STATUS            3U
#define LCDM_FONT_TITLE             2U
#define LCDM_FONT_RESULT            1U
#define LCDM_FONT_POINT             0U
#define LCDM_FONT_COUNT             5U
#define LCDM_BLACK                  0U
#define LCDM_BLUE                   31U
#define LCDM_RED                    63488U
#define LCDM_GREEN                  FIRST_GEN_DISPLAY_COLOR_GREEN
#define LCDM_DARK_GREEN             FIRST_GEN_DISPLAY_COLOR_AUTO_PASS_GREEN
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
#define LCDM_AUTO_LIST_PAGE_SIZE    FIRST_GEN_DISPLAY_AUTO_RESULT_PAGE_ROWS
#define LCDM_AUTO_LIST_ROW_H        29U
#define LCDM_AUTO_LIST_Y0           38U
#define LCDM_AUTO_LIST_H            (LCDM_AUTO_LIST_PAGE_SIZE * LCDM_AUTO_LIST_ROW_H)
#define LCDM_AUTO_LIST_TEXT_X       8U
#define LCDM_AUTO_LIST_TEXT_Y       5U
#define LCDM_AUTO_LIST_TEXT_H       18U
/* Compress the AUTO RESULT/DETAILS caption directly under STANDARD CABLE so
 * the middle panel has enough vertical room for three complete PASS totals.
 * PASS remains 2/5 : 3/5; NG uses the complete middle width for its I/O
 * connection group. */
#define LCDM_AUTO_SUMMARY_X          24U
#define LCDM_AUTO_SUMMARY_W          432U
#define LCDM_AUTO_SUMMARY_LABEL_Y    33U
#define LCDM_AUTO_SUMMARY_LABEL_H    20U
#define LCDM_AUTO_SUMMARY_Y          57U
#define LCDM_AUTO_SUMMARY_H          (LCDM_KEY_Y0 - LCDM_AUTO_SUMMARY_Y)
#define LCDM_AUTO_SUMMARY_LEFT_W     (((LCDM_AUTO_SUMMARY_W * 2U) + 2U) / 5U)
#define LCDM_AUTO_SUMMARY_RIGHT_X    (LCDM_AUTO_SUMMARY_X + LCDM_AUTO_SUMMARY_LEFT_W)
#define LCDM_AUTO_SUMMARY_RIGHT_W    (LCDM_AUTO_SUMMARY_W - LCDM_AUTO_SUMMARY_LEFT_W)
#define LCDM_AUTO_SUMMARY_DIVIDER_X  (LCDM_AUTO_SUMMARY_RIGHT_X - 1U)
#define LCDM_AUTO_SUMMARY_TEXT_X     (LCDM_AUTO_SUMMARY_RIGHT_X + 4U)
#define LCDM_AUTO_SUMMARY_TEXT_W     (LCDM_AUTO_SUMMARY_RIGHT_W - 8U)
#define LCDM_AUTO_SUMMARY_FULL_TEXT_X (LCDM_AUTO_SUMMARY_X + 4U)
#define LCDM_AUTO_SUMMARY_FULL_TEXT_W (LCDM_AUTO_SUMMARY_W - 8U)
#define LCDM_AUTO_SUMMARY_NG_MAX_LINES       10U
#define LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX   64U
/* The upper-right corner is outside the centred STANDARD CABLE title.  Keep
 * it as two compact cells so WIFI can communicate the production network
 * state without disturbing the title or the K1-K4 strip.  Font ID 4 is the
 * smallest Song resource in the loaded TFT and is shared by both labels. */
#define LCDM_WIFI_X                394U
#define LCDM_WIFI_Y                  1U
#define LCDM_WIFI_W                80U
#define LCDM_WIFI_H                13U
#define LCDM_HALL_X                394U
#define LCDM_HALL_Y                 17U
#define LCDM_HALL_W                80U
#define LCDM_HALL_H                13U
#define LCDM_PRINT_STATUS_Y        32U
#define LCDM_PRINT_STATUS_H        26U
#define LCDM_PRINT_BODY_Y          (LCDM_PRINT_STATUS_Y + LCDM_PRINT_STATUS_H)
#define LCDM_PRINT_BODY_H          (LCDM_KEY_Y0 - LCDM_PRINT_BODY_Y)
/* font-0song used by the AUTO list is a 10 px cell font.  Keep the layout
 * geometry on that real cell width so an I/O background ends with its text
 * instead of leaving a large coloured blank area after the last endpoint. */
#define LCDM_AUTO_CHAR_W_ESTIMATE   10U
/* Keep complete electrical circuits in RAM, then flow endpoint tokens across
 * physical result rows.  94 I points plus 94 O points need 940 bytes in the
 * worst case (including commas and the compact '-'). */
#define LCDM_AUTO_LINE_TEXT_MAX     960U
/* A 432 px text span / 10 px cell fits 43 characters.  Endpoint tokens are
 * never split: when Ixxx or Oxxx no longer fits, it starts the next physical
 * result row and pushes all following content down. */
#define LCDM_AUTO_TEXT_W            (LCDM_RESULT_W - (2U * LCDM_AUTO_LIST_TEXT_X))
#define LCDM_AUTO_ROW_CHAR_LIMIT    (LCDM_AUTO_TEXT_W / LCDM_AUTO_CHAR_W_ESTIMATE)
#define LCDM_AUTO_ROW_TEXT_MAX      (LCDM_AUTO_ROW_CHAR_LIMIT + 1U)
#define LCDM_XSTR_CMD_MAX           (LCDM_AUTO_LINE_TEXT_MAX + 96U)
#define LCDM_AUTO_LIST_MAX_PAGE     (((LCDM_TOTAL_POINTS - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U)
#define LCDM_TABLE_NG_WORDS         3U
#define LCDM_PASS_BLINK_READS       50U
#define LCDM_AUTO_TEST_BLINK_STEPS  6U
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
static char lcdm_auto_line_cache[LCDM_TOTAL_POINTS + 1U][LCDM_AUTO_LINE_TEXT_MAX];
static char lcdm_auto_footer_cache[80];
static char lcdm_layout_top_cache[32];
static char lcdm_learn_footer_scan_cache[20];
static char lcdm_learn_footer_pairs_cache[24];
static char lcdm_learn_footer_points_cache[24];
static uint8_t lcdm_auto_page_cache;
/* What is physically on the five AUTO result rows.  During scanning this
 * lets us redraw only rows whose text has really changed, rather than sending
 * a complete page (and needlessly disturbing the rest of the display). */
static uint8_t lcdm_auto_drawn_page;
static char lcdm_auto_drawn_row_cache[LCDM_AUTO_LIST_PAGE_SIZE][LCDM_AUTO_ROW_TEXT_MAX];
static uint16_t lcdm_auto_line_count;
static uint16_t lcdm_auto_input_count;
static uint32_t lcdm_auto_point_count;
/* A result row can deliberately retain a learned open endpoint so the user
 * can locate it.  These are the real measured counts used by the footer. */
static uint16_t lcdm_auto_actual_input_count;
static uint16_t lcdm_auto_actual_output_count;
static uint8_t lcdm_auto_actual_counts_valid;
static uint8_t lcdm_learn_outcome_blink_on;
static uint8_t lcdm_k1_page_hint;
static uint8_t lcdm_auto_test_blink_active;
static uint8_t lcdm_auto_test_blink_on;
static uint8_t lcdm_auto_test_blink_steps;
/* A live AUTO fault is stored separately from the CSV-style result lines.
 * Both bitmaps contain the entire electrically associated I/O group, so a
 * cross-short highlights every endpoint involved rather than only its first
 * missing learned pair. */
static uint32_t lcdm_auto_fault_out_bits[FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS];
static uint32_t lcdm_auto_fault_in_bits[FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS];
static uint8_t lcdm_auto_fault_type;
static uint8_t lcdm_auto_fault_blink_on;
/* PB8 is sampled by the scan service; this cache means an unchanged Hall
 * level sends no TJC traffic while matrix scanning is active. */
static uint8_t lcdm_hall_input_active;
static uint8_t lcdm_hall_input_drawn;
/* The WiFi indicator follows the same local-refresh rule as HALL IN. */
static uint8_t lcdm_wifi_connected;
static uint8_t lcdm_wifi_indicator_drawn;

typedef struct {
  char text[LCDM_AUTO_ROW_TEXT_MAX];
  uint8_t in_len;
  uint8_t separator_len;
  uint8_t out_len;
} lcdm_auto_visual_row_t;

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
static void lcdm_prepare_auto_test_page(uint8_t page);
static void lcdm_prepare_auto_summary_page(void);
static void lcdm_table_ng_clear(void);
static void lcdm_learn_table_clear(void);
static void lcdm_auto_test_clear(void);
static void lcdm_auto_test_clear_result_lines(void);
static void lcdm_draw_hall_indicator(uint8_t force);
static void lcdm_draw_wifi_indicator(uint8_t force);
static void lcdm_draw_print_progress_body(uint8_t state);
static void lcdm_auto_drawn_rows_invalidate(void);
static uint8_t lcdm_auto_test_point_visible(uint8_t page, uint16_t point);
static void lcdm_auto_test_point_rect(uint8_t page, uint16_t point, uint16_t *x, uint16_t *y);
static uint16_t lcdm_auto_visual_row_count(void);
static uint8_t lcdm_auto_get_visual_row(uint16_t visual_row, lcdm_auto_visual_row_t *row);
static uint8_t lcdm_auto_row_has_endpoint(const lcdm_auto_visual_row_t *row,
                                          char endpoint,
                                          uint16_t point);
static uint8_t lcdm_auto_fault_has_endpoint(char endpoint, uint16_t point);
static uint8_t lcdm_auto_row_has_fault_endpoint(const lcdm_auto_visual_row_t *row,
                                                char endpoint);
static uint8_t lcdm_auto_fault_has_any_endpoint(void);
static void lcdm_draw_auto_test_line(uint8_t page, uint16_t point);
static void lcdm_draw_auto_test_all(uint8_t page);
static void lcdm_draw_auto_test_changed(uint8_t page);
static void lcdm_draw_auto_test_fault_rows(uint8_t page);
static uint16_t lcdm_auto_test_set_line(uint16_t point, const char *line);
static uint8_t lcdm_auto_test_page_count(void);
static void lcdm_draw_auto_footer(uint8_t page, uint8_t done);
static void lcdm_draw_auto_result_pass_body(const char *detail_text);
static void lcdm_draw_auto_result_ng_detail(const char *detail_text);
static void lcdm_draw_auto_result_ng_body(const char *detail_text);
static void lcdm_draw_result_ng_detail(const char *detail_text);
static void lcdm_draw_result_ng_body(const char *detail_text);
static void lcdm_draw_auto_summary_top_labels(const char *left_text, const char *right_text);

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

/* The NG blink is intentionally a best-effort animation.  It must never
 * hold up the 94 x 94 electrical monitor while waiting for a LCDM command
 * acknowledgement, so it uses the non-ACK transport path below. */
static void lcdm_raw_fill_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
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
  lcdm_tjc_send_cmd_fast(cmd);
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
  /* Auto-result rows retain the complete Ixxx - Oxxx,... record for later
   * CSV export.  Its text can be substantially longer than ordinary labels. */
  char cmd[LCDM_XSTR_CMD_MAX];

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

static void lcdm_raw_xstr_full_fast(uint16_t x,
                                    uint16_t y,
                                    uint16_t w,
                                    uint16_t h,
                                    uint16_t font,
                                    uint16_t fg,
                                    uint16_t bg,
                                    uint8_t align,
                                    const char *text)
{
  char cmd[LCDM_XSTR_CMD_MAX];

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
                 1U,
                 text);
  lcdm_tjc_send_cmd_fast(cmd);
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
  lcdm_raw_xstr(x, (uint16_t)(y + 5U), w, (h > 10U) ? (uint16_t)(h - 10U) : h, LCDM_FONT_STATUS, LCDM_BLUE, bg, align, cache);
}

static uint16_t lcdm_key_background(uint8_t index, uint8_t active)
{
  if(active != 0U) {
    if(index == 0U) {
      return LCDM_NAVY;
    }
    if(index == 1U) {
      return LCDM_DEEP_PINK;
    }
    if(index == 2U) {
      return LCDM_PURPLE;
    }
    return LCDM_DARK_ORANGE;
  }

  if(index == 1U) {
    return LCDM_SOFT_PINK;
  }
  if(index == 2U) {
    return LCDM_MAGENTA;
  }
  if(index == 3U) {
    return LCDM_ORANGE;
  }
  return LCDM_BLUE;
}

static void lcdm_key_labels(uint8_t index,
                            const char **top,
                            const char **bottom,
                            uint16_t *bottom_fg)
{
  *top = "K1";
  *bottom = "SELF/LEARN";
  *bottom_fg = LCDM_WHITE;

  if(index == 0U && lcdm_k1_page_hint != 0U) {
    *bottom = "PAGE";
  } else if(index == 1U) {
    *top = "K2";
    *bottom = "AUTO TEST";
    /* During AUTO scanning, change only the caption colour.  The button face
     * stays intact, avoiding the white gaps from repeated full redraws. */
    if(lcdm_auto_test_blink_active != 0U && lcdm_auto_test_blink_on == 0U) {
      *bottom_fg = LCDM_NAVY;
    }
  } else if(index == 2U) {
    *top = "K3";
    *bottom = "RESET";
  } else if(index == 3U) {
    *top = "K4";
    *bottom = "OK/SAVE";
  }
}

static void lcdm_raw_draw_key_face(uint8_t index, uint8_t active)
{
  uint16_t x = (uint16_t)(index * LCDM_KEY_W);
  uint16_t bg = lcdm_key_background(index, active);
  uint16_t bottom_fg;
  const char *top;
  const char *bottom;

  lcdm_key_labels(index, &top, &bottom, &bottom_fg);
  if(active != 0U) {
    /* A single inset fill gives the pressed, darker face immediately.  The
     * normal rounded face is restored on release. */
    lcdm_raw_fill((uint16_t)(x + 2U),
                  (uint16_t)(LCDM_KEY_Y0 + 2U),
                  (uint16_t)(LCDM_KEY_W - 4U),
                  (uint16_t)(LCDM_KEY_H - 4U),
                  bg);
  } else {
    lcdm_raw_round_rect((uint16_t)(x + 2U),
                        (uint16_t)(LCDM_KEY_Y0 + 2U),
                        (uint16_t)(LCDM_KEY_W - 4U),
                        (uint16_t)(LCDM_KEY_H - 4U),
                        7U,
                        bg);
  }
  lcdm_raw_xstr_full((uint16_t)(x + 4U),
                     (uint16_t)(LCDM_KEY_Y0 + 5U),
                     112U,
                     31U,
                     LCDM_FONT_TITLE,
                     LCDM_WHITE,
                     bg,
                     1U,
                     top);
  lcdm_raw_xstr_full((uint16_t)(x + 4U),
                     (uint16_t)(LCDM_KEY_Y0 + 34U),
                     112U,
                     19U,
                     LCDM_FONT_SMALL,
                     bottom_fg,
                     bg,
                     1U,
                     bottom);
}

static void lcdm_raw_draw_key_caption(uint8_t index, uint8_t active)
{
  uint16_t x = (uint16_t)(index * LCDM_KEY_W);
  uint16_t bg = lcdm_key_background(index, active);
  uint16_t bottom_fg;
  const char *top;
  const char *bottom;

  lcdm_key_labels(index, &top, &bottom, &bottom_fg);
  (void)top;
  lcdm_raw_xstr_full((uint16_t)(x + 4U),
                     (uint16_t)(LCDM_KEY_Y0 + 34U),
                     112U,
                     19U,
                     LCDM_FONT_SMALL,
                     bottom_fg,
                     bg,
                     1U,
                     bottom);
}

static void lcdm_raw_draw_key(uint8_t index, uint8_t active)
{
  uint16_t x = (uint16_t)(index * LCDM_KEY_W);

  lcdm_raw_fill(x, LCDM_KEY_Y0, LCDM_KEY_W, LCDM_KEY_H, LCDM_WHITE);
  lcdm_raw_draw_key_face(index, active);
}

static void lcdm_raw_draw_keys(void)
{
  lcdm_raw_draw_key(0U, lcdm_current_key == FIRST_GEN_KEY_SET);
  lcdm_raw_draw_key(1U, lcdm_current_key == FIRST_GEN_KEY_CLEAR);
  lcdm_raw_draw_key(2U, lcdm_current_key == FIRST_GEN_KEY_PLUS);
  lcdm_raw_draw_key(3U, lcdm_current_key == FIRST_GEN_KEY_MINUS);
}

static void lcdm_set_k1_page_hint(uint8_t enabled)
{
  enabled = (enabled != 0U) ? 1U : 0U;
  if(lcdm_k1_page_hint == enabled) {
    return;
  }

  lcdm_k1_page_hint = enabled;
  /* Only redraw when the caption actually changes.  Result-page turns leave
   * K1-K4 untouched and repaint the result area alone. */
  lcdm_raw_draw_key_face(0U, lcdm_current_key == FIRST_GEN_KEY_SET);
}

static void lcdm_set_auto_test_blink(uint8_t enabled)
{
  enabled = (enabled != 0U) ? 1U : 0U;
  if(lcdm_auto_test_blink_active == enabled &&
     (enabled == 0U || lcdm_auto_test_blink_on != 0U)) {
    return;
  }

  lcdm_auto_test_blink_active = enabled;
  lcdm_auto_test_blink_on = 1U;
  lcdm_auto_test_blink_steps = 0U;
  lcdm_raw_draw_key_caption(1U, lcdm_current_key == FIRST_GEN_KEY_CLEAR);
}

static void lcdm_auto_test_blink_step(void)
{
  if(lcdm_auto_test_blink_active == 0U) {
    return;
  }

  lcdm_auto_test_blink_steps++;
  if(lcdm_auto_test_blink_steps < LCDM_AUTO_TEST_BLINK_STEPS) {
    return;
  }

  lcdm_auto_test_blink_steps = 0U;
  lcdm_auto_test_blink_on ^= 1U;
  lcdm_raw_draw_key_caption(1U, lcdm_current_key == FIRST_GEN_KEY_CLEAR);
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

  /* Repaint only this button's face.  It darkens on touch and returns to its
   * normal shade on release without clearing the key band first. */
  lcdm_raw_draw_key_face(index, active);
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
  lcdm_raw_xstr_full((uint16_t)(LCDM_RESULT_X + 4U), (uint16_t)(LCDM_DETAIL_Y + 4U), (uint16_t)(LCDM_PASS_W - 8U), (uint16_t)(LCDM_DETAIL_H - 8U), LCDM_FONT_RESULT, LCDM_WHITE, bg, 1U, "PASS");
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
  lcdm_raw_xstr_full((uint16_t)(LCDM_PRINT_X + 6U), (uint16_t)(LCDM_DETAIL_Y + 4U), (uint16_t)(LCDM_PRINT_W - 12U), (uint16_t)(LCDM_DETAIL_H - 8U), LCDM_FONT_RESULT, fg, bg, 1U, lcdm_raw_print_cache);
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
                     LCDM_FONT_TITLE,
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
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_tjc_send_cmd("cls 65535");
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  lcdm_raw_xstr_full(8U, 3U, 220U, 26U, LCDM_FONT_TITLE, LCDM_WHITE, LCDM_NAVY, 0U, "STANDARD CABLE");
  lcdm_raw_xstr_full(300U, 3U, 172U, 26U, LCDM_FONT_TITLE, LCDM_WHITE, LCDM_NAVY, 2U, "WIRE TESTER");
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
  lcdm_auto_test_blink_active = 0U;
  lcdm_auto_test_blink_on = 1U;
  lcdm_auto_test_blink_steps = 0U;
}

static void lcdm_draw_hall_indicator(uint8_t force)
{
  uint16_t color;

  if(force == 0U && lcdm_hall_input_drawn != 0U) {
    return;
  }

  color = (lcdm_hall_input_active != 0U) ? LCDM_RED : LCDM_GRAY;
  lcdm_raw_fill(LCDM_HALL_X, LCDM_HALL_Y, LCDM_HALL_W, LCDM_HALL_H, LCDM_NAVY);
  lcdm_raw_xstr_full(LCDM_HALL_X,
                     LCDM_HALL_Y,
                     LCDM_HALL_W,
                     LCDM_HALL_H,
                     LCDM_FONT_SMALL,
                     color,
                     LCDM_NAVY,
                     1U,
                     "HALL IN");
  lcdm_hall_input_drawn = 1U;
}

static void lcdm_draw_wifi_indicator(uint8_t force)
{
  uint16_t foreground;

  if(force == 0U && lcdm_wifi_indicator_drawn != 0U) {
    return;
  }

  /* Keep the label visible like HALL IN; green means the production ESP-AT
   * session is ONLINE, while gray makes an offline/starting state explicit. */
  foreground = (lcdm_wifi_connected != 0U) ? LCDM_GREEN : LCDM_GRAY;
  lcdm_raw_fill(LCDM_WIFI_X, LCDM_WIFI_Y, LCDM_WIFI_W, LCDM_WIFI_H, LCDM_NAVY);
  lcdm_raw_xstr_full(LCDM_WIFI_X,
                     LCDM_WIFI_Y,
                     LCDM_WIFI_W,
                     LCDM_WIFI_H,
                     LCDM_FONT_SMALL,
                     foreground,
                     LCDM_NAVY,
                     1U,
                     "WIFI");
  lcdm_wifi_indicator_drawn = 1U;
}

static void lcdm_draw_common_header(const char *top_right)
{
  if(top_right == 0) {
    top_right = "";
  }

  /* The full-screen base invalidates only the AUTO middle-area shadow cache;
   * subsequent live AUTO updates will redraw their rows, never the header or
   * K1-K4 strip again. */
  lcdm_auto_drawn_rows_invalidate();
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  (void)top_right;
  lcdm_k1_page_hint = 0U;
  lcdm_raw_xstr_full(0U, 1U, LCDM_W, 30U, LCDM_FONT_TITLE, LCDM_WHITE, LCDM_NAVY, 1U, "STANDARD CABLE");
  lcdm_draw_wifi_indicator(1U);
  lcdm_draw_hall_indicator(1U);
  lcdm_raw_draw_keys();
  lcdm_reset_text_caches();
}

static void lcdm_draw_auto_header(const char *title)
{
  (void)title;
  lcdm_auto_drawn_rows_invalidate();
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  lcdm_k1_page_hint = 0U;
  lcdm_raw_xstr_full(0U,
                     1U,
                     LCDM_W,
                     30U,
                     LCDM_FONT_TITLE,
                     LCDM_WHITE,
                     LCDM_NAVY,
                     1U,
                     "STANDARD CABLE");
  lcdm_draw_wifi_indicator(1U);
  lcdm_draw_hall_indicator(1U);
  lcdm_raw_draw_keys();
  lcdm_reset_text_caches();
}

static void lcdm_draw_print_progress_body(uint8_t state)
{
  const char *text;
  const char *status;
  uint16_t background;

  if(state == FIRST_GEN_PRINT_DISPLAY_COMPLETE) {
    text = "COMPLETE";
    status = "WAITING FOR PRINTING";
    background = LCDM_GREEN;
  } else if(state == FIRST_GEN_PRINT_DISPLAY_ERROR) {
    text = "NETWORK ERROR";
    status = "K3 RESET / RETRY PRINT";
    background = LCDM_RED;
  } else {
    text = "START PRINTING";
    status = "WAITING FOR PRINTING";
    background = LCDM_GREEN;
  }

  lcdm_raw_fill(0U,
                LCDM_PRINT_STATUS_Y,
                LCDM_W,
                LCDM_PRINT_STATUS_H,
                LCDM_WHITE);
  lcdm_raw_xstr_full(0U,
                     LCDM_PRINT_STATUS_Y,
                     LCDM_W,
                     LCDM_PRINT_STATUS_H,
                     LCDM_FONT_TITLE,
                     LCDM_BLACK,
                     LCDM_WHITE,
                     1U,
                     status);
  lcdm_raw_fill(0U,
                LCDM_PRINT_BODY_Y,
                LCDM_W,
                LCDM_PRINT_BODY_H,
                background);
  lcdm_raw_xstr_full(0U,
                     LCDM_PRINT_BODY_Y,
                     LCDM_W,
                     LCDM_PRINT_BODY_H,
                     LCDM_FONT_POINT,
                     LCDM_WHITE,
                     background,
                     1U,
                     text);
}

static void lcdm_draw_body_text(uint16_t y, uint16_t h, const char *text, uint16_t fg, uint16_t bg)
{
  lcdm_raw_fill(24U, y, 432U, h, bg);
  lcdm_raw_xstr_full(24U, y, 432U, h, LCDM_FONT_STATUS, fg, bg, 1U, text);
}

static void lcdm_draw_learning_pair_text(const char *text)
{
  lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
  lcdm_raw_xstr_full(24U, 84U, 432U, 82U, LCDM_FONT_POINT, LCDM_NAVY, LCDM_WHITE, 1U, text);
}

static void lcdm_draw_split_body(uint16_t left_x,
                                 uint16_t y,
                                 uint16_t left_w,
                                 uint16_t right_w,
                                 uint16_t h,
                                 uint16_t left_font,
                                 uint16_t right_font,
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
  lcdm_raw_xstr_full(left_x, y, left_w, h, left_font, left_fg, left_bg, 1U, left_text);
  lcdm_raw_xstr_full((uint16_t)(left_x + left_w), y, right_w, h, right_font, right_fg, right_bg, 1U, right_text);
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
  /* PASS and NG use the largest available Song resource (ID 0, 32 px).
   * The left result panel is 82 px high, so the font remains fully inside
   * its bounds while being clearly larger than the former 29 px PASS. */
  lcdm_raw_xstr_full(18U, 84U, 228U, 82U, LCDM_FONT_POINT, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(19U, 84U, 228U, 82U, LCDM_FONT_POINT, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(18U, 85U, 228U, 82U, LCDM_FONT_POINT, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(19U, 85U, 228U, 82U, LCDM_FONT_POINT, LCDM_BLACK, LCDM_GREEN, 1U, "PASS");
  lcdm_raw_xstr_full(244U, 84U, 208U, 26U, LCDM_FONT_STATUS, LCDM_NAVY, LCDM_WHITE, 1U, total_text);
  lcdm_raw_xstr_full(244U, 111U, 208U, 26U, LCDM_FONT_STATUS, LCDM_NAVY, LCDM_WHITE, 1U, pairs_text);
  lcdm_raw_xstr_full(244U, 138U, 208U, 28U, LCDM_FONT_STATUS, LCDM_NAVY, LCDM_WHITE, 1U, points_text);
}

static void lcdm_draw_auto_result_pass_body(const char *detail_text)
{
  char total_text[8] = "TOTAL";
  char pairs_text[24];
  char points_text[24];
  unsigned int pairs = 0U;
  unsigned int points = 0U;
  uint16_t total_h;
  uint16_t pairs_y;
  uint16_t pairs_h;
  uint16_t points_y;
  uint16_t points_h;

  if(detail_text == 0) {
    detail_text = "";
  }

  if(sscanf(detail_text, "TOTAL - %u PAIRS/%u POINTS", &pairs, &points) == 2) {
    (void)snprintf(pairs_text, sizeof(pairs_text), "%03u PAIRS", pairs);
    (void)snprintf(points_text, sizeof(points_text), "%03u POINTS", points);
  } else {
    (void)snprintf(pairs_text, sizeof(pairs_text), "%s", detail_text);
    points_text[0] = '\0';
  }

  /* TOTAL and POINTS retain their original cells.  PAIRS is deliberately
   * lowered inside the middle region so its smaller Song glyph does not sit
   * too close to the larger TOTAL text above it. */
  total_h = (uint16_t)(LCDM_AUTO_SUMMARY_H / 3U);
  points_y = (uint16_t)(LCDM_AUTO_SUMMARY_Y + ((LCDM_AUTO_SUMMARY_H * 2U) / 3U));
  points_h = (uint16_t)(LCDM_AUTO_SUMMARY_H - ((LCDM_AUTO_SUMMARY_H * 2U) / 3U));
  pairs_y = (uint16_t)(LCDM_AUTO_SUMMARY_Y + total_h + 11U);
  pairs_h = (uint16_t)(points_y - pairs_y);
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_X,
                LCDM_AUTO_SUMMARY_Y,
                LCDM_AUTO_SUMMARY_LEFT_W,
                LCDM_AUTO_SUMMARY_H,
                LCDM_DARK_GREEN);
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_RIGHT_X,
                LCDM_AUTO_SUMMARY_Y,
                LCDM_AUTO_SUMMARY_RIGHT_W,
                LCDM_AUTO_SUMMARY_H,
                LCDM_DARK_GREEN);
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_DIVIDER_X,
                LCDM_AUTO_SUMMARY_Y,
                2U,
                LCDM_AUTO_SUMMARY_H,
                LCDM_BLACK);
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_X,
                     LCDM_AUTO_SUMMARY_Y,
                     LCDM_AUTO_SUMMARY_LEFT_W,
                     LCDM_AUTO_SUMMARY_H,
                     LCDM_FONT_POINT,
                     LCDM_WHITE,
                     LCDM_DARK_GREEN,
                     1U,
                     "PASS");
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_TEXT_X,
                     LCDM_AUTO_SUMMARY_Y,
                     LCDM_AUTO_SUMMARY_TEXT_W,
                     total_h,
                     LCDM_FONT_RESULT,
                     LCDM_WHITE,
                     LCDM_DARK_GREEN,
                     1U,
                     total_text);
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_TEXT_X,
                     pairs_y,
                     LCDM_AUTO_SUMMARY_TEXT_W,
                     pairs_h,
                     /* "047 PAIRS" and "094 POINTS" are respectively 9
                      * and 10 characters.  The 29 px resource clips their
                      * last letters in the 3/5 panel; the 16 px Song font
                      * keeps both values complete while retaining a large
                      * three-line summary. */
                     LCDM_FONT_TITLE,
                     LCDM_WHITE,
                     LCDM_DARK_GREEN,
                     1U,
                     pairs_text);
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_TEXT_X,
                     points_y,
                     LCDM_AUTO_SUMMARY_TEXT_W,
                     points_h,
                     LCDM_FONT_TITLE,
                     LCDM_WHITE,
                     LCDM_DARK_GREEN,
                     1U,
                     points_text);
}

/* Append comma-separated endpoint labels without ever splitting an Ixxx or
 * Oxxx token.  The caller controls whether a row break is needed. */
static uint8_t lcdm_auto_ng_append_csv_lines(
    const char *text,
    uint16_t text_len,
    uint8_t char_limit,
    uint8_t reserve_last_char,
    uint8_t start_new_line,
    char lines[LCDM_AUTO_SUMMARY_NG_MAX_LINES][LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX],
    uint8_t *line_count,
    uint8_t max_lines)
{
  uint16_t token_start = 0U;
  uint16_t token_end;
  uint16_t token_len;
  uint8_t usable_limit;
  uint8_t current;
  uint8_t current_len;
  uint8_t needs_comma;

  if(text == 0 || line_count == 0 || char_limit == 0U ||
     reserve_last_char >= char_limit) {
    return 0U;
  }

  usable_limit = (uint8_t)(char_limit - reserve_last_char);
  if(start_new_line != 0U && *line_count != 0U) {
    if(*line_count >= max_lines) {
      return 0U;
    }
    lines[*line_count][0] = '\0';
    (*line_count)++;
  }

  while(token_start < text_len) {
    token_end = token_start;
    while(token_end < text_len && text[token_end] != ',') {
      token_end++;
    }
    token_len = (uint16_t)(token_end - token_start);
    if(token_len != 0U) {
      if(token_len > usable_limit || token_len >= LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX) {
        return 0U;
      }
      if(*line_count == 0U) {
        if(*line_count >= max_lines) {
          return 0U;
        }
        lines[*line_count][0] = '\0';
        (*line_count)++;
      }

      current = (uint8_t)(*line_count - 1U);
      current_len = (uint8_t)strlen(lines[current]);
      needs_comma = (current_len != 0U) ? 1U : 0U;
      if((uint16_t)current_len + (uint16_t)needs_comma + token_len > usable_limit) {
        if(*line_count >= max_lines) {
          return 0U;
        }
        lines[*line_count][0] = '\0';
        (*line_count)++;
        current = (uint8_t)(*line_count - 1U);
        current_len = 0U;
        needs_comma = 0U;
      }

      if(needs_comma != 0U) {
        lines[current][current_len++] = ',';
      }
      memcpy(&lines[current][current_len], &text[token_start], token_len);
      current_len = (uint8_t)(current_len + token_len);
      lines[current][current_len] = '\0';
    }

    token_start = (uint16_t)(token_end + 1U);
  }

  return 1U;
}

/* Format one endpoint group only.  The caller places the I group in the
 * upper half of the NG body and the O group in the lower half, so NG itself
 * never needs to consume display space. */
static uint8_t lcdm_auto_ng_format_group_lines(
    const char *text,
    uint16_t text_len,
    uint8_t char_limit,
    uint8_t max_lines,
    char lines[LCDM_AUTO_SUMMARY_NG_MAX_LINES][LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX],
    uint8_t *overflow)
{
  uint8_t line_count = 0U;

  (void)memset(lines,
               0,
               LCDM_AUTO_SUMMARY_NG_MAX_LINES * LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX);
  if(overflow != 0) {
    *overflow = 0U;
  }
  if(text == 0 || text_len == 0U || char_limit == 0U || max_lines == 0U) {
    return 0U;
  }

  if(text_len <= char_limit && text_len < LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX) {
    memcpy(lines[0], text, text_len);
    lines[0][text_len] = '\0';
    return 1U;
  }

  if(lcdm_auto_ng_append_csv_lines(text,
                                   text_len,
                                   char_limit,
                                   0U,
                                   0U,
                                   lines,
                                   &line_count,
                                   max_lines) == 0U && overflow != 0) {
    *overflow = 1U;
  }

  return line_count;
}

static void lcdm_draw_auto_ng_group(const char *text,
                                    uint16_t text_len,
                                    uint16_t y,
                                    uint16_t h,
                                    uint8_t fast)
{
  char lines[LCDM_AUTO_SUMMARY_NG_MAX_LINES][LCDM_AUTO_SUMMARY_NG_LINE_TEXT_MAX];
  uint8_t line_count;
  /* Do not wrap an NG endpoint list: show the first (lowest) labels at the
   * largest readable font and hide later labels until earlier faults clear. */
  uint8_t char_limit = 24U;
  uint16_t font = LCDM_FONT_POINT;

  if(text == 0 || text_len == 0U) {
    return;
  }

  line_count = lcdm_auto_ng_format_group_lines(text,
                                                text_len,
                                                char_limit,
                                                1U,
                                                lines,
                                                0);

  if(line_count == 0U) {
    line_count = 1U;
    (void)snprintf(lines[0], sizeof(lines[0]), "%s", "FAULT");
  }

  if(fast != 0U) {
    lcdm_raw_xstr_full_fast(LCDM_AUTO_SUMMARY_FULL_TEXT_X,
                             y,
                             LCDM_AUTO_SUMMARY_FULL_TEXT_W,
                             h,
                             font,
                             LCDM_WHITE,
                             LCDM_RED,
                             1U,
                             lines[0]);
  } else {
    lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_FULL_TEXT_X,
                       y,
                       LCDM_AUTO_SUMMARY_FULL_TEXT_W,
                       h,
                       font,
                       LCDM_WHITE,
                       LCDM_RED,
                       1U,
                       lines[0]);
  }
}

static void lcdm_draw_auto_result_ng_detail(const char *detail_text)
{
  const char *input_text;
  const char *output_text;
  const char *separator;
  uint16_t input_len;
  uint16_t output_len;
  uint16_t top_h;
  uint16_t divider_y;
  uint16_t bottom_y;
  uint16_t bottom_h;

  if(detail_text == 0) {
    detail_text = "";
  }

  /* NG is a full-width red I/O record.  Do not reserve a left "NG" cell;
   * the actual Ixxx/Oxxx labels are the information the operator needs. */
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_X,
                LCDM_AUTO_SUMMARY_Y,
                LCDM_AUTO_SUMMARY_W,
                LCDM_AUTO_SUMMARY_H,
                LCDM_RED);

  top_h = (uint16_t)((LCDM_AUTO_SUMMARY_H - 2U) / 2U);
  divider_y = (uint16_t)(LCDM_AUTO_SUMMARY_Y + top_h);
  bottom_y = (uint16_t)(divider_y + 2U);
  bottom_h = (uint16_t)(LCDM_AUTO_SUMMARY_H - top_h - 2U);

  if(detail_text[0] != '\0') {
    separator = strchr(detail_text, '-');
    input_text = detail_text;
    input_len = (uint16_t)strlen(detail_text);
    output_text = 0;
    output_len = 0U;
    if(separator != 0 && separator != detail_text && separator[1] != '\0') {
      input_len = (uint16_t)(separator - detail_text);
      output_text = separator + 1;
      output_len = (uint16_t)strlen(output_text);
    }

    lcdm_draw_auto_ng_group(input_text,
                            input_len,
                            LCDM_AUTO_SUMMARY_Y,
                            top_h,
                            0U);
    lcdm_draw_auto_ng_group(output_text, output_len, bottom_y, bottom_h, 0U);
  }

  lcdm_raw_fill(LCDM_AUTO_SUMMARY_X,
                divider_y,
                LCDM_AUTO_SUMMARY_W,
                2U,
                LCDM_BLACK);
}

/* The initial NG page is drawn through the reliable acknowledged path above.
 * Later blink frames do not need to rebuild the fixed red body: hiding the
 * text takes two fast fills, while restoring it sends only the text rows.
 * That keeps the scanner running even when the display is busy rendering a
 * blink frame. */
static void lcdm_draw_auto_result_ng_blink_detail(const char *detail_text)
{
  const char *input_text;
  const char *output_text;
  const char *separator;
  uint16_t input_len;
  uint16_t output_len;
  uint16_t top_h;
  uint16_t divider_y;
  uint16_t bottom_y;
  uint16_t bottom_h;

  top_h = (uint16_t)((LCDM_AUTO_SUMMARY_H - 2U) / 2U);
  divider_y = (uint16_t)(LCDM_AUTO_SUMMARY_Y + top_h);
  bottom_y = (uint16_t)(divider_y + 2U);
  bottom_h = (uint16_t)(LCDM_AUTO_SUMMARY_H - top_h - 2U);

  if(detail_text == 0 || detail_text[0] == '\0') {
    /* The red body was already established by the reliable initial draw.
     * Clear only its text content, then restore the fixed I/O divider. */
    lcdm_raw_fill_fast(LCDM_AUTO_SUMMARY_X,
                       LCDM_AUTO_SUMMARY_Y,
                       LCDM_AUTO_SUMMARY_W,
                       LCDM_AUTO_SUMMARY_H,
                       LCDM_RED);
    lcdm_raw_fill_fast(LCDM_AUTO_SUMMARY_X,
                       divider_y,
                       LCDM_AUTO_SUMMARY_W,
                       2U,
                       LCDM_BLACK);
    return;
  }

  separator = strchr(detail_text, '-');
  input_text = detail_text;
  input_len = (uint16_t)strlen(detail_text);
  output_text = 0;
  output_len = 0U;
  if(separator != 0 && separator != detail_text && separator[1] != '\0') {
    input_len = (uint16_t)(separator - detail_text);
    output_text = separator + 1;
    output_len = (uint16_t)strlen(output_text);
  }

  lcdm_draw_auto_ng_group(input_text,
                          input_len,
                          LCDM_AUTO_SUMMARY_Y,
                          top_h,
                          1U);
  lcdm_draw_auto_ng_group(output_text, output_len, bottom_y, bottom_h, 1U);
}

static void lcdm_draw_auto_result_ng_body(const char *detail_text)
{
  lcdm_draw_auto_result_ng_detail(detail_text);
}

static void lcdm_draw_result_ng_detail(const char *detail_text)
{
  uint16_t text_len;
  uint16_t split;
  uint16_t probe;
  char line_one[64];
  char line_two[64];

  if(detail_text == 0) {
    detail_text = "";
  }

  lcdm_raw_fill(240U, 84U, 216U, 82U, LCDM_RED);
  text_len = (uint16_t)strlen(detail_text);
  if(text_len == 0U) {
    return;
  }

  /* The current fault sequence is a primary result, not a footnote: use the
   * maximum 32 px Song font.  For a multi-endpoint group, break it on a
   * comma or the I/O separator so two 32 px rows remain readable. */
  if(text_len <= 11U) {
    lcdm_raw_xstr_full(244U, 84U, 208U, 82U, LCDM_FONT_POINT, LCDM_WHITE, LCDM_RED, 1U, detail_text);
    return;
  }

  split = (uint16_t)(text_len / 2U);
  for(probe = 0U; probe < text_len; probe++) {
    uint16_t right = (uint16_t)(split + probe);
    uint16_t left = (split > probe) ? (uint16_t)(split - probe) : 0U;

    if(right < text_len && (detail_text[right] == ',' || detail_text[right] == '-')) {
      split = (detail_text[right] == ',') ? (uint16_t)(right + 1U) : right;
      break;
    }
    if(left < text_len && (detail_text[left] == ',' || detail_text[left] == '-')) {
      split = (detail_text[left] == ',') ? (uint16_t)(left + 1U) : left;
      break;
    }
  }
  if(split == 0U || split >= text_len) {
    split = (uint16_t)(text_len / 2U);
  }
  if(split >= sizeof(line_one)) {
    split = (uint16_t)(sizeof(line_one) - 1U);
  }

  memcpy(line_one, detail_text, split);
  line_one[split] = '\0';
  (void)snprintf(line_two, sizeof(line_two), "%s", &detail_text[split]);
  lcdm_raw_xstr_full(244U, 84U, 208U, 40U, LCDM_FONT_POINT, LCDM_WHITE, LCDM_RED, 1U, line_one);
  lcdm_raw_xstr_full(244U, 125U, 208U, 40U, LCDM_FONT_POINT, LCDM_WHITE, LCDM_RED, 1U, line_two);
}

static void lcdm_draw_result_ng_body(const char *detail_text)
{
  lcdm_raw_fill(24U, 84U, 216U, 82U, LCDM_RED);
  lcdm_raw_fill(239U, 84U, 2U, 82U, LCDM_BLACK);
  lcdm_raw_xstr_full(24U, 84U, 216U, 82U, LCDM_FONT_POINT, LCDM_WHITE, LCDM_RED, 1U, "NG");
  lcdm_draw_result_ng_detail(detail_text);
}

static void lcdm_draw_top_split_labels(const char *left_text, const char *right_text, uint16_t fg, uint16_t bg)
{
  lcdm_raw_fill(24U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, bg);
  lcdm_raw_fill(240U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, bg);
  lcdm_raw_fill(240U, LCDM_STATUS_Y, 2U, LCDM_STATUS_H, LCDM_BLACK);
  lcdm_raw_xstr_full(24U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, LCDM_FONT_STATUS, fg, bg, 1U, left_text);
  lcdm_raw_xstr_full(240U, LCDM_STATUS_Y, 216U, LCDM_STATUS_H, LCDM_FONT_STATUS, fg, bg, 1U, right_text);
}

static void lcdm_draw_auto_summary_top_labels(const char *left_text, const char *right_text)
{
  if(left_text == 0) {
    left_text = "";
  }
  if(right_text == 0) {
    right_text = "";
  }

  lcdm_raw_fill(LCDM_AUTO_SUMMARY_X,
                LCDM_AUTO_SUMMARY_LABEL_Y,
                LCDM_AUTO_SUMMARY_LEFT_W,
                LCDM_AUTO_SUMMARY_LABEL_H,
                LCDM_WHITE);
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_RIGHT_X,
                LCDM_AUTO_SUMMARY_LABEL_Y,
                LCDM_AUTO_SUMMARY_RIGHT_W,
                LCDM_AUTO_SUMMARY_LABEL_H,
                LCDM_WHITE);
  lcdm_raw_fill(LCDM_AUTO_SUMMARY_DIVIDER_X,
                LCDM_AUTO_SUMMARY_LABEL_Y,
                2U,
                LCDM_AUTO_SUMMARY_LABEL_H,
                LCDM_BLACK);
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_X,
                     LCDM_AUTO_SUMMARY_LABEL_Y,
                     LCDM_AUTO_SUMMARY_LEFT_W,
                     LCDM_AUTO_SUMMARY_LABEL_H,
                     LCDM_FONT_SMALL,
                     LCDM_BLACK,
                     LCDM_WHITE,
                     1U,
                     left_text);
  lcdm_raw_xstr_full(LCDM_AUTO_SUMMARY_RIGHT_X,
                     LCDM_AUTO_SUMMARY_LABEL_Y,
                     LCDM_AUTO_SUMMARY_RIGHT_W,
                     LCDM_AUTO_SUMMARY_LABEL_H,
                     LCDM_FONT_SMALL,
                     LCDM_BLACK,
                     LCDM_WHITE,
                     1U,
                     right_text);
}

static void lcdm_prepare_standard_page(const char *top_right)
{
  if(top_right == 0) {
    top_right = "";
  }

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

static void lcdm_auto_drawn_rows_invalidate(void)
{
  uint8_t row;

  lcdm_auto_drawn_page = 0U;
  for(row = 0U; row < LCDM_AUTO_LIST_PAGE_SIZE; row++) {
    lcdm_auto_drawn_row_cache[row][0] = '\0';
  }
}

static void lcdm_auto_test_clear_result_lines(void)
{
  uint16_t point;

  for(point = 0U; point <= LCDM_TOTAL_POINTS; point++) {
    lcdm_auto_line_cache[point][0] = '\0';
  }
  lcdm_auto_line_count = 0U;
  lcdm_auto_input_count = 0U;
  lcdm_auto_point_count = 0UL;
  lcdm_auto_actual_input_count = 0U;
  lcdm_auto_actual_output_count = 0U;
  lcdm_auto_actual_counts_valid = 0U;
}

static void lcdm_auto_test_clear(void)
{
  uint8_t word;

  lcdm_auto_test_clear_result_lines();
  lcdm_auto_page_cache = 0U;
  lcdm_auto_footer_cache[0] = '\0';
  lcdm_auto_drawn_rows_invalidate();
  for(word = 0U; word < FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS; word++) {
    lcdm_auto_fault_out_bits[word] = 0U;
    lcdm_auto_fault_in_bits[word] = 0U;
  }
  lcdm_auto_fault_type = 0U;
  lcdm_auto_fault_blink_on = 0U;
}

static uint16_t lcdm_learn_group_color(uint16_t out_point)
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
    LCDM_GRAY
  };

  if(out_point == 0U) {
    out_point = 1U;
  }
  return colors[(out_point - 1U) % (sizeof(colors) / sizeof(colors[0]))];
}

static uint16_t lcdm_learn_text_color(uint16_t bg)
{
  if(bg == LCDM_PALE_CYAN || bg == LCDM_PALE_BLUE || bg == LCDM_GREEN) {
    return LCDM_NAVY;
  }
  return LCDM_WHITE;
}

static void lcdm_auto_visual_row_reset(lcdm_auto_visual_row_t *row)
{
  if(row == 0) {
    return;
  }

  row->text[0] = '\0';
  row->in_len = 0U;
  row->separator_len = 0U;
  row->out_len = 0U;
}

static uint8_t lcdm_auto_visual_row_append(lcdm_auto_visual_row_t *row,
                                           const char *text,
                                           uint8_t text_len,
                                           uint8_t part)
{
  uint16_t used;

  if(row == 0 || text == 0 || text_len == 0U) {
    return 0U;
  }

  used = (uint16_t)row->in_len + (uint16_t)row->separator_len + (uint16_t)row->out_len;
  if((uint16_t)(used + text_len) > LCDM_AUTO_ROW_CHAR_LIMIT) {
    return 0U;
  }

  memcpy(&row->text[used], text, text_len);
  row->text[used + text_len] = '\0';
  if(part == 0U) {
    row->in_len = (uint8_t)(row->in_len + text_len);
  } else if(part == 1U) {
    row->separator_len = (uint8_t)(row->separator_len + text_len);
  } else {
    row->out_len = (uint8_t)(row->out_len + text_len);
  }
  return 1U;
}

static void lcdm_auto_visual_row_emit(const lcdm_auto_visual_row_t *row,
                                      uint16_t *row_count,
                                      uint16_t wanted_row,
                                      lcdm_auto_visual_row_t *wanted)
{
  if(row == 0 || row_count == 0 || row->text[0] == '\0') {
    return;
  }

  *row_count = (uint16_t)(*row_count + 1U);
  if(wanted != 0 && wanted_row == *row_count) {
    *wanted = *row;
  }
}

static uint8_t lcdm_auto_endpoint_unit_length(const char *cursor, const char *end)
{
  uint8_t length = 1U;

  if(cursor == 0 || end == 0 || cursor >= end) {
    return 0U;
  }

  if((*cursor == 'I' || *cursor == 'O') &&
     (end - cursor) >= 4 &&
     cursor[1] >= '0' && cursor[1] <= '9' &&
     cursor[2] >= '0' && cursor[2] <= '9' &&
     cursor[3] >= '0' && cursor[3] <= '9') {
    length = 4U;
    if((cursor + length) < end && cursor[length] == ',') {
      length++;
    }
  }

  return length;
}

static void lcdm_auto_visual_append_unit(lcdm_auto_visual_row_t *row,
                                         uint16_t *row_count,
                                         uint16_t wanted_row,
                                         lcdm_auto_visual_row_t *wanted,
                                         const char *text,
                                         uint8_t text_len,
                                         uint8_t part)
{
  if(lcdm_auto_visual_row_append(row, text, text_len, part) != 0U) {
    return;
  }

  /* Endpoints are moved as whole Ixxx/Oxxx tokens.  A full physical row
   * therefore starts a fresh row rather than cutting off an identifier. */
  lcdm_auto_visual_row_emit(row, row_count, wanted_row, wanted);
  lcdm_auto_visual_row_reset(row);
  (void)lcdm_auto_visual_row_append(row, text, text_len, part);
}

/* Keep '-' attached to the first Oxxx token.  A nearly full I list must not
 * leave a lone dash at the far right of one row and its O value on the next. */
static void lcdm_auto_visual_append_first_output(lcdm_auto_visual_row_t *row,
                                                 uint16_t *row_count,
                                                 uint16_t wanted_row,
                                                 lcdm_auto_visual_row_t *wanted,
                                                 const char *text,
                                                 uint8_t text_len)
{
  uint16_t used;

  if(row == 0 || text == 0 || text_len == 0U) {
    return;
  }

  used = (uint16_t)row->in_len + (uint16_t)row->separator_len + (uint16_t)row->out_len;
  if((uint16_t)(used + 1U + text_len) > LCDM_AUTO_ROW_CHAR_LIMIT) {
    lcdm_auto_visual_row_emit(row, row_count, wanted_row, wanted);
    lcdm_auto_visual_row_reset(row);
  }

  (void)lcdm_auto_visual_row_append(row, "-", 1U, 1U);
  (void)lcdm_auto_visual_row_append(row, text, text_len, 2U);
}

static void lcdm_auto_visual_append_range(lcdm_auto_visual_row_t *row,
                                          uint16_t *row_count,
                                          uint16_t wanted_row,
                                          lcdm_auto_visual_row_t *wanted,
                                          const char *cursor,
                                          const char *end,
                                          uint8_t part)
{
  uint8_t unit_len;

  while(cursor != 0 && end != 0 && cursor < end) {
    unit_len = lcdm_auto_endpoint_unit_length(cursor, end);
    if(unit_len == 0U) {
      return;
    }
    lcdm_auto_visual_append_unit(row,
                                 row_count,
                                 wanted_row,
                                 wanted,
                                 cursor,
                                 unit_len,
                                 part);
    cursor += unit_len;
  }
}

/* A component record remains one electrical circuit internally, but is
 * expanded into as many physical screen rows as its I/O endpoints require.
 * Starting every component on a new row keeps the eventual CSV-style record
 * easy to read while all of its I points remain visible. */
static uint16_t lcdm_auto_collect_visual_rows(uint16_t wanted_row,
                                              lcdm_auto_visual_row_t *wanted)
{
  uint16_t component;
  uint16_t row_count = 0U;
  const char *line;
  const char *dash;
  const char *input_end;
  const char *output_start;
  const char *output_end;
  uint8_t first_output_len;
  lcdm_auto_visual_row_t row;

  if(wanted != 0) {
    lcdm_auto_visual_row_reset(wanted);
  }

  for(component = 1U; component <= lcdm_auto_line_count; component++) {
    line = lcdm_auto_line_cache[component];
    if(line[0] == '\0') {
      continue;
    }

    lcdm_auto_visual_row_reset(&row);
    dash = strchr(line, '-');
    if(dash == 0) {
      lcdm_auto_visual_append_range(&row,
                                    &row_count,
                                    wanted_row,
                                    wanted,
                                    line,
                                    line + strlen(line),
                                    0U);
    } else {
      input_end = dash;
      while(input_end > line && input_end[-1] == ' ') {
        input_end--;
      }
      lcdm_auto_visual_append_range(&row,
                                    &row_count,
                                    wanted_row,
                                    wanted,
                                    line,
                                    input_end,
                                    0U);
      output_start = dash + 1;
      while(*output_start == ' ') {
        output_start++;
      }
      output_end = output_start + strlen(output_start);
      first_output_len = lcdm_auto_endpoint_unit_length(output_start, output_end);
      if(first_output_len != 0U) {
        lcdm_auto_visual_append_first_output(&row,
                                             &row_count,
                                             wanted_row,
                                             wanted,
                                             output_start,
                                             first_output_len);
        output_start += first_output_len;
        lcdm_auto_visual_append_range(&row,
                                      &row_count,
                                      wanted_row,
                                      wanted,
                                      output_start,
                                      output_end,
                                      2U);
      } else {
        /* A learned I can be completely open while its expected O has moved
         * to another I.  Preserve the trailing '-' so the record reads
         * "I003-" instead of looking like a truncated identifier. */
        lcdm_auto_visual_append_unit(&row,
                                     &row_count,
                                     wanted_row,
                                     wanted,
                                     "-",
                                     1U,
                                     1U);
      }
    }
    lcdm_auto_visual_row_emit(&row, &row_count, wanted_row, wanted);
  }

  return row_count;
}

static uint16_t lcdm_auto_visual_row_count(void)
{
  return lcdm_auto_collect_visual_rows(0U, 0);
}

static uint8_t lcdm_auto_get_visual_row(uint16_t visual_row, lcdm_auto_visual_row_t *row)
{
  if(visual_row == 0U || row == 0) {
    return 0U;
  }

  return (lcdm_auto_collect_visual_rows(visual_row, row) >= visual_row) ? 1U : 0U;
}

static uint8_t lcdm_auto_row_has_endpoint(const lcdm_auto_visual_row_t *row,
                                          char endpoint,
                                          uint16_t point)
{
  char token[8];

  if(row == 0 || point == 0U || point > LCDM_TOTAL_POINTS) {
    return 0U;
  }

  (void)snprintf(token, sizeof(token), "%c%03u", endpoint, (unsigned int)point);
  return (strstr(row->text, token) != 0) ? 1U : 0U;
}

/* The AUTO browse cache is organized as complete electrical groups, but a
 * group can occupy more than one physical row.  Keep the fault identity as
 * endpoint bitmaps instead of one "first bad" pair so every part of a merged
 * circuit can be redrawn together. */
static uint8_t lcdm_auto_fault_has_endpoint(char endpoint, uint16_t point)
{
  uint8_t word;
  uint8_t bit;

  if(point == 0U || point > LCDM_TOTAL_POINTS) {
    return 0U;
  }

  word = (uint8_t)((point - 1U) >> 5);
  bit = (uint8_t)((point - 1U) & 0x1FU);
  if(word >= FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS) {
    return 0U;
  }

  if(endpoint == 'I') {
    return ((lcdm_auto_fault_in_bits[word] & (1UL << bit)) != 0U) ? 1U : 0U;
  }
  if(endpoint == 'O') {
    return ((lcdm_auto_fault_out_bits[word] & (1UL << bit)) != 0U) ? 1U : 0U;
  }
  return 0U;
}

static uint8_t lcdm_auto_row_has_fault_endpoint(const lcdm_auto_visual_row_t *row,
                                                char endpoint)
{
  uint16_t point;

  if(row == 0) {
    return 0U;
  }

  for(point = 1U; point <= LCDM_TOTAL_POINTS; point++) {
    if(lcdm_auto_fault_has_endpoint(endpoint, point) != 0U &&
       lcdm_auto_row_has_endpoint(row, endpoint, point) != 0U) {
      return 1U;
    }
  }
  return 0U;
}

static uint8_t lcdm_auto_fault_has_any_endpoint(void)
{
  uint8_t word;

  for(word = 0U; word < FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS; word++) {
    if(lcdm_auto_fault_out_bits[word] != 0U ||
       lcdm_auto_fault_in_bits[word] != 0U) {
      return 1U;
    }
  }
  return 0U;
}

static uint8_t lcdm_auto_test_point_visible(uint8_t page, uint16_t point)
{
  uint16_t first;
  uint16_t last;
  uint16_t visual_count = lcdm_auto_visual_row_count();

  if(page == 0U) {
    page = 1U;
  }
  if(page > lcdm_auto_test_page_count()) {
    page = lcdm_auto_test_page_count();
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);
  if(last > visual_count) {
    last = visual_count;
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

static uint16_t lcdm_auto_count_line_outputs(const char *line)
{
  uint16_t count = 0U;
  const char *cursor;

  if(line == 0 || line[0] == '\0') {
    return 0U;
  }

  cursor = strchr(line, '-');
  if(cursor == 0) {
    return 0U;
  }
  cursor++;
  while(*cursor != '\0' && *cursor != ';') {
    if((*cursor == 'O') &&
       (cursor[1] >= '0' && cursor[1] <= '9') &&
       (cursor[2] >= '0' && cursor[2] <= '9') &&
       (cursor[3] >= '0' && cursor[3] <= '9')) {
      count++;
    }
    cursor++;
  }

  return count;
}

static uint16_t lcdm_auto_count_line_inputs(const char *line)
{
  uint16_t count = 0U;
  const char *cursor;
  const char *dash;

  if(line == 0 || line[0] == '\0') {
    return 0U;
  }

  dash = strchr(line, '-');
  if(dash == 0) {
    return 0U;
  }
  for(cursor = line; cursor < dash; cursor++) {
    if((*cursor == 'I') &&
       (cursor[1] >= '0' && cursor[1] <= '9') &&
       (cursor[2] >= '0' && cursor[2] <= '9') &&
       (cursor[3] >= '0' && cursor[3] <= '9')) {
      count++;
    }
  }

  return count;
}

static uint8_t lcdm_auto_test_page_count(void)
{
  uint8_t page_count;
  uint16_t visual_count = lcdm_auto_visual_row_count();

  if(visual_count == 0U) {
    return 1U;
  }

  page_count = (uint8_t)(((visual_count - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  if(page_count > LCDM_AUTO_LIST_MAX_PAGE) {
    page_count = LCDM_AUTO_LIST_MAX_PAGE;
  }
  return page_count;
}

static uint16_t lcdm_auto_test_set_line(uint16_t point, const char *line)
{
  uint16_t old_input_count;
  uint16_t old_output_count;

  if(point == 0U || point > LCDM_TOTAL_POINTS) {
    return 0U;
  }
  if(line == 0) {
    line = "";
  }

  old_input_count = lcdm_auto_count_line_inputs(lcdm_auto_line_cache[point]);
  old_output_count = lcdm_auto_count_line_outputs(lcdm_auto_line_cache[point]);
  if(lcdm_auto_input_count >= old_input_count) {
    lcdm_auto_input_count = (uint16_t)(lcdm_auto_input_count - old_input_count);
  } else {
    lcdm_auto_input_count = 0U;
  }
  if(lcdm_auto_point_count >= old_output_count) {
    lcdm_auto_point_count -= old_output_count;
  } else {
    lcdm_auto_point_count = 0UL;
  }

  (void)snprintf(lcdm_auto_line_cache[point],
                 sizeof(lcdm_auto_line_cache[point]),
                 "%s",
                 line);
  if(line[0] != '\0' && point > lcdm_auto_line_count) {
    lcdm_auto_line_count = point;
  }
  lcdm_auto_input_count = (uint16_t)(lcdm_auto_input_count + lcdm_auto_count_line_inputs(line));
  lcdm_auto_point_count += (uint32_t)lcdm_auto_count_line_outputs(line);
  return point;
}

static void lcdm_draw_auto_test_line(uint8_t page, uint16_t point)
{
  uint16_t x;
  uint16_t y;
  uint16_t text_x;
  uint16_t input_width;
  uint16_t separator_width;
  uint16_t output_text_width;
  uint16_t output_x;
  uint16_t input_bg_width;
  uint16_t output_width;
  uint16_t input_bg = LCDM_PALE_BLUE;
  uint16_t output_bg = LCDM_PALE_CYAN;
  uint16_t separator_bg = LCDM_WHITE;
  uint16_t input_fg = LCDM_NAVY;
  uint16_t output_fg = LCDM_NAVY;
  uint16_t separator_fg = LCDM_NAVY;
  uint8_t input_fault;
  uint8_t output_fault;
  lcdm_auto_visual_row_t row;
  char input_text[LCDM_AUTO_ROW_TEXT_MAX];
  char output_text[LCDM_AUTO_ROW_TEXT_MAX];

  lcdm_auto_test_point_rect(page, point, &x, &y);
  if(lcdm_auto_get_visual_row(point, &row) == 0U) {
    /* Empty slots are plain background, not an unused I/O column. */
    lcdm_raw_fill(x, y, LCDM_RESULT_W, (uint16_t)(LCDM_AUTO_LIST_ROW_H - 1U), LCDM_WHITE);
    lcdm_raw_fill(x,
                  (uint16_t)(y + LCDM_AUTO_LIST_ROW_H - 1U),
                  LCDM_RESULT_W,
                  1U,
                  LCDM_ROW_BG);
    return;
  }

  input_width = (uint16_t)(row.in_len * LCDM_AUTO_CHAR_W_ESTIMATE);
  separator_width = (uint16_t)(row.separator_len * LCDM_AUTO_CHAR_W_ESTIMATE);
  output_text_width = (uint16_t)(row.out_len * LCDM_AUTO_CHAR_W_ESTIMATE);
  text_x = (uint16_t)(x + LCDM_AUTO_LIST_TEXT_X);

  /* K1 result browsing keeps normal I/O colours for every verified record.
   * Only the exact endpoints of the live open/short invert to white on red;
   * when the blink phase is off this same row redraws in its normal colour. */
  input_fault = (lcdm_auto_fault_blink_on != 0U &&
                 lcdm_auto_row_has_fault_endpoint(&row, 'I') != 0U) ? 1U : 0U;
  output_fault = (lcdm_auto_fault_blink_on != 0U &&
                  lcdm_auto_row_has_fault_endpoint(&row, 'O') != 0U) ? 1U : 0U;
  if(input_fault != 0U) {
    input_bg = LCDM_RED;
    input_fg = LCDM_WHITE;
  }
  if(output_fault != 0U) {
    output_bg = LCDM_RED;
    output_fg = LCDM_WHITE;
  }
  if((input_fault != 0U) || (output_fault != 0U)) {
    separator_bg = LCDM_RED;
    separator_fg = LCDM_WHITE;
  }

  lcdm_raw_fill(x, y, LCDM_RESULT_W, (uint16_t)(LCDM_AUTO_LIST_ROW_H - 1U), LCDM_WHITE);
  if(row.in_len != 0U) {
    input_bg_width = (uint16_t)(LCDM_AUTO_LIST_TEXT_X + input_width + 2U);
    if(input_bg_width > LCDM_RESULT_W) {
      input_bg_width = LCDM_RESULT_W;
    }
    lcdm_raw_fill(x, y, input_bg_width, (uint16_t)(LCDM_AUTO_LIST_ROW_H - 1U), input_bg);
  }

  output_x = (uint16_t)(text_x + input_width + separator_width);
  if(row.in_len == 0U && row.separator_len == 0U) {
    output_x = x;
  }
  if(row.out_len != 0U && output_x < (uint16_t)(x + LCDM_RESULT_W)) {
    output_width = (uint16_t)(output_text_width + 2U);
    if(row.in_len == 0U && row.separator_len == 0U) {
      output_width = (uint16_t)(LCDM_AUTO_LIST_TEXT_X + output_width);
    }
    if(output_width > (uint16_t)((x + LCDM_RESULT_W) - output_x)) {
      output_width = (uint16_t)((x + LCDM_RESULT_W) - output_x);
    }
    lcdm_raw_fill(output_x,
                  y,
                  output_width,
                  (uint16_t)(LCDM_AUTO_LIST_ROW_H - 1U),
                  output_bg);
  }
  lcdm_raw_fill(x,
                (uint16_t)(y + LCDM_AUTO_LIST_ROW_H - 1U),
                LCDM_RESULT_W,
                1U,
                LCDM_WHITE);

  if(row.in_len != 0U) {
    memcpy(input_text, row.text, row.in_len);
    input_text[row.in_len] = '\0';
    lcdm_raw_xstr_full(text_x,
                       (uint16_t)(y + LCDM_AUTO_LIST_TEXT_Y),
                       (uint16_t)(input_width + 2U),
                       LCDM_AUTO_LIST_TEXT_H,
                       LCDM_FONT_SMALL,
                       input_fg,
                       input_bg,
                       0U,
                       input_text);
  }
  if(row.separator_len != 0U) {
    lcdm_raw_xstr_full((uint16_t)(text_x + input_width),
                       (uint16_t)(y + LCDM_AUTO_LIST_TEXT_Y),
                       separator_width,
                       LCDM_AUTO_LIST_TEXT_H,
                       LCDM_FONT_SMALL,
                       separator_fg,
                       separator_bg,
                       0U,
                       "-");
  }
  if(row.out_len != 0U) {
    memcpy(output_text,
           &row.text[(uint16_t)row.in_len + (uint16_t)row.separator_len],
           row.out_len);
    output_text[row.out_len] = '\0';
    lcdm_raw_xstr_full((uint16_t)(text_x + input_width + separator_width),
                       (uint16_t)(y + LCDM_AUTO_LIST_TEXT_Y),
                       (uint16_t)(output_text_width + 2U),
                       LCDM_AUTO_LIST_TEXT_H,
                       LCDM_FONT_SMALL,
                       output_fg,
                       output_bg,
                       0U,
                       output_text);
  }
}

static void lcdm_auto_cache_drawn_visual_row(uint8_t page, uint16_t point)
{
  uint16_t first;
  uint16_t index;
  lcdm_auto_visual_row_t row;

  if(page == 0U) {
    page = 1U;
  }
  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  if(point < first || point >= (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE)) {
    return;
  }
  index = (uint16_t)(point - first);

  if(lcdm_auto_get_visual_row(point, &row) != 0U) {
    (void)snprintf(lcdm_auto_drawn_row_cache[index],
                   sizeof(lcdm_auto_drawn_row_cache[index]),
                   "%s",
                   row.text);
  } else {
    lcdm_auto_drawn_row_cache[index][0] = '\0';
  }
}

static void lcdm_draw_auto_test_all(uint8_t page)
{
  uint16_t point;
  uint16_t first;
  uint16_t last;

  if(page == 0U) {
    page = 1U;
  }
  if(page > lcdm_auto_test_page_count()) {
    page = lcdm_auto_test_page_count();
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);

  for(point = first; point <= last; point++) {
    lcdm_draw_auto_test_line(page, point);
    lcdm_auto_cache_drawn_visual_row(page, point);
  }
  lcdm_auto_drawn_page = page;
}

/* Live AUTO scanning has an already-drawn fixed frame.  Compare the five
 * physical result rows with what is on that frame and write only changed
 * rows.  Header, STANDARD CABLE text, and K1-K4 are deliberately untouched. */
static void lcdm_draw_auto_test_changed(uint8_t page)
{
  uint16_t point;
  uint16_t first;
  uint16_t last;
  uint16_t index;
  lcdm_auto_visual_row_t row;
  const char *text;

  if(page == 0U) {
    page = 1U;
  }
  if(page > lcdm_auto_test_page_count()) {
    page = lcdm_auto_test_page_count();
  }

  if(lcdm_auto_drawn_page != page) {
    lcdm_draw_auto_test_all(page);
    return;
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);
  for(point = first; point <= last; point++) {
    index = (uint16_t)(point - first);
    text = "";
    if(lcdm_auto_get_visual_row(point, &row) != 0U) {
      text = row.text;
    }
    if(strcmp(lcdm_auto_drawn_row_cache[index], text) == 0) {
      continue;
    }

    lcdm_draw_auto_test_line(page, point);
    lcdm_auto_cache_drawn_visual_row(page, point);
  }
}

/* Redraw only physical rows containing the current fault endpoints.  This is
 * used for the result-page blink and deliberately never touches the header,
 * summary labels, footer, or K1-K4 band. */
static void lcdm_draw_auto_test_fault_rows(uint8_t page)
{
  uint16_t point;
  uint16_t first;
  uint16_t last;
  lcdm_auto_visual_row_t row;

  if(lcdm_auto_fault_type == 0U ||
     lcdm_auto_fault_has_any_endpoint() == 0U) {
    return;
  }
  if(page == 0U) {
    page = 1U;
  }
  if(page > lcdm_auto_test_page_count()) {
    page = lcdm_auto_test_page_count();
  }

  first = (uint16_t)(((page - 1U) * LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  last = (uint16_t)(first + LCDM_AUTO_LIST_PAGE_SIZE - 1U);
  for(point = first; point <= last; point++) {
    if(lcdm_auto_get_visual_row(point, &row) == 0U) {
      continue;
    }
    if(lcdm_auto_row_has_fault_endpoint(&row, 'I') == 0U &&
       lcdm_auto_row_has_fault_endpoint(&row, 'O') == 0U) {
      continue;
    }
    lcdm_draw_auto_test_line(page, point);
  }
}

static void lcdm_draw_auto_footer(uint8_t page, uint8_t done)
{
  char page_text[24];
  char input_text[20];
  char output_text[20];
  char cache_text[80];
  uint8_t page_count = lcdm_auto_test_page_count();
  uint16_t input_count;
  uint32_t output_count;

  if(page == 0U) {
    page = 1U;
  }
  if(page > page_count) {
    page = page_count;
  }

  (void)snprintf(page_text,
                 sizeof(page_text),
                 "%s P%02u/%02u",
                 (done != 0U) ? "RESULT" : "AUTO",
                 (unsigned int)page,
                 (unsigned int)page_count);
  input_count = (lcdm_auto_actual_counts_valid != 0U) ?
                  lcdm_auto_actual_input_count : lcdm_auto_input_count;
  output_count = (lcdm_auto_actual_counts_valid != 0U) ?
                   (uint32_t)lcdm_auto_actual_output_count : lcdm_auto_point_count;
  (void)snprintf(input_text, sizeof(input_text), "I-%03u", (unsigned int)input_count);
  (void)snprintf(output_text, sizeof(output_text), "O-%03lu", (unsigned long)output_count);

  (void)snprintf(cache_text, sizeof(cache_text), "%s|%s|%s", page_text, input_text, output_text);
  if(strcmp(lcdm_auto_footer_cache, cache_text) == 0) {
    return;
  }

  (void)snprintf(lcdm_auto_footer_cache, sizeof(lcdm_auto_footer_cache), "%s", cache_text);
  lcdm_raw_fill(0U, 188U, LCDM_W, 24U, LCDM_WHITE);
  lcdm_raw_xstr_full(0U, 188U, 200U, 24U, LCDM_FONT_TABLE, LCDM_BLUE, LCDM_WHITE, 1U, page_text);
  lcdm_raw_xstr_full(200U, 188U, 120U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, input_text);
  lcdm_raw_xstr_full(320U, 188U, 160U, 24U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_WHITE, 1U, output_text);
}

static void lcdm_prepare_auto_test_page(uint8_t page)
{
  uint8_t new_layout;

  if(page == 0U) {
    page = 1U;
  }
  if(page > lcdm_auto_test_page_count()) {
    page = lcdm_auto_test_page_count();
  }

  new_layout = (lcdm_layout_mode != 4U) ? 1U : 0U;
  if(new_layout != 0U || lcdm_auto_page_cache != page) {
    lcdm_auto_page_cache = page;
    lcdm_auto_footer_cache[0] = '\0';
    if(new_layout != 0U) {
      lcdm_reset_dynamic_effects();
      lcdm_layout_mode = 4U;
      lcdm_layout_top_cache[0] = '\0';
      lcdm_draw_auto_header("AUTO TESTING");
    }

    /* A PASS/NG summary occupies the entire middle work area.  On an actual
     * K1 record-page turn, clear that area once before the five result rows
     * are drawn.  This removes residual green/red above and below the rows
     * while deliberately preserving STANDARD CABLE and the K1-K4 strip. */
    lcdm_raw_fill(0U,
                  LCDM_AUTO_SUMMARY_LABEL_Y,
                  LCDM_W,
                  (uint16_t)(LCDM_KEY_Y0 - LCDM_AUTO_SUMMARY_LABEL_Y),
                  LCDM_WHITE);
    lcdm_auto_drawn_rows_invalidate();
  }
}

/* AUTO summary, AUTO record pages, and their K1 page turns intentionally
 * share layout mode 4.  Once the AUTO frame has been drawn, this function
 * clears only the compressed summary work area (y=33..213); STANDARD CABLE
 * and K1-K4 are never redrawn while PASS/NG and record pages cycle. */
static void lcdm_prepare_auto_summary_page(void)
{
  if(lcdm_layout_mode != 4U) {
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 4U;
    lcdm_layout_top_cache[0] = '\0';
    lcdm_draw_auto_header("AUTO TESTING");
  }

  lcdm_auto_page_cache = 0U;
  lcdm_auto_footer_cache[0] = '\0';
  lcdm_auto_drawn_rows_invalidate();
  lcdm_raw_fill(0U,
                LCDM_AUTO_SUMMARY_LABEL_Y,
                LCDM_W,
                (uint16_t)(LCDM_KEY_Y0 - LCDM_AUTO_SUMMARY_LABEL_Y),
                LCDM_WHITE);
  lcdm_raw_state_cache[0] = '\0';
  lcdm_raw_main_cache[0] = '\0';
  lcdm_raw_result_cache[0] = '\0';
  lcdm_raw_sub_cache[0] = '\0';
}

static void lcdm_learn_set_group_connection(uint16_t out_point, uint16_t in_point, uint16_t group_index)
{
  uint16_t color;

  if(out_point == 0U || out_point > LCDM_TOTAL_POINTS ||
     in_point == 0U || in_point > LCDM_TOTAL_POINTS) {
    return;
  }

  color = lcdm_learn_group_color(group_index);
  lcdm_learn_out_bg[out_point] = color;
  lcdm_learn_in_bg[in_point] = color;
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
  /* xstr uses solid-background mode, so it paints the inner cell itself.
   * Avoiding a separate fill halves the serial commands for a result grid. */
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_GREEN, 1U, "IN");
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
    top_bg = LCDM_GREEN;
    bottom_bg = LCDM_DARK_GREEN;
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
  (void)snprintf(text, sizeof(text), "%03u", (unsigned int)point);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 18U, LCDM_FONT_TABLE, top_fg, top_bg, 1U, text);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 21U), 38U, 18U, LCDM_FONT_TABLE, bottom_fg, bottom_bg, 1U, text);
}

static void lcdm_draw_table_grid_lines(void)
{
  uint8_t col;

  lcdm_raw_fill(0U, 38U, 1U, 166U, LCDM_BLACK);
  lcdm_raw_fill(479U, 38U, 1U, 166U, LCDM_BLACK);
  for(col = 1U; col < 12U; col++) {
    lcdm_raw_fill((uint16_t)((col * 40U) - 1U), 38U, 2U, 166U, LCDM_BLACK);
  }

  lcdm_raw_fill(0U, 38U, LCDM_W, 1U, LCDM_BLACK);
  lcdm_raw_fill(0U, 78U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 120U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 162U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 203U, LCDM_W, 1U, LCDM_BLACK);
}

static void lcdm_draw_table_all(uint8_t page, uint16_t active_point)
{
  uint16_t point;
  uint16_t first = (page == 1U) ? 1U : 48U;
  uint16_t last = (page == 1U) ? 47U : 94U;

  /* Keep the unopened grid at its original white base.  The inner bands are
   * updated independently, so a page build never flashes a black backdrop. */
  lcdm_raw_fill(0U, 38U, LCDM_W, 166U, LCDM_WHITE);
  lcdm_draw_table_grid_lines();
  for(point = first; point <= last; point++) {
    lcdm_draw_table_pair(page, point, active_point);
  }
}

static void lcdm_prepare_table_page(uint8_t page, uint16_t active_point)
{
  uint16_t old_active = lcdm_table_active_cache;
  uint8_t new_layout;

  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  if(active_point == 0U) {
    lcdm_table_ng_clear();
  }

  new_layout = (lcdm_layout_mode != 2U) ? 1U : 0U;
  if(new_layout != 0U || lcdm_table_page_cache != page) {
    lcdm_tjc_draw_batch_begin();
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 2U;
    lcdm_table_page_cache = page;
    lcdm_table_active_cache = active_point;
    if(new_layout != 0U) {
      lcdm_layout_top_cache[0] = '\0';
      lcdm_draw_common_header("");
    }
    /* A page turn redraws only the table (y=38..203); the key strip stays
     * intact so K1-K4 do not visibly refresh after PAGE is pressed. */
    lcdm_draw_table_all(page, active_point);
    lcdm_draw_table_legend(page);
    lcdm_tjc_draw_batch_end();
    return;
  }

  if(old_active != active_point) {
    lcdm_tjc_draw_batch_begin();
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
    lcdm_tjc_draw_batch_end();
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
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, LCDM_FONT_TABLE, LCDM_NAVY, LCDM_ROW_BG, 1U, "IN");
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

  lcdm_learn_table_point_rect(page, point, &x, &y);
  (void)snprintf(text, sizeof(text), "%03u", (unsigned int)point);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 1U), 38U, 16U, LCDM_FONT_TABLE, top_fg, top_bg, 1U, text);
  lcdm_raw_xstr_full((uint16_t)(x + 1U), (uint16_t)(y + 19U), 38U, 16U, LCDM_FONT_TABLE, bottom_fg, bottom_bg, 1U, text);
}

static void lcdm_draw_learn_table_grid_lines(void)
{
  uint8_t col;

  lcdm_raw_fill(0U, 36U, 1U, 150U, LCDM_BLACK);
  lcdm_raw_fill(479U, 36U, 1U, 150U, LCDM_BLACK);
  for(col = 1U; col < 12U; col++) {
    lcdm_raw_fill((uint16_t)((col * 40U) - 1U), 36U, 2U, 150U, LCDM_BLACK);
  }

  lcdm_raw_fill(0U, 36U, LCDM_W, 1U, LCDM_BLACK);
  lcdm_raw_fill(0U, 72U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 110U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 148U, LCDM_W, 2U, LCDM_BLACK);
  lcdm_raw_fill(0U, 185U, LCDM_W, 1U, LCDM_BLACK);
}

static void lcdm_draw_learn_table_all(uint8_t page, uint16_t active_point)
{
  uint16_t point;
  uint16_t first = (page == 1U) ? 1U : 48U;
  uint16_t last = (page == 1U) ? 47U : 94U;

  /* Preserve the original white base until each learned number is drawn. */
  lcdm_raw_fill(0U, 36U, LCDM_W, 150U, LCDM_WHITE);
  lcdm_draw_learn_table_grid_lines();
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
  uint8_t new_layout;

  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  if(active_point == 0U && done == 0U) {
    lcdm_learn_table_clear();
  }

  new_layout = (lcdm_layout_mode != 3U) ? 1U : 0U;
  if(new_layout != 0U || lcdm_table_page_cache != page) {
    lcdm_tjc_draw_batch_begin();
    lcdm_reset_dynamic_effects();
    lcdm_layout_mode = 3U;
    lcdm_table_page_cache = page;
    lcdm_table_active_cache = active_point;
    if(new_layout != 0U) {
      lcdm_layout_top_cache[0] = '\0';
      lcdm_draw_common_header("");
    }
    /* As with self-test, page turns repaint the learning grid/footer only;
     * the complete K1-K4 strip is retained. */
    lcdm_draw_learn_table_all(page, active_point);
    lcdm_draw_learn_table_legend(page);
    lcdm_learn_footer_scan_cache[0] = '\0';
    lcdm_learn_footer_pairs_cache[0] = '\0';
    lcdm_learn_footer_points_cache[0] = '\0';
    lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
    lcdm_tjc_draw_batch_end();
    return;
  }

  if(old_active != active_point) {
    lcdm_tjc_draw_batch_begin();
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
    lcdm_draw_learn_footer(scan_point, pair_count, point_count, done);
    lcdm_tjc_draw_batch_end();
    return;
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
    } else if(strcmp(main_text, "NG") == 0U) {
      lcdm_draw_result_ng_body(result_text);
    } else {
      lcdm_draw_split_body(24U,
                           84U,
                           216U,
                           216U,
                           82U,
                           LCDM_FONT_RESULT,
                           LCDM_FONT_POINT,
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
                               LCDM_FONT_STATUS,
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
                           LCDM_FONT_STATUS,
                           LCDM_FONT_STATUS,
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
                               LCDM_FONT_STATUS,
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
                                 LCDM_FONT_STATUS,
                                 LCDM_NAVY,
                                 LCDM_WHITE,
                                 1U,
                                 main_text);
    } else {
      lcdm_raw_fill(24U, 84U, 432U, 82U, LCDM_WHITE);
    }
  } else if(strcmp(status_text, "RESET") == 0U) {
    lcdm_raw_write_status_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache),
                                 24U, LCDM_STATUS_Y, 432U, LCDM_STATUS_H,
                                 status_color, LCDM_ROW_BG, 1U, status_text);
    lcdm_draw_body_text(84U, 82U, " ", LCDM_BLUE, LCDM_WHITE);
  } else if(strcmp(status_text, "LEARNING") == 0U) {
    /* Learning now scans continuously without per-row display updates.  A
     * marquee begins off-screen, so it cannot serve as the immediate scan
     * state.  Draw the complete title synchronously before acquisition. */
    lcdm_idle_banner_stop();
    lcdm_raw_write_cached_font(lcdm_raw_state_cache,
                               sizeof(lcdm_raw_state_cache),
                               24U,
                               LCDM_STATUS_Y,
                               432U,
                               LCDM_STATUS_H,
                               LCDM_FONT_STATUS,
                               LCDM_BLUE,
                               LCDM_ROW_BG,
                               1U,
                               "LEARNING");
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

void first_gen_display_show_auto_test_pass_summary(const char *total_text)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_prepare_auto_summary_page();
  /* Keep the already drawn AUTO top/bottom frame intact; only this middle
   * RESULT/DETAILS area changes when PASS is shown. */
  lcdm_draw_auto_summary_top_labels("RESULT", "DETAILS");
  lcdm_draw_auto_result_pass_body(total_text);
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache),
                        24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U,
                        LCDM_GRAY, LCDM_WHITE, 1U, "");
}

void first_gen_display_show_auto_test_ng_summary(const char *fault_text)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_prepare_auto_summary_page();
  /* The NG table uses the same fixed AUTO frame and redraws only its middle
   * split header/body. */
  lcdm_draw_auto_summary_top_labels("RESULT", "DETAILS");
  lcdm_draw_auto_result_ng_body(fault_text);
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache),
                        24U, (uint16_t)(LCDM_SUB_Y + 3U), 432U, 22U,
                        LCDM_GRAY, LCDM_WHITE, 1U, "");
}

void first_gen_display_update_auto_test_ng_detail(const char *fault_text)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  /* Called for the NG blink.  Its fast path touches only the middle red
   * body and never waits for an LCDM acknowledgement, so K1-K4 stay intact
   * and the next IO row can be scanned without a display-timeout pause. */
  lcdm_draw_auto_result_ng_blink_detail(fault_text);
}

void first_gen_display_show_auto_table_page(uint8_t page, uint16_t active_point)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_prepare_table_page(page, active_point);
}

void first_gen_display_set_self_test_matrix(const uint32_t *matrix,
                                            uint16_t point_count,
                                            uint8_t words_per_row)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;

  if(display_is_lcdm == 0U || matrix == 0 || words_per_row == 0U) {
    return;
  }
  if(point_count > LCDM_TOTAL_POINTS) {
    point_count = LCDM_TOTAL_POINTS;
  }

  /* Build the result bitmap in RAM first.  Calling the old per-pair drawing
   * entry point here would make the physical IO scan wait for hundreds of LCDM
   * acknowledgements. */
  lcdm_table_ng_clear();
  for(out_point = 1U; out_point <= point_count; out_point++) {
    for(in_point = 1U; in_point <= point_count; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if(word >= words_per_row) {
        continue;
      }
      if((matrix[((uint32_t)(out_point - 1U) * words_per_row) + word] & (1UL << bit)) != 0U) {
        lcdm_table_ng_set_out(out_point);
        lcdm_table_ng_set_in(in_point);
      }
    }
  }
}

void first_gen_display_set_k1_page_hint(uint8_t enabled)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_set_k1_page_hint(enabled);
}

void first_gen_display_set_auto_test_blink(uint8_t enabled)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_set_auto_test_blink(enabled);
}

void first_gen_display_auto_test_blink_step(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_auto_test_blink_step();
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
  lcdm_tjc_draw_batch_begin();
  lcdm_prepare_table_page(page, point);
  if(lcdm_table_point_visible(page, point) != 0U) {
    lcdm_draw_table_pair(page, point, point);
  }
  lcdm_tjc_draw_batch_end();
}

void first_gen_display_show_auto_table_ng_pair(uint8_t page, uint16_t out_point, uint16_t in_point)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_table_ng_set_out(out_point);
  lcdm_table_ng_set_in(in_point);
  lcdm_tjc_draw_batch_begin();
  lcdm_prepare_table_page(page, in_point);
  if(lcdm_table_point_visible(page, in_point) != 0U) {
    lcdm_draw_table_pair(page, in_point, in_point);
  }
  if(out_point != in_point && lcdm_table_point_visible(page, out_point) != 0U) {
    lcdm_draw_table_pair(page, out_point, in_point);
  }
  lcdm_tjc_draw_batch_end();
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

void first_gen_display_set_learn_table_group_connection(uint16_t out_point,
                                                        uint16_t in_point,
                                                        uint16_t group_index)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_learn_set_group_connection(out_point, in_point, group_index);
}

void first_gen_display_apply_learn_table_groups(const uint16_t out_groups[],
                                                const uint16_t in_groups[],
                                                uint16_t active_point)
{
  uint16_t point;
  uint16_t new_in_bg;
  uint16_t new_out_bg;
  uint8_t changed;
  uint8_t redraw_started = 0U;

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
      if(redraw_started == 0U) {
        lcdm_tjc_draw_batch_begin();
        redraw_started = 1U;
      }
      lcdm_draw_learn_table_pair(lcdm_table_page_cache, point, active_point);
    }
  }

  if(redraw_started != 0U) {
    lcdm_tjc_draw_batch_end();
  }
}

void first_gen_display_clear_auto_test_lines(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_auto_test_clear();
  lcdm_layout_mode = 0U;
  lcdm_auto_page_cache = 0U;
}

void first_gen_display_reset_auto_test_result_lines(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  /* Keep the fixed frame, current page, footer cache, and on-screen row
   * shadow intact.  The next live redraw compares only the middle result
   * rows, so STANDARD CABLE and K1-K4 cannot flash during scanning. */
  lcdm_auto_test_clear_result_lines();
}

void first_gen_display_add_auto_test_result_line(uint16_t row_index, const char *line)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  (void)lcdm_auto_test_set_line(row_index, line);
}

void first_gen_display_set_auto_test_result_actual_counts(uint16_t input_count,
                                                          uint16_t output_count)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  lcdm_auto_actual_input_count = input_count;
  lcdm_auto_actual_output_count = output_count;
  lcdm_auto_actual_counts_valid = 1U;
  /* The result rows may be unchanged while only the measured status count
   * changed, so do not reuse an old footer cache. */
  lcdm_auto_footer_cache[0] = '\0';
}

uint8_t first_gen_display_auto_test_page_count(void)
{
  if(display_is_lcdm == 0U) {
    return 1U;
  }

  return lcdm_auto_test_page_count();
}

void first_gen_display_set_auto_test_result_fault(uint16_t out_point,
                                                  uint16_t in_point,
                                                  uint8_t problem_type)
{
  uint32_t out_bits[FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS] = {0U};
  uint32_t in_bits[FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS] = {0U};
  uint8_t out_word;
  uint8_t in_word;

  if(out_point == 0U || out_point > LCDM_TOTAL_POINTS ||
     in_point == 0U || in_point > LCDM_TOTAL_POINTS) {
    return;
  }

  out_word = (uint8_t)((out_point - 1U) >> 5);
  in_word = (uint8_t)((in_point - 1U) >> 5);
  out_bits[out_word] = (1UL << ((out_point - 1U) & 0x1FU));
  in_bits[in_word] = (1UL << ((in_point - 1U) & 0x1FU));
  first_gen_display_set_auto_test_result_fault_group(out_bits,
                                                      in_bits,
                                                      FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS,
                                                      problem_type);
}

void first_gen_display_set_auto_test_result_fault_group(const uint32_t out_bits[],
                                                        const uint32_t in_bits[],
                                                        uint8_t word_count,
                                                        uint8_t problem_type)
{
  uint8_t word;
  uint8_t changed;
  uint32_t next_out_bits;
  uint32_t next_in_bits;

  if(display_is_lcdm == 0U) {
    return;
  }

  changed = (lcdm_auto_fault_type != problem_type) ? 1U : 0U;
  for(word = 0U; word < FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS; word++) {
    next_out_bits = (out_bits != 0 && word < word_count) ? out_bits[word] : 0U;
    next_in_bits = (in_bits != 0 && word < word_count) ? in_bits[word] : 0U;
    if(lcdm_auto_fault_out_bits[word] != next_out_bits ||
       lcdm_auto_fault_in_bits[word] != next_in_bits) {
      changed = 1U;
    }
    lcdm_auto_fault_out_bits[word] = next_out_bits;
    lcdm_auto_fault_in_bits[word] = next_in_bits;
  }

  if(problem_type == 0U || lcdm_auto_fault_has_any_endpoint() == 0U) {
    lcdm_auto_fault_type = 0U;
    lcdm_auto_fault_blink_on = 0U;
  } else {
    lcdm_auto_fault_type = problem_type;
    lcdm_auto_fault_blink_on = 1U;
  }

  /* A wiring change may occur while K1 is already showing a record page.
   * Redraw those five middle rows once so removed old marks are cleared and
   * every endpoint in the new group becomes visible immediately. */
  if(changed != 0U && lcdm_layout_mode == 4U &&
     lcdm_auto_page_cache != 0U &&
     lcdm_auto_drawn_page == lcdm_auto_page_cache) {
    lcdm_draw_auto_test_all(lcdm_auto_page_cache);
  }
}

void first_gen_display_clear_auto_test_result_fault(void)
{
  uint8_t word;

  if(display_is_lcdm == 0U) {
    return;
  }

  for(word = 0U; word < FIRST_GEN_DISPLAY_AUTO_FAULT_WORDS; word++) {
    lcdm_auto_fault_out_bits[word] = 0U;
    lcdm_auto_fault_in_bits[word] = 0U;
  }
  lcdm_auto_fault_type = 0U;
  lcdm_auto_fault_blink_on = 0U;
}

void first_gen_display_set_auto_test_result_fault_blink(uint8_t visible)
{
  if(display_is_lcdm == 0U ||
     lcdm_auto_fault_type == 0U ||
     lcdm_auto_fault_has_any_endpoint() == 0U) {
    return;
  }

  visible = (visible != 0U) ? 1U : 0U;
  if(lcdm_auto_fault_blink_on == visible) {
    return;
  }
  lcdm_auto_fault_blink_on = visible;

  /* A summary page has no cached record page (page cache is zero), so retain
   * only the state here.  The next K1 page draw will use it immediately. */
  if(lcdm_layout_mode == 4U &&
     lcdm_auto_page_cache != 0U &&
     lcdm_auto_drawn_page == lcdm_auto_page_cache) {
    lcdm_draw_auto_test_fault_rows(lcdm_auto_page_cache);
  }
}

void first_gen_display_show_auto_test_result_page(uint8_t page, uint8_t done)
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

  lcdm_prepare_auto_test_page(page);
  if(done != 0U) {
    /* Result browsing can paint a complete middle page after the scan has
     * stopped. */
    lcdm_draw_auto_test_all(page);
  } else {
    lcdm_draw_auto_test_changed(page);
  }
  lcdm_draw_auto_footer(page, done);
  lcdm_set_k1_page_hint(done);
}

uint8_t first_gen_display_show_auto_test_result_row(uint16_t row_index, uint8_t done)
{
  uint8_t page;
  uint8_t page_count;

  if(display_is_lcdm == 0U || row_index == 0U) {
    return 1U;
  }

  page_count = lcdm_auto_test_page_count();
  page = (uint8_t)(((row_index - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  if(page > page_count) {
    page = page_count;
  }

  if(lcdm_layout_mode != 4U || lcdm_auto_page_cache != page) {
    lcdm_prepare_auto_test_page(page);
    lcdm_draw_auto_test_all(page);
  } else if(lcdm_auto_test_point_visible(page, row_index) != 0U) {
    lcdm_draw_auto_test_line(page, row_index);
    lcdm_auto_cache_drawn_visual_row(page, row_index);
  }
  lcdm_draw_auto_footer(page, done);
  lcdm_set_k1_page_hint(done);
  return page;
}

void first_gen_display_show_auto_test_line(uint16_t out_point, const char *line, uint8_t done)
{
  uint8_t page;

  if(display_is_lcdm == 0U) {
    return;
  }

  if(line != 0 && line[0] != '\0') {
    first_gen_display_add_auto_test_result_line(out_point, line);
  }

  if(out_point == 0U) {
    page = 1U;
  } else {
    page = (uint8_t)(((out_point - 1U) / LCDM_AUTO_LIST_PAGE_SIZE) + 1U);
  }
  first_gen_display_show_auto_test_result_page(page, done);
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

  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_tjc_send_cmd("cls 65535");
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 30U, LCDM_NAVY);
  lcdm_raw_xstr(8U, 3U, 464U, 24U, LCDM_FONT_SMALL, LCDM_WHITE, LCDM_NAVY, 1U, "LCDM LARGE FONT SELECT");

  for(font = 0U; font < LCDM_FONT_COUNT; font++) {
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
  lcdm_k1_page_hint = 0U;
  lcdm_auto_test_blink_active = 0U;
  lcdm_auto_test_blink_on = 1U;
  lcdm_auto_test_blink_steps = 0U;
  lcdm_hall_input_active = 0U;
  lcdm_hall_input_drawn = 0U;
  lcdm_wifi_connected = 0U;
  lcdm_wifi_indicator_drawn = 0U;
  lcdm_tjc_init();
  lcdm_tjc_force_baudrate(LCDM_TJC_BAUDRATE);
  delay_ms(150U);
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("dim=100");
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_tjc_set_command_ack(1U);
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
#if LCDM_TOUCH_KEYS_ONLY
      /* Raw coordinate events in the bottom key strip are the only active
       * controls for now.  Ignore every component-style touch elsewhere. */
      continue;
#else
      lcdm_request_recover();
      continue;
#endif
    } else if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      key = lcdm_coord_to_key(event.x, event.y);
      g_first_gen_lcdm_last_x = event.x;
      g_first_gen_lcdm_last_y = event.y;
      if(event.touch_event == 0U) {
        /* A release may be reported just outside the key after a finger
         * slides away.  It must still clear an already-latched bottom key. */
        lcdm_release_key(key);
        return FIRST_GEN_KEY_NONE;
      }
      if(key != FIRST_GEN_KEY_NONE) {
        return lcdm_latch_key(key);
      }
      /* All presses above K1-K4 are intentionally ignored: no page redraw,
       * no debug text, and no firmware action. */
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

void first_gen_display_leave_maintenance(void)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  /* The settings overlay draws directly through lcdm_tjc, so invalidate the
   * normal-page cache rather than allowing a same-title fast path to retain
   * its field rows or unlocked touch state. */
  lcdm_current_key = FIRST_GEN_KEY_NONE;
  lcdm_key_hold_reads = 0U;
  lcdm_key_waits_release = 0U;
  g_first_gen_lcdm_last_key = FIRST_GEN_KEY_NONE;
  lcdm_layout_mode = 0U;
  lcdm_layout_top_cache[0] = '\0';
  lcdm_reset_dynamic_effects();
  lcdm_tjc_send_cmd("tsw 255,0");
  lcdm_tjc_send_cmd("sendxy=1");
  /* Settings deliberately disables blocking draw acknowledgements while its
   * keyboard is repainting.  Restore the normal verified LCDM rendering mode
   * before the tester grid/page code resumes. */
  lcdm_tjc_set_command_ack(1U);
}

void first_gen_display_set_hall_input(uint8_t active)
{
  active = (active != 0U) ? 1U : 0U;
  if(lcdm_hall_input_active != active) {
    lcdm_hall_input_active = active;
    lcdm_hall_input_drawn = 0U;
  }

  if(display_is_lcdm != 0U && lcdm_hall_input_drawn == 0U) {
    lcdm_draw_hall_indicator(0U);
  }
}

void first_gen_display_set_wifi_connected(uint8_t connected)
{
  connected = (connected != 0U) ? 1U : 0U;
  if(lcdm_wifi_connected != connected) {
    lcdm_wifi_connected = connected;
    lcdm_wifi_indicator_drawn = 0U;
  }

  if(display_is_lcdm != 0U && lcdm_wifi_indicator_drawn == 0U) {
    lcdm_draw_wifi_indicator(0U);
  }
}

void first_gen_display_show_print_progress(uint8_t state)
{
  if(display_is_lcdm == 0U) {
    return;
  }

  if(state != FIRST_GEN_PRINT_DISPLAY_COMPLETE &&
     state != FIRST_GEN_PRINT_DISPLAY_ERROR) {
    state = FIRST_GEN_PRINT_DISPLAY_START;
  }

  lcdm_tjc_draw_batch_begin();
  lcdm_reset_dynamic_effects();
  lcdm_layout_mode = 5U;
  lcdm_draw_auto_header("PRINT");
  lcdm_draw_print_progress_body(state);
  lcdm_tjc_draw_batch_end();
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
