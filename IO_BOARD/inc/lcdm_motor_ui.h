#ifndef LCDM_MOTOR_UI_H
#define LCDM_MOTOR_UI_H

#include <stdint.h>

extern volatile uint8_t g_lcdm_motor_page;
extern volatile uint8_t g_lcdm_motor_selected;
extern volatile uint16_t g_lcdm_motor_touch_count;
extern volatile uint16_t g_lcdm_motor_last_x;
extern volatile uint16_t g_lcdm_motor_last_y;
extern volatile uint8_t g_lcdm_motor_last_event;
extern volatile uint8_t g_lcdm_motor_active_key;
extern volatile uint16_t g_lcdm_motor_key_press_count;
extern volatile uint16_t g_lcdm_motor_key_release_count;
extern volatile uint32_t g_lcdm_motor_refresh_count;

void lcdm_motor_ui_init(void);
void lcdm_motor_ui_service(void);

#endif
