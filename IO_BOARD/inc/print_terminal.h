#ifndef PRINT_TERMINAL_H
#define PRINT_TERMINAL_H

#include <stdint.h>

typedef enum {
  PRINT_TERMINAL_STATE_EDIT = 0,
  PRINT_TERMINAL_STATE_PREVIEW,
  PRINT_TERMINAL_STATE_PRINTED,
  PRINT_TERMINAL_STATE_ERROR
} print_terminal_state_t;

extern volatile uint32_t g_print_terminal_event_count;
extern volatile uint32_t g_print_terminal_refresh_count;
extern volatile uint32_t g_print_terminal_print_count;
extern volatile uint32_t g_print_terminal_ir_request_count;
extern volatile uint32_t g_print_terminal_ir_print_count;
extern volatile uint8_t g_print_terminal_state;

void print_terminal_init(void);
void print_terminal_service(void);

#endif
