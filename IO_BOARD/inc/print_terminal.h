#ifndef PRINT_TERMINAL_H
#define PRINT_TERMINAL_H

#include <stdint.h>

typedef enum {
  PRINT_TERMINAL_STATE_EDIT = 0,
  PRINT_TERMINAL_STATE_PREVIEW,
  PRINT_TERMINAL_STATE_PRINTED,
  PRINT_TERMINAL_STATE_ERROR,
  PRINT_TERMINAL_STATE_PRINTING,
  PRINT_TERMINAL_STATE_NETWORK_ERROR
} print_terminal_state_t;

/* No physical printer is connected during this integration stage.  The
 * terminal holds PRINTING for five seconds, then completes the request.  A
 * future RS232 backend can override these at compile time and replace the
 * completion hook without changing the WiFi/Hall protocol. */
#ifndef PRINT_TERMINAL_SIMULATE_PRINT
#define PRINT_TERMINAL_SIMULATE_PRINT 1U
#endif
#ifndef PRINT_TERMINAL_SIMULATED_PRINT_MS
#define PRINT_TERMINAL_SIMULATED_PRINT_MS 5000UL
#endif

extern volatile uint32_t g_print_terminal_event_count;
extern volatile uint32_t g_print_terminal_refresh_count;
extern volatile uint32_t g_print_terminal_print_count;
extern volatile uint32_t g_print_terminal_ir_request_count;
extern volatile uint32_t g_print_terminal_ir_print_count;
extern volatile uint8_t g_print_terminal_state;

void print_terminal_init(void);
void print_terminal_service(void);

#endif
