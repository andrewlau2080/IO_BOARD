#ifndef IO_BOARD_H
#define IO_BOARD_H

#include "at32f45x.h"

typedef enum {
  IO_MUX_IN_A = 0,
  IO_MUX_IN_B,
  IO_MUX_OUT_A,
  IO_MUX_OUT_B
} io_mux_bank_t;

typedef enum {
  IO_BTN_CANCEL = 0,
  IO_BTN_ENTER,
  IO_BTN_RIGHT,
  IO_BTN_LEFT,
  IO_BTN_DOWN,
  IO_BTN_UP
} io_button_t;

void io_board_init(void);
void io_debug_write(uint8_t level);
void io_debug_toggle(void);
void io_mux_disable_all(void);
void io_mux_select(io_mux_bank_t bank, uint8_t index);
uint8_t io_button_read(io_button_t button);

#endif
