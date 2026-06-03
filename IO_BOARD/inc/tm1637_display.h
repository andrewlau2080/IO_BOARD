#ifndef TM1637_DISPLAY_H
#define TM1637_DISPLAY_H

#include <stdint.h>

#define TM1637_DIGITS 6U

#define TM1637_KEY_NONE       0xFFU
#define TM1637_KEY_SET        0xF3U
#define TM1637_KEY_CLEAR      0xF4U
#define TM1637_KEY_PLUS       0xF5U
#define TM1637_KEY_MINUS      0xF0U

void tm1637_display_init(void);
void tm1637_display_set_brightness(uint8_t brightness, uint8_t enabled);
void tm1637_display_clear(void);
uint8_t tm1637_key_read_raw(void);
void tm1637_display_write_raw(uint8_t position, uint8_t segments);
void tm1637_display_write_raw6(const uint8_t segments[TM1637_DIGITS]);
void tm1637_display_write_text6(const char text[TM1637_DIGITS]);
void tm1637_display_write_u32_6(uint32_t value);
uint8_t tm1637_display_encode_char(char ch);

#endif
