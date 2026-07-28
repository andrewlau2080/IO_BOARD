#ifndef FIRST_GEN_DISPLAY_H
#define FIRST_GEN_DISPLAY_H

#include <stdint.h>

#define FIRST_GEN_DISPLAY_DIGITS 6U
#define FIRST_GEN_DISPLAY_AUTO_RESULT_PAGE_ROWS 5U

#define FIRST_GEN_KEY_NONE       0xFFU
#define FIRST_GEN_KEY_SET        0xF3U
#define FIRST_GEN_KEY_CLEAR      0xF4U
#define FIRST_GEN_KEY_PLUS       0xF5U
#define FIRST_GEN_KEY_MINUS      0xF0U

#define FIRST_GEN_DISPLAY_COLOR_BLACK       0U
#define FIRST_GEN_DISPLAY_COLOR_BLUE        31U
#define FIRST_GEN_DISPLAY_COLOR_RED         63488U
#define FIRST_GEN_DISPLAY_COLOR_GREEN       2016U
#define FIRST_GEN_DISPLAY_COLOR_WHITE       65535U
#define FIRST_GEN_DISPLAY_COLOR_ROW_BG      61374U
#define FIRST_GEN_DISPLAY_COLOR_PALE_BLUE   50719U
#define FIRST_GEN_DISPLAY_COLOR_PALE_CYAN   49151U
#define FIRST_GEN_DISPLAY_COLOR_PALE_PINK   61503U
#define FIRST_GEN_DISPLAY_COLOR_DARK_PINK   46521U

extern volatile uint32_t g_first_gen_lcdm_touch_count;
extern volatile uint32_t g_first_gen_lcdm_key_press_count;
extern volatile uint32_t g_first_gen_lcdm_key_release_count;
extern volatile uint8_t g_first_gen_lcdm_last_event_type;
extern volatile uint8_t g_first_gen_lcdm_last_touch_event;
extern volatile uint8_t g_first_gen_lcdm_last_key;
extern volatile uint16_t g_first_gen_lcdm_last_x;
extern volatile uint16_t g_first_gen_lcdm_last_y;

void first_gen_display_init(void);
void first_gen_display_clear(void);
uint8_t first_gen_display_is_lcdm(void);
uint8_t first_gen_display_key_read_raw(void);
void first_gen_display_effect_step(void);
void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS]);
void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS]);
void first_gen_display_write_learn_summary(uint16_t out_count, uint16_t in_count, uint32_t total);
void first_gen_display_show_page(const char *top_right,
                                 const char *status_text,
                                 const char *main_text,
                                 const char *result_text,
                                 const char *sub_text,
                                 uint16_t status_color,
                                 uint16_t result_bg,
                                 uint16_t result_fg);
void first_gen_display_show_auto_table_page(uint8_t page, uint16_t active_point);
void first_gen_display_show_auto_table_ng(uint8_t page, uint16_t point);
void first_gen_display_show_auto_table_ng_pair(uint8_t page, uint16_t out_point, uint16_t in_point);
/* Load all self-test NG marks into the display cache without sending per-pair
 * raster commands.  The caller then renders the completed result page once. */
void first_gen_display_set_self_test_matrix(const uint32_t *matrix,
                                            uint16_t point_count,
                                            uint8_t words_per_row);
void first_gen_display_set_k1_page_hint(uint8_t enabled);
void first_gen_display_set_auto_test_blink(uint8_t enabled);
void first_gen_display_auto_test_blink_step(void);
void first_gen_display_show_learn_table_page(uint8_t page,
                                             uint16_t active_point,
                                             uint16_t scan_point,
                                             uint16_t pair_count,
                                             uint32_t point_count,
                                             uint8_t done);
void first_gen_display_clear_learn_table_groups(void);
void first_gen_display_set_learn_table_group_connection(uint16_t out_point,
                                                        uint16_t in_point,
                                                        uint16_t group_index);
void first_gen_display_apply_learn_table_groups(const uint16_t out_groups[],
                                                const uint16_t in_groups[],
                                                uint16_t active_point);
void first_gen_display_clear_auto_test_lines(void);
/* Clear cached AUTO rows while retaining the current LCDM page/key strip.
 * Used when a newly scanned edge merges two electrical circuits. */
void first_gen_display_reset_auto_test_result_lines(void);
/* Cache one completed AUTO result row.  Only connected I rows are appended,
 * so row_index is compact and the footer I count is the true connected count. */
void first_gen_display_add_auto_test_result_line(uint16_t row_index, const char *line);
uint8_t first_gen_display_auto_test_page_count(void);
void first_gen_display_show_auto_test_result_page(uint8_t page, uint8_t done);
/* Update one already-cached AUTO row immediately and return its displayed
 * page.  Used while the matrix is still being scanned. */
uint8_t first_gen_display_show_auto_test_result_row(uint16_t row_index, uint8_t done);
/* Compatibility entry point for older callers. */
void first_gen_display_show_auto_test_line(uint16_t out_point, const char *line, uint8_t done);

#endif
