#ifndef FIRST_GEN_DISPLAY_H
#define FIRST_GEN_DISPLAY_H

#include <stdint.h>

#define FIRST_GEN_DISPLAY_DIGITS 6U

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

#endif
