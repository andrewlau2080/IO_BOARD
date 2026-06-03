/*
 * Normalized NEC capture from 2 matching frames. ADDR=0x79 ~ADDR=0x0A CMD=0x20 ~CMD=0x50
 * Unit: microseconds. start_level 0 means IR carrier mark first for the current transmitter.
 */
#include <stdint.h>

static const uint8_t archived_ir_normalized_start_level = 0;
static const uint16_t archived_ir_normalized_count = 67;
static const uint16_t archived_ir_normalized_signal_us[67] = {
  9000, 4500, 560, 1690, 560, 560, 560, 560, 560, 1690, 560, 1690,
  560, 1690, 560, 1690, 560, 560, 560, 560, 560, 1690, 560, 560,
  560, 1690, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560,
  560, 560, 560, 560, 560, 560, 560, 560, 560, 1690, 560, 560,
  560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560, 1690,
  560, 560, 560, 1690, 560, 560, 560
};
