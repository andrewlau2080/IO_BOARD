#include "io_scan.h"

#include "at32f45x_board.h"

#include <string.h>

volatile uint16_t g_scan_active_out_pos;
volatile uint16_t g_scan_active_in_pos;
volatile uint32_t g_scan_pair_counter;
volatile uint32_t g_scan_frame_counter;
volatile uint32_t g_scan_connected_counter;

static const io_scan_profile_t scan_profiles[] = {
  {IO_SCAN_PROFILE_OLD_DB50, "old_db50_96x96", 96U, 96U},
  {IO_SCAN_PROFILE_DB78_64X4, "db78_64x4_128x128", 128U, 128U},
};

static const io_scan_profile_t *active_profile = &scan_profiles[0];

static uint16_t point_count_for_pos(uint16_t pos_code)
{
  if(active_profile == 0) {
    return 0U;
  }

  return IO_POS_IS_IN(pos_code) ? active_profile->in_count : active_profile->out_count;
}

const io_scan_profile_t *io_scan_get_profile(io_scan_profile_id_t profile_id)
{
  uint8_t i;

  for(i = 0; i < (uint8_t)(sizeof(scan_profiles) / sizeof(scan_profiles[0])); i++) {
    if(scan_profiles[i].id == profile_id) {
      return &scan_profiles[i];
    }
  }

  return 0;
}

const io_scan_profile_t *io_scan_active_profile(void)
{
  return active_profile;
}

void io_scan_init(io_scan_profile_id_t profile_id)
{
  const io_scan_profile_t *profile;

  profile = io_scan_get_profile(profile_id);
  if(profile != 0) {
    active_profile = profile;
  }

  io_mux_disable_all();
  g_scan_active_out_pos = 0U;
  g_scan_active_in_pos = 0U;
  g_scan_pair_counter = 0U;
  g_scan_connected_counter = 0U;
}

uint8_t io_scan_position_valid(uint16_t pos_code)
{
  uint8_t point;

  point = IO_POS_INDEX_1_BASED(pos_code);
  if(point == 0U) {
    return 0U;
  }

  return point <= point_count_for_pos(pos_code);
}

uint8_t io_scan_position_to_bank_index(uint16_t pos_code, io_mux_bank_t *bank, uint8_t *index)
{
  uint8_t point;
  uint8_t zero_based;

  if(!io_scan_position_valid(pos_code) || bank == 0 || index == 0) {
    return 0U;
  }

  point = IO_POS_INDEX_1_BASED(pos_code);
  zero_based = (uint8_t)(point - 1U);

  if(IO_POS_IS_IN(pos_code)) {
    *bank = (zero_based < 64U) ? IO_MUX_IN_A : IO_MUX_IN_B;
  } else {
    *bank = (zero_based < 64U) ? IO_MUX_OUT_A : IO_MUX_OUT_B;
  }
  *index = (uint8_t)(zero_based & 0x3FU);

  return 1U;
}

io_scan_status_t io_scan_select_out(uint16_t out_pos)
{
  io_mux_bank_t bank;
  uint8_t index;

  if(IO_POS_IS_IN(out_pos) || !io_scan_position_to_bank_index(out_pos, &bank, &index)) {
    return IO_SCAN_INVALID_POINT;
  }

  io_mux_select(bank, index);
  g_scan_active_out_pos = out_pos;
  return IO_SCAN_OK;
}

io_scan_status_t io_scan_select_in(uint16_t in_pos)
{
  io_mux_bank_t bank;
  uint8_t index;

  if(!IO_POS_IS_IN(in_pos) || !io_scan_position_to_bank_index(in_pos, &bank, &index)) {
    return IO_SCAN_INVALID_POINT;
  }

  io_mux_select(bank, index);
  g_scan_active_in_pos = in_pos;
  return IO_SCAN_OK;
}

__attribute__((weak)) uint8_t io_scan_measure_selected_pair(void)
{
  return 0U;
}

io_scan_status_t io_scan_read_pair(uint16_t out_pos, uint16_t in_pos, io_scan_pair_result_t *result)
{
  uint8_t connected;

  if(result == 0) {
    return IO_SCAN_INVALID_POINT;
  }
  if(IO_POS_IS_IN(out_pos) || !IO_POS_IS_IN(in_pos)) {
    return IO_SCAN_INVALID_POINT;
  }
  if(io_scan_select_out(out_pos) != IO_SCAN_OK || io_scan_select_in(in_pos) != IO_SCAN_OK) {
    return IO_SCAN_INVALID_POINT;
  }

  delay_us(20);
  connected = io_scan_measure_selected_pair();

  result->out_pos = out_pos;
  result->in_pos = in_pos;
  result->connected = connected;

  g_scan_pair_counter++;
  if(connected != 0U) {
    g_scan_connected_counter++;
  }

  return IO_SCAN_OK;
}

void io_scan_clear_result(io_scan_result_t *result)
{
  if(result != 0) {
    memset(result, 0, sizeof(*result));
  }
}

io_scan_status_t io_scan_all(io_scan_result_t *result)
{
  uint16_t out_point;
  uint16_t in_point;
  io_scan_pair_result_t pair;
  uint16_t out_pos;
  uint16_t in_pos;
  uint8_t word;
  uint8_t bit;

  if(result == 0 || active_profile == 0) {
    return IO_SCAN_INVALID_PROFILE;
  }

  io_scan_clear_result(result);
  result->profile_id = active_profile->id;
  result->out_count = active_profile->out_count;
  result->in_count = active_profile->in_count;

  for(out_point = 1U; out_point <= active_profile->out_count; out_point++) {
    out_pos = IO_POS_OUT(out_point);
    result->active_out_pos = out_pos;

    for(in_point = 1U; in_point <= active_profile->in_count; in_point++) {
      in_pos = IO_POS_IN(in_point);
      if(io_scan_read_pair(out_pos, in_pos, &pair) != IO_SCAN_OK) {
        io_mux_disable_all();
        return IO_SCAN_INVALID_POINT;
      }

      result->scanned_pairs++;
      if(pair.connected != 0U) {
        word = (uint8_t)((in_point - 1U) >> 5);
        bit = (uint8_t)((in_point - 1U) & 0x1FU);
        result->matrix[out_point - 1U][word] |= (1UL << bit);
        result->connected_pairs++;
      }
    }
  }

  io_mux_disable_all();
  result->scan_counter = ++g_scan_frame_counter;
  return IO_SCAN_OK;
}
