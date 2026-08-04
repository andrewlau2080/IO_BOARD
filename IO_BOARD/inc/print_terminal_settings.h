#ifndef PRINT_TERMINAL_SETTINGS_H
#define PRINT_TERMINAL_SETTINGS_H

#include "print_job_model.h"

#include <stdint.h>

typedef enum {
  PRINT_TERMINAL_SETTINGS_LABEL = 0,
  PRINT_TERMINAL_SETTINGS_COMM
} print_terminal_settings_page_t;

void print_terminal_settings_init(void);
uint8_t print_terminal_settings_begin(print_terminal_settings_page_t page,
                                      const print_job_t *current_job);
/* Same as print_terminal_settings_begin(), but called from a touch press.
 * The first press is latched until the panel reports release so a TFT with
 * sendxy=1 cannot replay the entry touch and redraw the page repeatedly. */
uint8_t print_terminal_settings_begin_from_touch(print_terminal_settings_page_t page,
                                                 const print_job_t *current_job);
uint8_t print_terminal_settings_is_active(void);
void print_terminal_settings_service(void);
/* One-shot notifications consumed by print_terminal_service(). */
uint8_t print_terminal_settings_take_return(void);
uint8_t print_terminal_settings_take_job(print_job_t *out_job);
void print_terminal_settings_set_network_status(const char *text,
                                                uint16_t color);

#endif
