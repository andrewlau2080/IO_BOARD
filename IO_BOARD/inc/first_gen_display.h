#ifndef FIRST_GEN_DISPLAY_H
#define FIRST_GEN_DISPLAY_H

#include <stdint.h>

#define FIRST_GEN_DISPLAY_DIGITS 6U

#define FIRST_GEN_KEY_NONE       0xFFU
#define FIRST_GEN_KEY_SET        0xF3U
#define FIRST_GEN_KEY_CLEAR      0xF4U
#define FIRST_GEN_KEY_PLUS       0xF5U
#define FIRST_GEN_KEY_MINUS      0xF0U

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
uint8_t first_gen_display_key_read_raw(void);
void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS]);
void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS]);

#endif
