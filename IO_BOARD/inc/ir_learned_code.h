#ifndef IR_LEARNED_CODE_H
#define IR_LEARNED_CODE_H

#include "ir_remote.h"

#include <stdint.h>

extern const uint8_t g_ir_learned_code_available;
extern const uint8_t g_ir_learned_start_level;
extern const uint16_t g_ir_learned_count;
extern const uint16_t g_ir_learned_signal_us[IR_CAPTURE_MAX_EDGES];

uint16_t ir_load_learned_signal(ir_raw_signal_t *signal);

#endif
