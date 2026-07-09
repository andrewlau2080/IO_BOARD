#ifndef FIRST_GEN_DISPLAY_H
#define FIRST_GEN_DISPLAY_H

#include <stdint.h>

#define FIRST_GEN_DISPLAY_DIGITS 6U

#define FIRST_GEN_KEY_NONE       0xFFU
#define FIRST_GEN_KEY_SET        0xF3U
#define FIRST_GEN_KEY_CLEAR      0xF4U
#define FIRST_GEN_KEY_PLUS       0xF5U
#define FIRST_GEN_KEY_MINUS      0xF0U

void first_gen_display_init(void);
void first_gen_display_clear(void);
uint8_t first_gen_display_key_read_raw(void);
void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS]);
void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS]);

#endif
