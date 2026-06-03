#include "ir_learned_code.h"

const uint8_t g_ir_learned_code_available = 0U;
const uint8_t g_ir_learned_start_level = 0U;
const uint16_t g_ir_learned_count = 0U;
const uint16_t g_ir_learned_signal_us[IR_CAPTURE_MAX_EDGES] = {0U};

uint16_t ir_load_learned_signal(ir_raw_signal_t *signal)
{
  uint16_t i;

  if(signal == 0 || g_ir_learned_code_available == 0U) {
    return 0U;
  }

  signal->start_level = g_ir_learned_start_level;
  signal->count = g_ir_learned_count;

  for(i = 0; i < IR_CAPTURE_MAX_EDGES; i++) {
    signal->duration_us[i] = (i < g_ir_learned_count) ? g_ir_learned_signal_us[i] : 0U;
  }

  return signal->count;
}
