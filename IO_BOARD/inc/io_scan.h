#ifndef IO_SCAN_H
#define IO_SCAN_H

#include "io_board.h"

#include <stdint.h>

#define IO_SCAN_MAX_POINTS              128U
#define IO_SCAN_MATRIX_WORDS            ((IO_SCAN_MAX_POINTS + 31U) / 32U)

#define IO_POS_OUT_BASE                 0x0000U
#define IO_POS_IN_BASE                  0x0100U
#define IO_POS_OUT(point_1_based)       (uint16_t)(IO_POS_OUT_BASE | ((point_1_based) & 0x00FFU))
#define IO_POS_IN(point_1_based)        (uint16_t)(IO_POS_IN_BASE | ((point_1_based) & 0x00FFU))
#define IO_POS_IS_IN(code)              (((code) & 0x0100U) != 0U)
#define IO_POS_INDEX_1_BASED(code)      ((uint8_t)((code) & 0x00FFU))

typedef enum {
  IO_SCAN_PROFILE_OLD_DB50 = 0,
  IO_SCAN_PROFILE_DB78_64X4 = 1
} io_scan_profile_id_t;

typedef enum {
  IO_SCAN_OK = 0,
  IO_SCAN_INVALID_POINT,
  IO_SCAN_INVALID_PROFILE,
  IO_SCAN_NOT_IMPLEMENTED
} io_scan_status_t;

typedef struct {
  io_scan_profile_id_t id;
  const char *name;
  uint8_t out_count;
  uint8_t in_count;
} io_scan_profile_t;

typedef struct {
  uint16_t out_pos;
  uint16_t in_pos;
  uint8_t connected;
} io_scan_pair_result_t;

typedef struct {
  io_scan_profile_id_t profile_id;
  uint8_t out_count;
  uint8_t in_count;
  uint16_t active_out_pos;
  uint32_t matrix[IO_SCAN_MAX_POINTS][IO_SCAN_MATRIX_WORDS];
  uint32_t scanned_pairs;
  uint32_t connected_pairs;
  uint32_t scan_counter;
} io_scan_result_t;

extern volatile uint16_t g_scan_active_out_pos;
extern volatile uint16_t g_scan_active_in_pos;
extern volatile uint32_t g_scan_pair_counter;
extern volatile uint32_t g_scan_frame_counter;
extern volatile uint32_t g_scan_connected_counter;

void io_scan_init(io_scan_profile_id_t profile_id);
const io_scan_profile_t *io_scan_active_profile(void);
const io_scan_profile_t *io_scan_get_profile(io_scan_profile_id_t profile_id);
io_scan_status_t io_scan_select_out(uint16_t out_pos);
io_scan_status_t io_scan_select_in(uint16_t in_pos);
io_scan_status_t io_scan_read_pair(uint16_t out_pos, uint16_t in_pos, io_scan_pair_result_t *result);
io_scan_status_t io_scan_all(io_scan_result_t *result);
void io_scan_clear_result(io_scan_result_t *result);
uint8_t io_scan_position_valid(uint16_t pos_code);
uint8_t io_scan_position_to_bank_index(uint16_t pos_code, io_mux_bank_t *bank, uint8_t *index);
uint8_t io_scan_measure_selected_pair(void);

#endif
