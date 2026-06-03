#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include "at32f45x.h"

#define IR_CAPTURE_MAX_EDGES 256U

typedef struct {
  uint8_t start_level;
  uint16_t count;
  uint16_t duration_us[IR_CAPTURE_MAX_EDGES];
} ir_raw_signal_t;

extern volatile uint8_t g_ir_rx_level;
extern volatile uint8_t g_ir_rx_last_level;
extern volatile uint8_t g_ir_new_frame_flag;
extern volatile uint8_t g_ir_code_start_level;
extern volatile uint16_t g_ir_code_count;
extern volatile uint16_t g_ir_code_table_us[IR_CAPTURE_MAX_EDGES];
extern volatile uint32_t g_ir_poll_counter;
extern volatile uint32_t g_ir_rx_edge_counter;
extern volatile uint32_t g_ir_frame_counter;
extern volatile uint32_t g_ir_capture_timeout_counter;
extern volatile uint32_t g_ir_last_edge_delta_us;
extern volatile uint8_t g_ir_tx_active;
extern volatile uint8_t g_ir_tx_level;
extern volatile uint16_t g_ir_tx_segment_index;
extern volatile uint16_t g_ir_tx_segment_count;
extern volatile uint32_t g_ir_tx_duration_us;
extern volatile uint32_t g_ir_tx_packet_us;
extern volatile uint32_t g_ir_tx_gap_us;

void ir_io_init(void);
void ir_poll_rx_status(void);
void ir_debug_frame_ready(void);
void ir_force_space_us(uint32_t duration_us);
uint16_t ir_capture_once(ir_raw_signal_t *signal, uint32_t start_timeout_ms);
void ir_print_signal(const ir_raw_signal_t *signal);
void ir_transmit_signal(const ir_raw_signal_t *signal, uint8_t repeat_count);
void ir_transmit_timings(uint8_t start_level,
                         const uint16_t *durations_us,
                         uint16_t count,
                         uint8_t repeat_count,
                         uint32_t repeat_gap_us);

#endif
