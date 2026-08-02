#include "first_gen_4051_scan.h"

#include "at32f45x.h"
#include "at32f45x_board.h"
#include "at32f45x_flash.h"
#include "device_config.h"
#include "io_board.h"
#include "io_scan.h"
#include "first_gen_display.h"
#include "tester_settings.h"
#include "tester_wifi_print.h"

#include <stdio.h>
#include <string.h>

#ifndef IO_APP_MODE
#define IO_APP_MODE 2
#endif

#define IO_APP_MODE_FIRST_GEN_4051_LOCAL 3

#ifndef FIRST_GEN_4051_DAC_CODE_DEFAULT
#define FIRST_GEN_4051_DAC_CODE_DEFAULT 3000U
#endif

#ifndef FIRST_GEN_4051_ADC_THRESHOLD_DEFAULT
#define FIRST_GEN_4051_ADC_THRESHOLD_DEFAULT 1600U
#endif

#define FIRST_GEN_4051_SCAN_PERIOD_MS 0U
#define FIRST_GEN_4051_ERROR_PERIOD_MS 120U
#define FIRST_GEN_4051_ADC_TIMEOUT    20000U
#define FIRST_GEN_HALL_DEBOUNCE_MS    20U
#define FIRST_GEN_PASS_HOLD_MS        900U
#define FIRST_GEN_REMOVE_CHECK_MS     250U
#define FIRST_GEN_REMOVE_CONFIRM_MS   10000U
#define FIRST_GEN_LEARN_MAX_CONNECTED_PAIRS 512UL
#define FIRST_GEN_PANEL_TEST_START    1U
#define FIRST_GEN_PANEL_TEST_END      47U
/* K1-K4 are polled once for every completed OUT row.  Do not add a fixed row
 * pause here: the old 50/100 ms display pacing was removed after it was
 * confirmed to make acquisition feel slow. */
#define FIRST_GEN_PANEL_TEST_STEP_MS  0U
#define FIRST_GEN_PANEL_PASS_MS       1200U
#define FIRST_GEN_PANEL_KEY_POLL_MS   5U
#define FIRST_GEN_PROBLEM_RECHECK_MS  50U
#define FIRST_GEN_PANEL_K1_LONG_MS    3000U
#define FIRST_GEN_PANEL_K3_LONG_MS    3000U
#define FIRST_GEN_PANEL_K1_DOUBLE_WINDOW_MS 160U
#define FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS 200U
#define FIRST_GEN_IDLE_SCROLL_MS      60000U
#define FIRST_GEN_TRIGGER_DIAG_ONLY   0U
#define FIRST_GEN_BUZZER_PASS_ON_MS   1000U
#define FIRST_GEN_BUZZER_PASS_OFF_MS  1000U
#define FIRST_GEN_BUZZER_NG_ON_MS     500U
#define FIRST_GEN_BUZZER_NG_GAP_MS    500U
#define FIRST_GEN_BUZZER_NG_OFF_MS    1000U
#define FIRST_GEN_LEARN_CONFIRMED_BLINK_MS 500U
#define FIRST_GEN_AUTO_NG_BLINK_MS    500U
/* Result-page fault marks blink independently of the red NG summary. */
#define FIRST_GEN_AUTO_RESULT_FAULT_BLINK_MS 400U
/* The first detected connection appears immediately.  Later record updates
 * are grouped by 16 OUT rows so LCDM's acknowledged drawing traffic cannot
 * dominate a full PASS scan; electrical open/short checks still run on every
 * OUT row while completed AUTO testing continues as a live monitor. */
#define FIRST_GEN_AUTO_RESULT_RENDER_ROW_INTERVAL 16U
/* Polling the LCDM event queue on every mux point costs more than checking a
 * short key interval.  Sixteen pair reads remain far below a perceptible K1
 * delay while keeping the electrical pass continuous. */
#define FIRST_GEN_UNLEARNED_KEY_POLL_INTERVAL 16U
#define FIRST_GEN_PROBLEM_NONE        0U
#define FIRST_GEN_PROBLEM_MISSING     1U
#define FIRST_GEN_PROBLEM_SHORT       2U
#define FIRST_GEN_SCAN_RESULT_FAIL        0U
#define FIRST_GEN_SCAN_RESULT_PASS        1U
/* A foreground key was accepted while a long PASS verification was running.
 * This is distinct from an electrical NG: leave the partial monitor frame in
 * RAM and resume it immediately after the key/page work is complete. */
#define FIRST_GEN_SCAN_RESULT_INTERRUPTED 2U
#define FIRST_GEN_SHORT_BLINK_MS      500U

#define FIRST_GEN_RECIPE_FLASH_ADDR   0x0807F800U
#define FIRST_GEN_RECIPE_MAGIC        0x3148544CU
#define FIRST_GEN_RECIPE_VERSION      2U
#define FIRST_GEN_ACTIVE_POINT_COUNT  94U
#define FIRST_GEN_A_HALF_POINT_COUNT  48U

#define FIRST_GEN_PANEL_MODE_IDLE      0U
#define FIRST_GEN_PANEL_MODE_SELF_TEST 1U
#define FIRST_GEN_PANEL_MODE_AUTO_TEST 2U
#define FIRST_GEN_PANEL_MODE_RESET     3U

#define PANEL_PROBLEM_CONTINUE         0U
#define PANEL_PROBLEM_INTERRUPTED      1U
#define PANEL_PROBLEM_RESTORED         2U

volatile uint16_t g_first_gen_last_adc1;
volatile uint16_t g_first_gen_last_adc2;
volatile uint16_t g_first_gen_adc_threshold = FIRST_GEN_4051_ADC_THRESHOLD_DEFAULT;
volatile uint16_t g_first_gen_dac_code = FIRST_GEN_4051_DAC_CODE_DEFAULT;
volatile uint16_t g_first_gen_first_fail_out;
volatile uint16_t g_first_gen_first_fail_in;
volatile uint16_t g_first_gen_current_out = 1U;
volatile uint16_t g_first_gen_current_problem_in;
volatile uint32_t g_first_gen_scan_counter;
volatile uint32_t g_first_gen_missing_counter;
volatile uint32_t g_first_gen_unexpected_counter;
volatile uint32_t g_first_gen_learn_counter;
volatile uint32_t g_first_gen_print_request_counter;
volatile uint32_t g_first_gen_print_ack_counter;
volatile uint32_t g_first_gen_print_done_counter;
volatile uint32_t g_first_gen_print_error_counter;
volatile uint32_t g_first_gen_print_blocked_counter;
volatile uint8_t g_first_gen_recipe_valid;
volatile uint8_t g_first_gen_learn_status;
volatile uint8_t g_first_gen_last_pass;
volatile uint8_t g_first_gen_print_ready;
volatile uint8_t g_first_gen_print_waiting_for_wifi;
volatile uint8_t g_first_gen_panel_mode;
volatile uint8_t g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
volatile uint8_t g_first_gen_pass_hold_active;
volatile uint8_t g_first_gen_print_done;
volatile uint8_t g_first_gen_print_trigger_level;
volatile uint8_t g_first_gen_print_trigger_count;
volatile uint8_t g_first_gen_hall_active;
volatile uint8_t g_first_gen_print_state;
volatile uint32_t g_first_gen_last_connected_pairs;
volatile uint32_t g_first_gen_learn_connected_pairs;
volatile uint16_t g_first_gen_learn_out_count;
volatile uint16_t g_first_gen_learn_in_count;
volatile uint8_t g_first_gen_learn_pending;

static io_scan_result_t scan_result;
static uint32_t expected_matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS];
/* scan_result is cleared before every OUT row.  Keep a separate complete
 * matrix for the AUTO result page so it can be reorganised by INPUT after the
 * whole cable has been measured. */
static uint32_t auto_result_matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS];
/* The priority PASS monitor temporarily seeds its work matrix from the
 * learned recipe.  Preserve the last fully measured physical matrix here so
 * K1 result rows never turn back into learned-only rows between scans. */
static uint32_t auto_result_record_matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS];
static uint16_t auto_k1_double_remaining_ms;
static uint16_t auto_result_last_render_out;
static uint16_t scan_out_point = 1U;
static uint8_t waiting_on_error;
static uint8_t current_problem_type;
static uint8_t panel_auto_enabled;
static uint8_t panel_last_test_mode;
static uint8_t panel_operation_interrupted;
static uint8_t panel_waiting_for_reconnect;
static uint32_t panel_idle_ms;
static uint8_t scan_cycle_has_problem;
static uint8_t panel_display_state;
static uint16_t panel_ng_out;
static uint16_t panel_ng_in;
static uint16_t panel_ng_tick;
static uint8_t panel_ng_phase;
static uint16_t panel_scan_tick;
static uint8_t panel_scan_phase;
static uint16_t print_hall_active_ms;
static uint32_t print_event_sequence;
static uint32_t print_event_id;
static uint32_t print_test_count;
static uint8_t printed_hold_active;
static uint16_t printed_hold_out = 1U;
static uint8_t self_test_result_ready;
static uint8_t self_test_result_page = 1U;
static uint8_t auto_result_ready;
/* The LCDM summary has priority over the result list.  A completed physical
 * matrix is retained immediately, but its formatted K1 records are prepared
 * on the following service pass so first AUTO never waits on (or shows) an
 * empty result page before PASS/NG. */
static uint8_t auto_result_cache_pending;
static uint8_t auto_result_page = 1U;
static uint8_t auto_result_summary_active;
/* After a live PASS monitor finds its first NG, make one complete physical
 * sweep in the background.  That is what lets K1 mark every bad group,
 * while the NG summary can still report the first (lowest) group at once. */
static uint8_t auto_result_fault_collecting;
/* K1 record browsing is a deliberate foreground view.  Keep it visible
 * while continuous AUTO monitoring runs in the background; the live NG
 * summary resumes only when K1 cycles back to that virtual page. */
static uint8_t auto_result_browse_active;
/* Once the first AUTO result is available, each monitor frame validates the
 * learned edges first.  Only after that compact check succeeds are all
 * non-learned combinations swept for extra/shorted connections. */
static uint8_t auto_monitor_expected_verified;
static uint8_t auto_result_ng_blink_on;
static uint16_t auto_result_ng_blink_tick;
static uint8_t auto_result_fault_blink_on;
static uint16_t auto_result_fault_blink_tick;
static uint8_t auto_result_cached_fault_type;
/* The live monitor can rediscover the same first NG many times per second.
 * Keep its complete endpoint group too, so that only a real wiring/group
 * change restarts the K1 result-page blink cycle. */
static uint32_t auto_result_cached_fault_out_bits[IO_SCAN_MATRIX_WORDS];
static uint32_t auto_result_cached_fault_in_bits[IO_SCAN_MATRIX_WORDS];
/* The currently displayed live NG.  Keeping this separate from the first
 * fault in a scan cycle prevents the same red page from being redrawn on
 * every continuous monitor pass. */
static uint16_t auto_result_displayed_ng_out;
static uint16_t auto_result_displayed_ng_in;
static uint8_t auto_result_displayed_ng_type;
static uint32_t panel_monitor_cycles_per_ms;
static uint32_t panel_monitor_last_cycles;
static uint8_t learn_result_page = 1U;
static uint8_t learn_confirmed_hold_active;
static uint16_t learn_confirmed_blink_tick;
static uint8_t buzzer_pattern;
static uint8_t buzzer_step;
static uint16_t buzzer_step_remaining_ms;

#define FIRST_GEN_DISPLAY_UNKNOWN 0U
#define FIRST_GEN_DISPLAY_PASS    1U
#define FIRST_GEN_DISPLAY_NG      2U
#define FIRST_GEN_DISPLAY_SCAN    3U
#define FIRST_GEN_NG_DISPLAY_PERIOD_SCANS 24U
#define FIRST_GEN_SCAN_DISPLAY_PERIOD_SCANS 12U
#define FIRST_GEN_BUZZER_PATTERN_NONE 0U
#define FIRST_GEN_BUZZER_PATTERN_PASS 1U
#define FIRST_GEN_BUZZER_PATTERN_NG   2U
/* One electrical circuit may contain every I and O endpoint: 94 input
 * labels + 94 output labels + the compact '-' separator require 940 bytes. */
#define FIRST_GEN_AUTO_RESULT_LINE_MAX 960U
static char auto_result_ng_line[FIRST_GEN_AUTO_RESULT_LINE_MAX];

uint8_t first_gen_4051_learn_current_harness(void);
static uint8_t first_gen_4051_learn_preview(void);
static uint8_t first_gen_4051_confirm_learn_save(void);
static void panel_start_auto_test(void);
static void panel_reset_to_zero(void);
static void matrix_clear(uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS]);
static void panel_wait_all_keys_released(uint16_t stable_ms);
static uint8_t panel_priority_delay_ms(uint32_t duration_ms);
static uint8_t panel_handle_key(void);
static uint8_t panel_handle_k3_press(void);
static uint8_t panel_settings_allowed(void);
static uint8_t scan_one_row(uint16_t out_point);
static uint8_t scan_and_check_current_row(void);
static uint8_t scan_and_check_unlearned_row(void);
static uint8_t auto_result_check_known_shorts(uint16_t *problem_out,
                                              uint16_t *problem_in);
static uint8_t scan_problem_row_live(uint16_t out_point);
static uint8_t expected_harness_find_next_problem(uint16_t start_out,
                                                  uint16_t start_in,
                                                  uint16_t *problem_out,
                                                  uint16_t *problem_in);
static void panel_arm_print_workflow(void);
static void panel_hold_print_error(void);
static uint8_t panel_service_print_event(uint16_t elapsed_ms);
static uint8_t panel_all_connections_open_for_out(uint16_t out_point);
static uint8_t panel_printed_hold_service(void);
static void display_auto_idle(void);
static void display_auto_wait_for_harness(void);
static void display_self_test_result_page(uint8_t page);
static void auto_result_store_current_row(uint16_t out_point);
static void auto_result_merge_current_row(uint16_t out_point);
static void auto_result_live_reset(void);
static void auto_result_update_lcdm_for_out(uint16_t out_point);
static void auto_result_publish_completed_fault_marks(void);
static void auto_result_complete_pending_cache(void);
static uint8_t auto_result_find_first_problem_line(char out[FIRST_GEN_AUTO_RESULT_LINE_MAX]);
static uint8_t auto_result_build_immediate_problem_line(uint16_t out_point,
                                                         uint16_t in_point,
                                                         uint8_t problem_type,
                                                         char out[FIRST_GEN_AUTO_RESULT_LINE_MAX]);
static void panel_finish_lcdm_auto_result(void);
static void panel_show_lcdm_auto_ng_now(void);
static void panel_show_lcdm_auto_pass(uint8_t force);
static void panel_show_lcdm_auto_summary(void);
static void panel_auto_result_summary_service(uint16_t elapsed_ms);
static void panel_auto_result_fault_browse_service(uint16_t elapsed_ms);
static uint8_t panel_result_page_active(void);
static uint8_t panel_result_page_next(void);
static uint8_t panel_auto_result_page_back(uint8_t count);
static void panel_auto_k1_double_tick(uint16_t elapsed_ms);
static uint8_t panel_handle_result_page_k1(void);
static void display_lcdm_total_line(char *out, uint8_t len);
static void display_lcdm_pair_line(uint16_t left, uint16_t right, char *out, uint8_t len);
static void display_auto_test_pair(uint16_t point);
static void display_learn_summary_page(uint8_t confirmed);
static void display_self_pass(void);
static void display_error_code(uint16_t point);
static uint8_t panel_check_open_pair(uint16_t point);
static void panel_run_lcdm_self_test(void);
static void lcdm_apply_learn_component_colors(void);
static uint8_t panel_scan_full_matrix(void);
static void panel_monitor_timebase_init(void);
static uint16_t panel_monitor_elapsed_ms(void);

static void buzzer_stop(void)
{
  buzzer_pattern = FIRST_GEN_BUZZER_PATTERN_NONE;
  buzzer_step = 0U;
  buzzer_step_remaining_ms = 0U;
  io_buzzer_write(0U);
}

static void buzzer_start(uint8_t pattern)
{
  buzzer_pattern = pattern;
  buzzer_step = 0U;
  buzzer_step_remaining_ms = 0U;
}

static void buzzer_set_step(uint8_t level, uint16_t duration_ms)
{
  io_buzzer_write(level);
  buzzer_step_remaining_ms = duration_ms;
}

static void buzzer_advance(void)
{
  if(buzzer_pattern == FIRST_GEN_BUZZER_PATTERN_PASS) {
    if(buzzer_step == 0U) {
      buzzer_step = 1U;
      buzzer_set_step(1U, FIRST_GEN_BUZZER_PASS_ON_MS);
    } else {
      buzzer_step = 0U;
      buzzer_set_step(0U, FIRST_GEN_BUZZER_PASS_OFF_MS);
    }
    return;
  }

  if(buzzer_pattern == FIRST_GEN_BUZZER_PATTERN_NG) {
    switch(buzzer_step) {
    case 0U:
      buzzer_step = 1U;
      buzzer_set_step(1U, FIRST_GEN_BUZZER_NG_ON_MS);
      break;
    case 1U:
      buzzer_step = 2U;
      buzzer_set_step(0U, FIRST_GEN_BUZZER_NG_GAP_MS);
      break;
    case 2U:
      buzzer_step = 3U;
      buzzer_set_step(1U, FIRST_GEN_BUZZER_NG_ON_MS);
      break;
    default:
      buzzer_step = 0U;
      buzzer_set_step(0U, FIRST_GEN_BUZZER_NG_OFF_MS);
      break;
    }
    return;
  }

  buzzer_stop();
}

static void buzzer_service(uint16_t elapsed_ms)
{
  if(buzzer_pattern == FIRST_GEN_BUZZER_PATTERN_NONE) {
    io_buzzer_write(0U);
    return;
  }

  if(buzzer_step_remaining_ms == 0U) {
    buzzer_advance();
    return;
  }

  if(elapsed_ms >= buzzer_step_remaining_ms) {
    buzzer_step_remaining_ms = 0U;
    buzzer_advance();
  } else {
    buzzer_step_remaining_ms = (uint16_t)(buzzer_step_remaining_ms - elapsed_ms);
  }
}

/* Continuous AUTO monitoring must not add a fixed millisecond pause after
 * each OUT row.  Use the Cortex-M cycle counter only for the 0.5 s NG text
 * blink, leaving the electrical scan free-running. */
static void panel_monitor_timebase_init(void)
{
  panel_monitor_cycles_per_ms = system_core_clock / 1000U;
  if(panel_monitor_cycles_per_ms == 0U) {
    panel_monitor_cycles_per_ms = 1U;
  }

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  panel_monitor_last_cycles = DWT->CYCCNT;
}

static uint16_t panel_monitor_elapsed_ms(void)
{
  uint32_t now = DWT->CYCCNT;
  uint32_t elapsed_cycles = now - panel_monitor_last_cycles;
  uint32_t elapsed_ms = elapsed_cycles / panel_monitor_cycles_per_ms;

  if(elapsed_ms == 0U) {
    return 0U;
  }
  if(elapsed_ms > 0xFFFFU) {
    panel_monitor_last_cycles = now;
    return 0xFFFFU;
  }

  panel_monitor_last_cycles += elapsed_ms * panel_monitor_cycles_per_ms;
  return (uint16_t)elapsed_ms;
}

static void panel_reset_scan_state(void)
{
  scan_out_point = 1U;
  matrix_clear(auto_result_matrix);
  matrix_clear(auto_result_record_matrix);
  auto_result_live_reset();
  auto_result_ready = 0U;
  auto_result_cache_pending = 0U;
  auto_result_page = 1U;
  auto_result_summary_active = 0U;
  auto_result_fault_collecting = 0U;
  auto_result_browse_active = 0U;
  auto_monitor_expected_verified = 0U;
  auto_result_ng_blink_on = 0U;
  auto_result_ng_blink_tick = 0U;
  auto_result_fault_blink_on = 0U;
  auto_result_fault_blink_tick = 0U;
  auto_result_cached_fault_type = FIRST_GEN_PROBLEM_NONE;
  memset(auto_result_cached_fault_out_bits, 0, sizeof(auto_result_cached_fault_out_bits));
  memset(auto_result_cached_fault_in_bits, 0, sizeof(auto_result_cached_fault_in_bits));
  auto_result_displayed_ng_out = 0U;
  auto_result_displayed_ng_in = 0U;
  auto_result_displayed_ng_type = FIRST_GEN_PROBLEM_NONE;
  auto_result_ng_line[0] = '\0';
  waiting_on_error = 0U;
  g_first_gen_first_fail_out = 0U;
  g_first_gen_first_fail_in = 0U;
  g_first_gen_current_out = 1U;
  g_first_gen_current_problem_in = 0U;
  current_problem_type = FIRST_GEN_PROBLEM_NONE;
  g_first_gen_missing_counter = 0U;
  g_first_gen_unexpected_counter = 0U;
  scan_cycle_has_problem = 0U;
  panel_display_state = FIRST_GEN_DISPLAY_UNKNOWN;
  g_first_gen_pass_hold_active = 0U;
  printed_hold_active = 0U;
  printed_hold_out = 1U;
  buzzer_stop();
  first_gen_display_clear_auto_test_result_fault();
}

static void panel_reset_print_state(void)
{
  g_first_gen_print_ready = 0U;
  g_first_gen_print_done = 0U;
  g_first_gen_print_waiting_for_wifi = 0U;
  g_first_gen_print_state = FIRST_GEN_PRINT_STATE_IDLE;
  print_hall_active_ms = 0U;
  print_event_id = 0U;
  printed_hold_active = 0U;
  printed_hold_out = 1U;
  /* A normal print reset must not tear down the separate startup/configure
   * ESP-AT exchange.  While that exchange owns raw AT capture, no print JSON
   * has been sent and it will release the transport itself on completion. */
  if(tester_settings_wifi_is_busy() == 0U) {
    tester_wifi_print_cancel();
  }
}

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t point_count;
  uint32_t words_per_row;
  uint32_t matrix_crc;
  uint32_t learn_counter;
  uint32_t reserved[2];
  uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS];
} first_gen_recipe_image_t;

static first_gen_recipe_image_t recipe_image;

static void scan_signal_gpio_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_2;
  gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
  gpio_init(GPIOA, &gpio_init_struct);

  gpio_bits_reset(GPIOA, GPIO_PINS_4 | GPIO_PINS_5);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = GPIO_PINS_4 | GPIO_PINS_5;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOA, &gpio_init_struct);
  gpio_bits_set(GPIOA, GPIO_PINS_4 | GPIO_PINS_5);
}

#if 0
static void adc_unit_config(adc_type *adc_x, adc_channel_select_type channel)
{
  adc_base_config_type adc_base_struct;

  adc_base_default_para_init(&adc_base_struct);
  adc_base_struct.sequence_mode = FALSE;
  adc_base_struct.repeat_mode = FALSE;
  adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
  adc_base_struct.ordinary_channel_length = 1U;
  adc_base_config(adc_x, &adc_base_struct);
  adc_resolution_set(adc_x, ADC_RESOLUTION_12B);
  adc_ordinary_channel_set(adc_x, channel, 1U, ADC_SAMPLETIME_92_5);
  adc_ordinary_conversion_trigger_set(adc_x, ADC_ORDINARY_TRIG_TMR1CH1, ADC_ORDINARY_TRIG_EDGE_NONE);
  adc_dma_mode_enable(adc_x, FALSE);
  adc_dma_request_repeat_enable(adc_x, FALSE);
  adc_occe_each_conversion_enable(adc_x, TRUE);
  adc_convert_fail_auto_abort_enable(adc_x, TRUE);
}

static void adc_init_inputs(void)
{
  adc_common_config_type adc_common_struct;

  crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_ADC2_PERIPH_CLOCK, TRUE);
  adc_reset();

  adc_common_default_para_init(&adc_common_struct);
  adc_common_struct.combine_mode = ADC_INDEPENDENT_MODE;
  adc_common_struct.div = ADC_HCLK_DIV_6;
  adc_common_struct.common_dma_mode = ADC_COMMON_DMAMODE_DISABLE;
  adc_common_struct.common_dma_request_repeat_state = FALSE;
  adc_common_struct.sampling_interval = ADC_SAMPLING_INTERVAL_5CYCLES;
  adc_common_struct.tempervintrv_state = FALSE;
  adc_common_struct.vbat_state = FALSE;
  adc_common_config(&adc_common_struct);

  adc_unit_config(ADC1, ADC_CHANNEL_0);
  adc_unit_config(ADC2, ADC_CHANNEL_2);

  adc_enable(ADC1, TRUE);
  adc_enable(ADC2, TRUE);
  while(adc_flag_get(ADC1, ADC_RDY_FLAG) == RESET) {
  }
  while(adc_flag_get(ADC2, ADC_RDY_FLAG) == RESET) {
  }

  adc_calibration_init(ADC1);
  while(adc_calibration_init_status_get(ADC1)) {
  }
  adc_calibration_start(ADC1);
  while(adc_calibration_status_get(ADC1)) {
  }

  adc_calibration_init(ADC2);
  while(adc_calibration_init_status_get(ADC2)) {
  }
  adc_calibration_start(ADC2);
  while(adc_calibration_status_get(ADC2)) {
  }
}
#endif

static void matrix_clear(uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS])
{
  uint16_t row;
  uint8_t word;

  for(row = 0U; row < FIRST_GEN_4051_POINT_COUNT; row++) {
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      matrix[row][word] = 0U;
    }
  }
}

static void matrix_copy(uint32_t dst[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS],
                        const uint32_t src[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS])
{
  uint16_t row;
  uint8_t word;

  for(row = 0U; row < FIRST_GEN_4051_POINT_COUNT; row++) {
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      dst[row][word] = src[row][word];
    }
  }
}

static uint32_t matrix_crc32(const uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS])
{
  uint32_t crc = 0x811C9DC5U;
  uint16_t row;
  uint8_t word;
  uint8_t byte;
  uint32_t value;

  for(row = 0U; row < FIRST_GEN_4051_POINT_COUNT; row++) {
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      value = matrix[row][word];
      for(byte = 0U; byte < 4U; byte++) {
        crc ^= (uint8_t)(value & 0xFFU);
        crc *= 16777619U;
        value >>= 8;
      }
    }
  }

  return crc;
}

static uint8_t first_set_bit_1_based(uint32_t value)
{
  uint8_t bit;

  for(bit = 0U; bit < 32U; bit++) {
    if((value & (1UL << bit)) != 0U) {
      return (uint8_t)(bit + 1U);
    }
  }

  return 0U;
}

static void display_pair(uint16_t left, uint16_t right)
{
  char text[FIRST_GEN_DISPLAY_DIGITS];
  char pair_line[20];

  if(first_gen_display_is_lcdm() != 0U) {
    display_lcdm_pair_line(left, right, pair_line, sizeof(pair_line));
    first_gen_display_show_page("",
                                "RESULT",
                                "NG",
                                pair_line,
                                "",
                                FIRST_GEN_DISPLAY_COLOR_RED,
                                FIRST_GEN_DISPLAY_COLOR_RED,
                                FIRST_GEN_DISPLAY_COLOR_WHITE);
    return;
  }

  text[0] = (char)('0' + ((left / 100U) % 10U));
  text[1] = (char)('0' + ((left / 10U) % 10U));
  text[2] = (char)('0' + (left % 10U));
  text[3] = (char)('0' + ((right / 100U) % 10U));
  text[4] = (char)('0' + ((right / 10U) % 10U));
  text[5] = (char)('0' + (right % 10U));
  first_gen_display_write_text6(text);
}

static void display_lcdm_total_line(char *out, uint8_t len)
{
  uint16_t pairs = g_first_gen_learn_out_count;
  uint32_t points = g_first_gen_learn_connected_pairs;

  if(pairs == 0U) {
    pairs = FIRST_GEN_ACTIVE_POINT_COUNT;
  }
  if(points == 0UL) {
    points = (uint32_t)pairs * 2UL;
  }

  (void)snprintf(out,
                 len,
                 "TOTAL - %03u PAIRS/%u POINTS",
                 (unsigned int)pairs,
                 (unsigned int)points);
}

static void display_lcdm_pair_line(uint16_t left, uint16_t right, char *out, uint8_t len)
{
  (void)snprintf(out,
                 len,
                 "%03u - %03u",
                 (unsigned int)left,
                 (unsigned int)right);
}

static uint16_t count_nonzero_rows(const uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS])
{
  uint16_t row;
  uint8_t word;
  uint16_t count = 0U;

  for(row = 0U; row < FIRST_GEN_ACTIVE_POINT_COUNT; row++) {
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      if(matrix[row][word] != 0U) {
        count++;
        break;
      }
    }
  }

  return count;
}

static uint16_t count_nonzero_inputs(const uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS])
{
  uint16_t in_point;
  uint16_t row;
  uint8_t word;
  uint8_t bit;
  uint16_t count = 0U;

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    word = (uint8_t)((in_point - 1U) >> 5);
    bit = (uint8_t)((in_point - 1U) & 0x1FU);
    for(row = 0U; row < FIRST_GEN_ACTIVE_POINT_COUNT; row++) {
      if((matrix[row][word] & (1UL << bit)) != 0U) {
        count++;
        break;
      }
    }
  }

  return count;
}

static uint16_t learn_union_find_root(uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U], uint16_t node)
{
  uint16_t root = node;
  uint16_t next;

  while(parent[root] != root) {
    root = parent[root];
  }

  while(parent[node] != node) {
    next = parent[node];
    parent[node] = root;
    node = next;
  }

  return root;
}

static void learn_union_find_join(uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
                                  uint16_t a,
                                  uint16_t b)
{
  uint16_t root_a = learn_union_find_root(parent, a);
  uint16_t root_b = learn_union_find_root(parent, b);

  if(root_a == root_b) {
    return;
  }
  if(root_a < root_b) {
    parent[root_b] = root_a;
  } else {
    parent[root_a] = root_b;
  }
}

static void lcdm_apply_learn_component_colors(void)
{
  uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t root_to_group[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t out_groups[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint16_t in_groups[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint16_t node;
  uint16_t out_point;
  uint16_t in_point;
  uint16_t root;
  uint16_t group = 1U;
  uint8_t word;
  uint8_t bit;

  if(first_gen_display_is_lcdm() == 0U) {
    return;
  }

  for(node = 0U; node <= (FIRST_GEN_ACTIVE_POINT_COUNT * 2U); node++) {
    parent[node] = node;
    root_to_group[node] = 0U;
  }
  for(node = 0U; node <= FIRST_GEN_ACTIVE_POINT_COUNT; node++) {
    out_groups[node] = 0U;
    in_groups[node] = 0U;
  }

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((scan_result.matrix[out_point - 1U][word] & (1UL << bit)) != 0U) {
        learn_union_find_join(parent, out_point, (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      }
    }
  }

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((scan_result.matrix[out_point - 1U][word] & (1UL << bit)) == 0U) {
        continue;
      }

      root = learn_union_find_root(parent, out_point);
      if(root_to_group[root] == 0U) {
        root_to_group[root] = group;
        group++;
      }
      out_groups[out_point] = root_to_group[root];
      in_groups[in_point] = root_to_group[root];
    }
  }

  /* The matrix is scanned first, then its finished component colours are
   * staged in the LCDM cache in one pass.  No raster traffic is generated
   * while IO scanning is in progress. */
  first_gen_display_apply_learn_table_groups(out_groups, in_groups, 0U);
}

static void display_panel_test_pair(uint16_t point)
{
  char text[FIRST_GEN_DISPLAY_DIGITS];
  uint8_t page;

  if(first_gen_display_is_lcdm() != 0U) {
    page = (point <= 47U) ? 1U : 2U;
    first_gen_display_show_auto_table_page(page, point);
    return;
  }

  text[0] = (char)('0' + ((point / 100U) % 10U));
  text[1] = (char)('0' + ((point / 10U) % 10U));
  text[2] = (char)('0' + (point % 10U));
  text[3] = text[0];
  text[4] = text[1];
  text[5] = text[2];
  first_gen_display_write_text6(text);
}

static void display_panel_test_progress(uint16_t tested_point)
{
  uint8_t page;

  if(first_gen_display_is_lcdm() != 0U) {
    if(tested_point == 0U) {
      page = 1U;
    } else {
      page = (tested_point <= 47U) ? 1U : 2U;
    }
    first_gen_display_show_auto_table_page(page, tested_point);
    return;
  }

  if(tested_point == 0U) {
    tested_point = 1U;
  }
  display_panel_test_pair(tested_point);
}

static void auto_result_store_current_row(uint16_t out_point)
{
  uint8_t word;

  if(out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return;
  }

  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    auto_result_matrix[out_point - 1U][word] = scan_result.matrix[out_point - 1U][word];
    auto_result_record_matrix[out_point - 1U][word] = scan_result.matrix[out_point - 1U][word];
  }
}

/* A priority monitor frame first seeds auto_result_matrix with the verified
 * learned matrix, then adds only observed non-learned connections.  OR-merge
 * preserves the learned edges while a sparse background row is scanned. */
static void auto_result_merge_current_row(uint16_t out_point)
{
  uint8_t word;

  if(out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return;
  }

  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    auto_result_matrix[out_point - 1U][word] |= scan_result.matrix[out_point - 1U][word];
  }
}

static void auto_result_live_reset(void)
{
  auto_k1_double_remaining_ms = 0U;
  auto_result_last_render_out = 0U;
}

static uint8_t auto_result_append_endpoint(char *out,
                                           uint16_t len,
                                           uint16_t *used,
                                           uint8_t *first,
                                           char prefix,
                                           uint16_t point)
{
  int written;

  if(out == 0 || used == 0 || first == 0 || *used >= len) {
    return 0U;
  }

  if(*first == 0U) {
    if((uint16_t)(len - *used) <= 1U) {
      return 0U;
    }
    out[(*used)++] = ',';
    out[*used] = '\0';
  }

  written = snprintf(&out[*used], (size_t)(len - *used), "%c%03u", prefix, (unsigned int)point);
  if(written != 4 || (uint16_t)written >= (uint16_t)(len - *used)) {
    return 0U;
  }
  *used = (uint16_t)(*used + (uint16_t)written);
  *first = 0U;
  return 1U;
}

static uint8_t auto_result_build_component_line(uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
                                                uint16_t component_root,
                                                char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint16_t in_point;
  uint16_t out_point;
  uint16_t used = 0U;
  uint16_t root;
  uint8_t first = 1U;
  uint8_t input_count = 0U;
  uint8_t output_count = 0U;

  out[0] = '\0';
  root = learn_union_find_root(parent, component_root);

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(learn_union_find_root(parent, (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point)) != root) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'I',
                                   in_point) == 0U) {
      return 0U;
    }
    input_count++;
  }

  if(input_count == 0U || (uint16_t)(FIRST_GEN_AUTO_RESULT_LINE_MAX - used) <= 1U) {
    return 0U;
  }
  out[used++] = '-';
  out[used] = '\0';

  first = 1U;
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(parent, out_point) != root) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'O',
                                   out_point) == 0U) {
      return 0U;
    }
    output_count++;
  }

  return (output_count != 0U) ? 1U : 0U;
}

/* Result-page records follow the physical connection first.  For every
 * actual component, keep the learned outputs belonging to its I endpoints
 * in the same line as well.  This shows a crossed lead as
 * "I002-O002,O003" instead of incorrectly merging I003 into that line. */
static uint8_t auto_result_build_actual_display_component_line(
    uint16_t actual_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_root,
    char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint8_t output_seen[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint16_t in_point;
  uint16_t out_point;
  uint16_t used = 0U;
  uint16_t root;
  uint8_t word;
  uint8_t bit;
  uint8_t first = 1U;
  uint8_t input_count = 0U;
  uint8_t output_count = 0U;

  out[0] = '\0';
  memset(output_seen, 0, sizeof(output_seen));
  root = learn_union_find_root(actual_parent, component_root);

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(learn_union_find_root(actual_parent,
                            (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point)) != root) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'I',
                                   in_point) == 0U) {
      return 0U;
    }
    input_count++;
  }
  if(input_count == 0U || (uint16_t)(FIRST_GEN_AUTO_RESULT_LINE_MAX - used) <= 1U) {
    return 0U;
  }
  out[used++] = '-';
  out[used] = '\0';

  /* Actual O endpoints in this physical component. */
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(actual_parent, out_point) == root) {
      output_seen[out_point] = 1U;
    }
  }
  /* Plus the learned O endpoints belonging to those actual I endpoints.
   * Missing learned O points therefore remain visible next to a crossed
   * connection instead of disappearing from the diagnostic record. */
  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(learn_union_find_root(actual_parent,
                            (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point)) != root) {
      continue;
    }
    word = (uint8_t)((in_point - 1U) >> 5);
    bit = (uint8_t)((in_point - 1U) & 0x1FU);
    for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
      if((expected_matrix[out_point - 1U][word] & (1UL << bit)) != 0U) {
        output_seen[out_point] = 1U;
      }
    }
  }

  first = 1U;
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(output_seen[out_point] == 0U) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'O',
                                   out_point) == 0U) {
      return 0U;
    }
    output_count++;
  }

  return (output_count != 0U) ? 1U : 0U;
}

/* If a learned I endpoint has no physical connection at all, retain one
 * compact locator row.  Its O labels appear only when those O endpoints are
 * also physically absent; if O003 has moved to I002, the row becomes
 * "I003-", exactly identifying the open I without duplicating O003. */
static uint8_t auto_result_build_missing_display_component_line(
    uint16_t expected_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_root,
    const uint8_t actual_in_has_edge[FIRST_GEN_ACTIVE_POINT_COUNT + 1U],
    const uint8_t actual_out_has_edge[FIRST_GEN_ACTIVE_POINT_COUNT + 1U],
    char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint16_t in_point;
  uint16_t out_point;
  uint16_t used = 0U;
  uint16_t root;
  uint8_t first = 1U;
  uint8_t input_count = 0U;

  out[0] = '\0';
  root = learn_union_find_root(expected_parent, component_root);

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(actual_in_has_edge[in_point] != 0U ||
       learn_union_find_root(expected_parent,
                            (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point)) != root) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'I',
                                   in_point) == 0U) {
      return 0U;
    }
    input_count++;
  }
  if(input_count == 0U || (uint16_t)(FIRST_GEN_AUTO_RESULT_LINE_MAX - used) <= 1U) {
    return 0U;
  }
  out[used++] = '-';
  out[used] = '\0';

  first = 1U;
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(actual_out_has_edge[out_point] != 0U ||
       learn_union_find_root(expected_parent, out_point) != root) {
      continue;
    }
    if(auto_result_append_endpoint(out,
                                   FIRST_GEN_AUTO_RESULT_LINE_MAX,
                                   &used,
                                   &first,
                                   'O',
                                   out_point) == 0U) {
      return 0U;
    }
  }

  return 1U;
}

static void auto_result_build_component_parent(
    const uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS],
    uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U])
{
  uint16_t node;
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;

  for(node = 0U; node <= (FIRST_GEN_ACTIVE_POINT_COUNT * 2U); node++) {
    parent[node] = node;
  }

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((matrix[out_point - 1U][word] & (1UL << bit)) != 0U) {
        learn_union_find_join(parent,
                              out_point,
                              (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      }
    }
  }
}

static void auto_result_fault_set_bit(uint32_t bits[IO_SCAN_MATRIX_WORDS],
                                      uint16_t point)
{
  uint8_t word;
  uint8_t bit;

  if(point == 0U || point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return;
  }

  word = (uint8_t)((point - 1U) >> 5);
  bit = (uint8_t)((point - 1U) & 0x1FU);
  bits[word] |= (1UL << bit);
}

static void auto_result_fault_collect_component(
    uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_node,
    uint32_t out_bits[IO_SCAN_MATRIX_WORDS],
    uint32_t in_bits[IO_SCAN_MATRIX_WORDS])
{
  uint16_t out_point;
  uint16_t in_point;
  uint16_t root;

  if(component_node == 0U ||
     component_node > (FIRST_GEN_ACTIVE_POINT_COUNT * 2U)) {
    return;
  }

  root = learn_union_find_root(parent, component_node);
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(parent, out_point) == root) {
      auto_result_fault_set_bit(out_bits, out_point);
    }
  }
  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(learn_union_find_root(parent,
                            (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point)) == root) {
      auto_result_fault_set_bit(in_bits, in_point);
    }
  }
}

static uint8_t auto_result_fault_group_changed(
    const uint32_t out_bits[IO_SCAN_MATRIX_WORDS],
    const uint32_t in_bits[IO_SCAN_MATRIX_WORDS],
    uint8_t problem_type)
{
  uint8_t word;

  if(auto_result_cached_fault_type != problem_type) {
    return 1U;
  }
  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    if(auto_result_cached_fault_out_bits[word] != out_bits[word] ||
       auto_result_cached_fault_in_bits[word] != in_bits[word]) {
      return 1U;
    }
  }
  return 0U;
}

/* Publish only a changed endpoint set.  The same live fault is rediscovered
 * many times while monitoring, and repeatedly resetting its timer was the
 * reason that the result-page flash previously appeared just once. */
static void auto_result_publish_fault_group(
    const uint32_t out_bits[IO_SCAN_MATRIX_WORDS],
    const uint32_t in_bits[IO_SCAN_MATRIX_WORDS],
    uint8_t problem_type)
{
  uint8_t word;

  if(auto_result_fault_group_changed(out_bits, in_bits, problem_type) == 0U) {
    return;
  }

  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    auto_result_cached_fault_out_bits[word] = out_bits[word];
    auto_result_cached_fault_in_bits[word] = in_bits[word];
  }
  auto_result_cached_fault_type = problem_type;
  auto_result_fault_blink_tick = 0U;
  auto_result_fault_blink_on = (problem_type != FIRST_GEN_PROBLEM_NONE) ? 1U : 0U;

  if(problem_type == FIRST_GEN_PROBLEM_NONE) {
    first_gen_display_clear_auto_test_result_fault();
  } else {
    first_gen_display_set_auto_test_result_fault_group(out_bits,
                                                        in_bits,
                                                        IO_SCAN_MATRIX_WORDS,
                                                        problem_type);
  }
}

/* The completed physical AUTO matrix is the source of truth for K1 result
 * pages.  Mark every learned component containing an open and every actual
 * component containing a short.  An actual bridge therefore marks the whole
 * merged I/O circuit; an isolated open marks its learned I/O group. */
static void auto_result_publish_completed_fault_marks(void)
{
  uint16_t expected_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t actual_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint32_t out_bits[IO_SCAN_MATRIX_WORDS] = {0U};
  uint32_t in_bits[IO_SCAN_MATRIX_WORDS] = {0U};
  uint16_t out_point;
  uint8_t word;
  uint8_t has_missing = 0U;
  uint8_t has_short = 0U;

  auto_result_build_component_parent(expected_matrix, expected_parent);
  auto_result_build_component_parent(auto_result_record_matrix, actual_parent);

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      if((expected_matrix[out_point - 1U][word] &
          ~auto_result_record_matrix[out_point - 1U][word]) != 0U) {
        has_missing = 1U;
        auto_result_fault_collect_component(expected_parent,
                                            out_point,
                                            out_bits,
                                            in_bits);
      }
      if((auto_result_record_matrix[out_point - 1U][word] &
          ~expected_matrix[out_point - 1U][word]) != 0U) {
        has_short = 1U;
        auto_result_fault_collect_component(actual_parent,
                                            out_point,
                                            out_bits,
                                            in_bits);
      }
    }
  }

  if(has_missing == 0U && has_short == 0U) {
    static const uint32_t no_fault_bits[IO_SCAN_MATRIX_WORDS] = {0U};
    auto_result_publish_fault_group(no_fault_bits,
                                    no_fault_bits,
                                    FIRST_GEN_PROBLEM_NONE);
    return;
  }

  auto_result_publish_fault_group(out_bits,
                                  in_bits,
                                  (has_short != 0U) ? FIRST_GEN_PROBLEM_SHORT :
                                                       FIRST_GEN_PROBLEM_MISSING);
}

static uint16_t auto_result_collect_component_roots(
    const uint32_t matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS],
    uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t root_to_component[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_root[FIRST_GEN_ACTIVE_POINT_COUNT + 1U])
{
  uint16_t node;
  uint16_t in_point;
  uint16_t out_point;
  uint16_t root;
  uint16_t component_count = 0U;
  uint8_t word;
  uint8_t bit;

  for(node = 0U; node <= (FIRST_GEN_ACTIVE_POINT_COUNT * 2U); node++) {
    root_to_component[node] = 0U;
  }

  /* This is deliberately the same order used by the AUTO result list:
   * earliest I point first, then each complete electrical connection group. */
  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    word = (uint8_t)((in_point - 1U) >> 5);
    bit = (uint8_t)((in_point - 1U) & 0x1FU);
    for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
      if((matrix[out_point - 1U][word] & (1UL << bit)) != 0U) {
        break;
      }
    }
    if(out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
      continue;
    }

    root = learn_union_find_root(parent, (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
    if(root_to_component[root] == 0U) {
      component_count++;
      root_to_component[root] = component_count;
      component_root[component_count] = root;
    }
  }

  return component_count;
}

static uint8_t auto_result_component_has_difference(
    uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_root)
{
  uint16_t out_point;
  uint8_t word;

  component_root = learn_union_find_root(parent, component_root);
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(parent, out_point) != component_root) {
      continue;
    }
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      if(expected_matrix[out_point - 1U][word] != auto_result_matrix[out_point - 1U][word]) {
        return 1U;
      }
    }
  }

  return 0U;
}

static uint8_t auto_result_component_has_unexpected_connection(
    uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t component_root)
{
  uint16_t out_point;
  uint8_t word;

  component_root = learn_union_find_root(parent, component_root);
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(parent, out_point) != component_root) {
      continue;
    }
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      if((auto_result_matrix[out_point - 1U][word] &
          ~expected_matrix[out_point - 1U][word]) != 0U) {
        return 1U;
      }
    }
  }

  return 0U;
}

static uint8_t auto_result_build_current_component_for_expected(
    uint16_t expected_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    uint16_t expected_root,
    uint16_t actual_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U],
    char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint16_t out_point;
  uint16_t actual_root;
  uint8_t word;

  expected_root = learn_union_find_root(expected_parent, expected_root);
  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    if(learn_union_find_root(expected_parent, out_point) != expected_root) {
      continue;
    }
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      if(auto_result_matrix[out_point - 1U][word] != 0U) {
        actual_root = learn_union_find_root(actual_parent, out_point);
        return auto_result_build_component_line(actual_parent, actual_root, out);
      }
    }
  }

  return 0U;
}

static uint8_t auto_result_find_first_problem_line(char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint16_t expected_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t actual_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t root_to_component[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t component_root[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint16_t component_count;
  uint16_t component;
  uint16_t root;

  if(out == 0) {
    return 0U;
  }
  out[0] = '\0';

  auto_result_build_component_parent(expected_matrix, expected_parent);
  auto_result_build_component_parent(auto_result_matrix, actual_parent);
  component_count = auto_result_collect_component_roots(expected_matrix,
                                                         expected_parent,
                                                         root_to_component,
                                                         component_root);

  for(component = 1U; component <= component_count; component++) {
    root = component_root[component];
    if(auto_result_component_has_difference(expected_parent, root) == 0U) {
      continue;
    }

    /* A short/extra lead is more useful when shown as the current merged
     * group.  A missing lead retains the learned group, which makes the
     * absent endpoint visible. */
    if(auto_result_component_has_unexpected_connection(expected_parent, root) != 0U &&
       auto_result_build_current_component_for_expected(expected_parent,
                                                        root,
                                                        actual_parent,
                                                        out) != 0U) {
      return 1U;
    }
    return auto_result_build_component_line(expected_parent, root, out);
  }

  /* An unexpected connection may involve an unused learned output.  It has
   * no expected component, so use the first current component in the same
   * I-first order as the on-screen AUTO record. */
  component_count = auto_result_collect_component_roots(auto_result_matrix,
                                                         actual_parent,
                                                         root_to_component,
                                                         component_root);
  for(component = 1U; component <= component_count; component++) {
    root = component_root[component];
    if(auto_result_component_has_difference(actual_parent, root) != 0U &&
       auto_result_build_component_line(actual_parent, root, out) != 0U) {
      return 1U;
    }
  }

  return 0U;
}

/* A full AUTO record is intentionally not required before reporting a real
 * fault.  For an open, show the complete learned connection group; for a
 * short/extra lead, show the already measured current group.  This keeps the
 * first NG report accurate even though later, unrelated OUT rows have not
 * yet been scanned. */
static uint8_t auto_result_build_immediate_problem_line(
    uint16_t out_point,
    uint16_t in_point,
    uint8_t problem_type,
    char out[FIRST_GEN_AUTO_RESULT_LINE_MAX])
{
  uint16_t parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];

  if(out == 0 || out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT ||
     in_point == 0U || in_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return 0U;
  }
  out[0] = '\0';

  if(problem_type == FIRST_GEN_PROBLEM_MISSING) {
    auto_result_build_component_parent(expected_matrix, parent);
    if(auto_result_build_component_line(parent, out_point, out) != 0U) {
      return 1U;
    }
  } else if(problem_type == FIRST_GEN_PROBLEM_SHORT) {
    auto_result_build_component_parent(auto_result_matrix, parent);
    if(auto_result_build_component_line(parent, out_point, out) != 0U) {
      return 1U;
    }
  }

  (void)snprintf(out,
                 FIRST_GEN_AUTO_RESULT_LINE_MAX,
                 "I%03u-O%03u",
                 (unsigned int)in_point,
                 (unsigned int)out_point);
  return 1U;
}

static void auto_result_update_lcdm_for_out(uint16_t out_point)
{
  uint16_t actual_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t expected_parent[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint16_t in_point;
  uint16_t scanned_out;
  uint16_t root;
  uint16_t display_row = 0U;
  uint16_t actual_input_count = 0U;
  uint16_t actual_output_count = 0U;
  uint8_t word;
  uint8_t bit;
  uint8_t has_current_connection = 0U;
  uint8_t render_due = 0U;
  uint8_t actual_in_has_edge[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint8_t actual_out_has_edge[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint8_t expected_in_has_edge[FIRST_GEN_ACTIVE_POINT_COUNT + 1U];
  uint8_t actual_root_emitted[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  uint8_t expected_root_emitted[(FIRST_GEN_ACTIVE_POINT_COUNT * 2U) + 1U];
  char line[FIRST_GEN_AUTO_RESULT_LINE_MAX];

  if(first_gen_display_is_lcdm() == 0U ||
     out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return;
  }

  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    if(auto_result_record_matrix[out_point - 1U][word] != 0U) {
      has_current_connection = 1U;
      break;
    }
  }
  if((auto_result_last_render_out == 0U && has_current_connection != 0U) ||
     out_point == FIRST_GEN_ACTIVE_POINT_COUNT ||
     (auto_result_last_render_out != 0U &&
      (uint16_t)(out_point - auto_result_last_render_out) >=
        FIRST_GEN_AUTO_RESULT_RENDER_ROW_INTERVAL)) {
    render_due = 1U;
  }
  if(render_due == 0U ||
     (has_current_connection == 0U && auto_result_last_render_out == 0U &&
      out_point != FIRST_GEN_ACTIVE_POINT_COUNT)) {
    return;
  }

  memset(actual_in_has_edge, 0, sizeof(actual_in_has_edge));
  memset(actual_out_has_edge, 0, sizeof(actual_out_has_edge));
  memset(expected_in_has_edge, 0, sizeof(expected_in_has_edge));
  memset(actual_root_emitted, 0, sizeof(actual_root_emitted));
  memset(expected_root_emitted, 0, sizeof(expected_root_emitted));
  auto_result_build_component_parent(auto_result_record_matrix, actual_parent);
  auto_result_build_component_parent(expected_matrix, expected_parent);

  for(scanned_out = 1U; scanned_out <= FIRST_GEN_ACTIVE_POINT_COUNT; scanned_out++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((auto_result_record_matrix[scanned_out - 1U][word] & (1UL << bit)) != 0U) {
        actual_out_has_edge[scanned_out] = 1U;
        actual_in_has_edge[in_point] = 1U;
      }
      if((expected_matrix[scanned_out - 1U][word] & (1UL << bit)) != 0U) {
        expected_in_has_edge[in_point] = 1U;
      }
    }
  }

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(actual_in_has_edge[in_point] != 0U) {
      actual_input_count++;
      root = learn_union_find_root(actual_parent,
                                   (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      actual_root_emitted[root] = 1U;
    }

    /* A fully detached learned I endpoint still needs a result record.  It
     * is emitted at its smallest I number, after any earlier actual group. */
    if(expected_in_has_edge[in_point] != 0U && actual_in_has_edge[in_point] == 0U) {
      root = learn_union_find_root(expected_parent,
                                   (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      expected_root_emitted[root] = 1U;
    }
  }
  for(scanned_out = 1U; scanned_out <= FIRST_GEN_ACTIVE_POINT_COUNT; scanned_out++) {
    if(actual_out_has_edge[scanned_out] != 0U) {
      actual_output_count++;
    }
  }

  first_gen_display_reset_auto_test_result_lines();
  /* Resetting the LCDM cache must happen before visual rows are installed,
   * otherwise an old row could survive after a wire is removed. */
  display_row = 0U;
  /* Re-run the compact ordered emission after the cache clear. */
  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(actual_in_has_edge[in_point] != 0U) {
      root = learn_union_find_root(actual_parent,
                                   (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      if(actual_root_emitted[root] == 1U) {
        actual_root_emitted[root] = 2U;
        if(auto_result_build_actual_display_component_line(actual_parent, root, line) != 0U) {
          display_row++;
          first_gen_display_add_auto_test_result_line(display_row, line);
        }
      }
    }
    if(expected_in_has_edge[in_point] != 0U && actual_in_has_edge[in_point] == 0U) {
      root = learn_union_find_root(expected_parent,
                                   (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + in_point));
      if(expected_root_emitted[root] == 1U) {
        expected_root_emitted[root] = 2U;
        if(auto_result_build_missing_display_component_line(expected_parent,
                                                            root,
                                                            actual_in_has_edge,
                                                            actual_out_has_edge,
                                                            line) != 0U) {
          display_row++;
          first_gen_display_add_auto_test_result_line(display_row, line);
        }
      }
    }
  }
  first_gen_display_set_auto_test_result_actual_counts(actual_input_count,
                                                        actual_output_count);

  auto_result_page = first_gen_display_auto_test_page_count();
  auto_result_last_render_out = out_point;
  /* The final matrix rebuild is for K1 result browsing and the PASS/NG
   * comparison.  The summary page is drawn immediately afterwards, so avoid
   * one redundant full LCDM result-page transfer at OUT094. */
  if(out_point != FIRST_GEN_ACTIVE_POINT_COUNT) {
    first_gen_display_show_auto_test_result_page(auto_result_page, 0U);
  } else if(auto_result_browse_active != 0U) {
    /* A background NG sweep may have just changed several result rows while
     * K1 is open.  Paint the rebuilt current page once; the display backend
     * redraws only the middle result area and preserves both fixed bands. */
    first_gen_display_show_auto_test_result_page(auto_result_page, 1U);
  }
}

/* Keep electrical verdict first, list formatting second.  This function only
 * rebuilds RAM/cache data while the PASS/NG summary is on screen; it draws a
 * result page only when the operator has deliberately opened K1 browsing. */
static void auto_result_complete_pending_cache(void)
{
  if(auto_result_cache_pending == 0U || first_gen_display_is_lcdm() == 0U) {
    return;
  }

  auto_result_publish_completed_fault_marks();
  auto_result_update_lcdm_for_out(FIRST_GEN_ACTIVE_POINT_COUNT);
  auto_result_cache_pending = 0U;
}

static void panel_show_lcdm_auto_ng_now(void)
{
  if(first_gen_display_is_lcdm() == 0U) {
    return;
  }

  /* Keep a result page stable only while it represents the same live fault.
   * A new open/short (including PASS -> NG) is a real state change, so it
   * must immediately take the operator back to the current NG summary. */
  if(auto_result_browse_active != 0U) {
    if(auto_result_displayed_ng_type != FIRST_GEN_PROBLEM_NONE &&
       auto_result_displayed_ng_out == panel_ng_out &&
       auto_result_displayed_ng_in == panel_ng_in &&
       auto_result_displayed_ng_type == current_problem_type) {
      return;
    }
    auto_result_browse_active = 0U;
    auto_result_summary_active = 0U;
  }

  /* While one fault remains present, retain its existing blinking page.  A
   * different fault, or a K1 result-page browse, immediately brings the live
   * NG indication back to the foreground. */
  if(auto_result_summary_active != 0U &&
     g_first_gen_last_pass == 0U &&
     auto_result_displayed_ng_out == panel_ng_out &&
     auto_result_displayed_ng_in == panel_ng_in &&
     auto_result_displayed_ng_type == current_problem_type) {
    return;
  }

  if(auto_result_build_immediate_problem_line(panel_ng_out,
                                              panel_ng_in,
                                              current_problem_type,
                                              auto_result_ng_line) == 0U &&
     auto_result_find_first_problem_line(auto_result_ng_line) == 0U) {
    if(panel_ng_out != 0U && panel_ng_in != 0U) {
      (void)snprintf(auto_result_ng_line,
                     sizeof(auto_result_ng_line),
                     "I%03u-O%03u",
                     (unsigned int)panel_ng_in,
                     (unsigned int)panel_ng_out);
    } else {
      (void)snprintf(auto_result_ng_line, sizeof(auto_result_ng_line), "FAULT");
    }
  }

  g_first_gen_last_pass = 0U;
  auto_result_summary_active = 1U;
  auto_result_ng_blink_tick = 0U;
  auto_result_ng_blink_on = 1U;
  auto_result_displayed_ng_out = panel_ng_out;
  auto_result_displayed_ng_in = panel_ng_in;
  auto_result_displayed_ng_type = current_problem_type;
  first_gen_display_show_auto_test_ng_summary(auto_result_ng_line);
  if(auto_result_ready != 0U) {
    first_gen_display_set_k1_page_hint(1U);
  }
}

static void panel_show_lcdm_auto_pass(uint8_t force)
{
  char total_line[36];

  if(first_gen_display_is_lcdm() == 0U) {
    return;
  }
  if(force == 0U && g_first_gen_last_pass != 0U) {
    return;
  }

  /* A repaired cable is a state change too.  Do not leave a stale record
   * page in the foreground once its live NG has recovered to PASS. */
  auto_result_browse_active = 0U;
  auto_result_fault_collecting = 0U;
  auto_result_ng_line[0] = '\0';
  auto_result_summary_active = 1U;
  auto_result_ng_blink_tick = 0U;
  auto_result_ng_blink_on = 0U;
  auto_result_fault_blink_tick = 0U;
  auto_result_fault_blink_on = 0U;
  auto_result_cached_fault_type = FIRST_GEN_PROBLEM_NONE;
  memset(auto_result_cached_fault_out_bits, 0, sizeof(auto_result_cached_fault_out_bits));
  memset(auto_result_cached_fault_in_bits, 0, sizeof(auto_result_cached_fault_in_bits));
  auto_result_displayed_ng_out = 0U;
  auto_result_displayed_ng_in = 0U;
  auto_result_displayed_ng_type = FIRST_GEN_PROBLEM_NONE;
  g_first_gen_last_pass = 1U;
  first_gen_display_clear_auto_test_result_fault();
  display_lcdm_total_line(total_line, sizeof(total_line));
  first_gen_display_show_auto_test_pass_summary(total_line);
  if(auto_result_ready != 0U) {
    first_gen_display_set_k1_page_hint(1U);
  }
}

/* K1 treats the current PASS/NG panel as one virtual page before/after the
 * cached AUTO record pages.  The display backend preserves its fixed top and
 * bottom frame while this helper redraws only the middle summary. */
static void panel_show_lcdm_auto_summary(void)
{
  auto_result_browse_active = 0U;
  if(g_first_gen_last_pass != 0U) {
    panel_show_lcdm_auto_pass(1U);
  } else {
    /* Force the same live NG back from a record page so its fault line and
     * 0.5 s blink resume immediately. */
    auto_result_summary_active = 0U;
    panel_show_lcdm_auto_ng_now();
  }
}

/* A completed cycle publishes PASS only after all 94 OUT rows agree with the
 * recipe.  It deliberately leaves panel_auto_enabled set so the next cycle
 * immediately continues as a live cable monitor. */
static void panel_finish_lcdm_auto_result(void)
{
  uint8_t pass = (scan_cycle_has_problem == 0U) ? 1U : 0U;

  io_mux_disable_all();
  first_gen_display_set_auto_test_blink(0U);
  auto_result_ready = 1U;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_AUTO_TEST;
  g_first_gen_pass_hold_active = 0U;
  panel_display_state = FIRST_GEN_DISPLAY_SCAN;
  buzzer_stop();
  if(auto_result_page == 0U) {
    auto_result_page = 1U;
  }

  if(pass != 0U) {
    /* The first fast precheck may already have put a clean cable on PASS.
     * Completing the invisible record sweep must only enable K1 browsing; a
     * second PASS redraw is both unnecessary and visually looks like a
     * state change.  Draw only when this completed scan is genuinely
     * recovering from NG. */
    if(g_first_gen_last_pass == 0U) {
      panel_show_lcdm_auto_pass(0U);
    }
    /* A PASS is armed once for its Hall-triggered print workflow.  Later
     * monitor frames preserve that state instead of rearming/reprinting. */
    panel_arm_print_workflow();
  } else {
    panel_reset_print_state();
    g_first_gen_last_pass = 0U;
    if(auto_result_summary_active == 0U) {
      panel_show_lcdm_auto_ng_now();
    }
  }

  /* K1 can browse the cached record after the first complete cycle while the
   * invisible electrical monitor continues to run. */
  first_gen_display_set_k1_page_hint(1U);
}

static void panel_auto_result_summary_service(uint16_t elapsed_ms)
{
  if(auto_result_browse_active != 0U ||
     auto_result_summary_active == 0U ||
     g_first_gen_panel_mode != FIRST_GEN_PANEL_MODE_AUTO_TEST ||
     g_first_gen_last_pass != 0U ||
     first_gen_display_is_lcdm() == 0U) {
    return;
  }

  if(elapsed_ms >= (uint16_t)(FIRST_GEN_AUTO_NG_BLINK_MS - auto_result_ng_blink_tick)) {
    auto_result_ng_blink_tick = 0U;
    auto_result_ng_blink_on ^= 1U;
    first_gen_display_update_auto_test_ng_detail((auto_result_ng_blink_on != 0U) ?
                                                  auto_result_ng_line : "");
  } else {
    auto_result_ng_blink_tick = (uint16_t)(auto_result_ng_blink_tick + elapsed_ms);
  }
}

/* On K1 result pages, blink only the I/O tokens that belong to the live NG.
 * The regular red NG summary keeps its own body blink; the two animations are
 * mutually exclusive so neither redraws the other page or K1-K4. */
static void panel_auto_result_fault_browse_service(uint16_t elapsed_ms)
{
  if(auto_result_browse_active == 0U ||
     g_first_gen_panel_mode != FIRST_GEN_PANEL_MODE_AUTO_TEST ||
     g_first_gen_last_pass != 0U ||
     auto_result_cached_fault_type == FIRST_GEN_PROBLEM_NONE ||
     first_gen_display_is_lcdm() == 0U) {
    return;
  }

  if(elapsed_ms >=
     (uint16_t)(FIRST_GEN_AUTO_RESULT_FAULT_BLINK_MS - auto_result_fault_blink_tick)) {
    auto_result_fault_blink_tick = 0U;
    auto_result_fault_blink_on ^= 1U;
    first_gen_display_set_auto_test_result_fault_blink(auto_result_fault_blink_on);
  } else {
    auto_result_fault_blink_tick =
      (uint16_t)(auto_result_fault_blink_tick + elapsed_ms);
  }
}

static uint8_t panel_result_page_active(void)
{
  if(g_first_gen_learn_pending != 0U || learn_confirmed_hold_active != 0U) {
    return 1U;
  }
  if(self_test_result_ready != 0U && g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_SELF_TEST) {
    return 1U;
  }
  if(auto_result_ready != 0U && g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_AUTO_TEST) {
    return 1U;
  }
  return 0U;
}

static uint8_t panel_result_page_next(void)
{
  uint8_t page_count;

  if(g_first_gen_learn_pending != 0U || learn_confirmed_hold_active != 0U) {
    learn_result_page = (learn_result_page == 1U) ? 2U : 1U;
    display_learn_summary_page((learn_confirmed_hold_active != 0U) ? 1U : 0U);
  } else if(self_test_result_ready != 0U &&
            g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_SELF_TEST) {
    self_test_result_page = (self_test_result_page == 1U) ? 2U : 1U;
    display_self_test_result_page(self_test_result_page);
  } else if(auto_result_ready != 0U &&
            g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_AUTO_TEST) {
    page_count = first_gen_display_auto_test_page_count();
    if(auto_result_summary_active != 0U) {
      auto_result_page = 1U;
      auto_result_summary_active = 0U;
      auto_result_browse_active = 1U;
      if(g_first_gen_last_pass == 0U &&
         auto_result_cached_fault_type != FIRST_GEN_PROBLEM_NONE) {
        auto_result_fault_blink_tick = 0U;
        auto_result_fault_blink_on = 1U;
        first_gen_display_set_auto_test_result_fault_blink(1U);
      }
      first_gen_display_show_auto_test_result_page(auto_result_page, 1U);
    } else if(auto_result_page >= page_count) {
      panel_show_lcdm_auto_summary();
    } else {
      auto_result_page++;
      auto_result_browse_active = 1U;
      first_gen_display_show_auto_test_result_page(auto_result_page, 1U);
    }
  } else {
    return 0U;
  }

  first_gen_display_set_k1_page_hint(1U);
  return 1U;
}

static uint8_t panel_auto_result_page_back(uint8_t count)
{
  uint8_t page_count;
  uint8_t summary_selected;

  if(auto_result_ready == 0U ||
     g_first_gen_panel_mode != FIRST_GEN_PANEL_MODE_AUTO_TEST) {
    return 0U;
  }

  page_count = first_gen_display_auto_test_page_count();
  summary_selected = auto_result_summary_active;
  while(count != 0U) {
    if(summary_selected != 0U) {
      auto_result_page = page_count;
      summary_selected = 0U;
    } else if(auto_result_page <= 1U) {
      summary_selected = 1U;
    } else {
      auto_result_page--;
    }
    count--;
  }
  auto_result_summary_active = summary_selected;
  auto_result_browse_active = (summary_selected != 0U) ? 0U : 1U;
  if(summary_selected != 0U) {
    panel_show_lcdm_auto_summary();
  } else {
    if(g_first_gen_last_pass == 0U &&
       auto_result_cached_fault_type != FIRST_GEN_PROBLEM_NONE) {
      auto_result_fault_blink_tick = 0U;
      auto_result_fault_blink_on = 1U;
      first_gen_display_set_auto_test_result_fault_blink(1U);
    }
    first_gen_display_show_auto_test_result_page(auto_result_page, 1U);
  }
  first_gen_display_set_k1_page_hint(1U);
  return 1U;
}

static void panel_auto_k1_double_tick(uint16_t elapsed_ms)
{
  if(auto_k1_double_remaining_ms == 0U) {
    return;
  }

  if(elapsed_ms >= auto_k1_double_remaining_ms) {
    auto_k1_double_remaining_ms = 0U;
  } else {
    auto_k1_double_remaining_ms = (uint16_t)(auto_k1_double_remaining_ms - elapsed_ms);
  }
}

static uint8_t panel_handle_result_page_k1(void)
{
  uint8_t auto_result_page_active;

  if(panel_result_page_active() == 0U || first_gen_display_is_lcdm() == 0U) {
    return 0U;
  }

  auto_result_page_active = (auto_result_ready != 0U &&
                             g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_AUTO_TEST) ? 1U : 0U;

  if(auto_result_page_active != 0U) {
    /* PASS/NG is allowed to appear before the formatted result records.  If
     * K1 is touched immediately afterwards, finish that RAM-only work first
     * so this first page is complete rather than an empty white list. */
    auto_result_complete_pending_cache();
    if(auto_k1_double_remaining_ms != 0U) {
      /* The first touch already advanced immediately.  A second touch inside
       * the short window converts that pair into one page backward. */
      auto_k1_double_remaining_ms = 0U;
      (void)panel_auto_result_page_back(2U);
    } else {
      (void)panel_result_page_next();
      auto_k1_double_remaining_ms = FIRST_GEN_PANEL_K1_DOUBLE_WINDOW_MS;
    }
  } else {
    (void)panel_result_page_next();
  }

  /* The LCDM driver already latches one physical press until its release.
   * Do not busy-wait for that release here: returning immediately lets the
   * scanner continue and makes the next page start drawing on the touch.
   * panel_handle_key() clears the latch normally when the release arrives. */
  return 1U;
}

static void display_auto_test_pair(uint16_t point)
{
  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_auto_test_line(point, "", 0U);
    return;
  }

  {
    char text[FIRST_GEN_DISPLAY_DIGITS];
    text[0] = (char)('0' + ((point / 100U) % 10U));
    text[1] = (char)('0' + ((point / 10U) % 10U));
    text[2] = (char)('0' + (point % 10U));
    text[3] = text[0];
    text[4] = text[1];
    text[5] = text[2];
    first_gen_display_write_text6(text);
  }
}

static void display_learn_summary_page(uint8_t confirmed)
{
  (void)confirmed;

  if(first_gen_display_is_lcdm() == 0U) {
    first_gen_display_write_learn_summary(g_first_gen_learn_out_count,
                                          g_first_gen_learn_in_count,
                                          g_first_gen_learn_connected_pairs);
    return;
  }

  if(learn_result_page == 0U) {
    learn_result_page = 1U;
  }
  if(learn_result_page > 2U) {
    learn_result_page = 2U;
  }

  first_gen_display_show_learn_table_page(learn_result_page,
                                          0U,
                                          FIRST_GEN_ACTIVE_POINT_COUNT,
                                          g_first_gen_learn_out_count,
                                          g_first_gen_learn_connected_pairs,
                                          confirmed != 0U ? 2U : 1U);
  first_gen_display_set_k1_page_hint(1U);
}

static uint8_t panel_check_open_pair(uint16_t point)
{
  io_scan_pair_result_t pair;

  if(point == 0U || point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return 1U;
  }

  if(io_scan_read_pair(IO_POS_OUT(point), IO_POS_IN(point), &pair) != IO_SCAN_OK) {
    return 0U;
  }

  return (pair.connected == 0U) ? 1U : 0U;
}

static void panel_run_lcdm_self_test(void)
{
  panel_auto_enabled = 0U;
  first_gen_display_set_auto_test_blink(0U);
  panel_last_test_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
  self_test_result_ready = 0U;

  /* Keep the screen lightweight while the shared matrix scanner runs.  The
   * old per-row grid update made the IO scan wait for LCDM acknowledgements. */
  first_gen_display_show_page("",
                              "SELF TESTING",
                              "SCANNING",
                              "",
                              "",
                              FIRST_GEN_DISPLAY_COLOR_BLUE,
                              FIRST_GEN_DISPLAY_COLOR_WHITE,
                              FIRST_GEN_DISPLAY_COLOR_BLUE);
  if(panel_scan_full_matrix() == 0U) {
    display_error_code(1U);
    return;
  }

  g_first_gen_last_connected_pairs = scan_result.connected_pairs;
  first_gen_display_set_self_test_matrix(&scan_result.matrix[0][0],
                                          FIRST_GEN_ACTIVE_POINT_COUNT,
                                          IO_SCAN_MATRIX_WORDS);
  self_test_result_ready = 1U;
  self_test_result_page = 1U;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
  display_self_test_result_page(self_test_result_page);
}

static void display_pass(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'P', 'A', 'S', 'S', ' ', ' '};
  char total_line[36];

  if(first_gen_display_is_lcdm() != 0U) {
    display_lcdm_total_line(total_line, sizeof(total_line));
    first_gen_display_show_page("",
                                "RESULT",
                                "PASS",
                                total_line,
                                "",
                                FIRST_GEN_DISPLAY_COLOR_GREEN,
                                FIRST_GEN_DISPLAY_COLOR_GREEN,
                                FIRST_GEN_DISPLAY_COLOR_WHITE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_ng(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'N', 'G', ' ', ' ', ' ', ' '};
  char pair_line[20];

  display_lcdm_pair_line(panel_ng_out, panel_ng_in, pair_line, sizeof(pair_line));

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "RESULT",
                                "NG",
                                pair_line,
                                "",
                                FIRST_GEN_DISPLAY_COLOR_RED,
                                FIRST_GEN_DISPLAY_COLOR_RED,
                                FIRST_GEN_DISPLAY_COLOR_WHITE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void panel_display_pass_once(void)
{
  if(panel_display_state != FIRST_GEN_DISPLAY_PASS) {
    panel_ng_out = 0U;
    panel_ng_in = 0U;
    display_pass();
    panel_display_state = FIRST_GEN_DISPLAY_PASS;
    buzzer_start(FIRST_GEN_BUZZER_PATTERN_PASS);
  }
}

static void panel_display_ng_once(void)
{
  if(panel_display_state != FIRST_GEN_DISPLAY_NG) {
    display_ng();
    panel_display_state = FIRST_GEN_DISPLAY_NG;
    panel_ng_tick = 0U;
    panel_ng_phase = 0U;
    buzzer_start(FIRST_GEN_BUZZER_PATTERN_NG);
  }
}

static void panel_display_ng_service(void)
{
  if(panel_display_state != FIRST_GEN_DISPLAY_NG || panel_ng_out == 0U || panel_ng_in == 0U) {
    return;
  }

  panel_ng_tick++;
  if(panel_ng_tick < FIRST_GEN_NG_DISPLAY_PERIOD_SCANS) {
    return;
  }

  panel_ng_tick = 0U;
  panel_ng_phase ^= 1U;
  if(panel_ng_phase == 0U) {
    display_ng();
  } else {
    display_pair(panel_ng_out, panel_ng_in);
  }
}

static void panel_display_scan_step(void)
{
  uint8_t segments[FIRST_GEN_DISPLAY_DIGITS] = {0U, 0U, 0U, 0U, 0U, 0U};
  uint8_t pos = (uint8_t)(panel_scan_phase * 2U);

  if(first_gen_display_is_lcdm() != 0U) {
    display_auto_idle();
    panel_display_state = FIRST_GEN_DISPLAY_SCAN;
    return;
  }

  segments[pos] = 0x40U;
  segments[pos + 1U] = 0x40U;
  first_gen_display_write_raw6(segments);
  panel_display_state = FIRST_GEN_DISPLAY_SCAN;
}

static void panel_display_scan_service(void)
{
  if(panel_display_state == FIRST_GEN_DISPLAY_SCAN) {
    return;
  }

  display_auto_idle();
  panel_display_state = FIRST_GEN_DISPLAY_SCAN;
}

static void display_self_pass(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'0', '0', '1', '0', '0', '1'};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "RESULT",
                                "PASS",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_GREEN,
                                FIRST_GEN_DISPLAY_COLOR_GREEN,
                                FIRST_GEN_DISPLAY_COLOR_WHITE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_self_test_result_page(uint8_t page)
{
  if(page == 0U) {
    page = 1U;
  }
  if(page > 2U) {
    page = 2U;
  }

  self_test_result_page = page;
  /* 95 is an off-grid completion marker: all 94 cells become confirmed
   * green/red results and none remains in the blue running-cell colour. */
  first_gen_display_show_auto_table_page(page,
                                         (uint16_t)(FIRST_GEN_ACTIVE_POINT_COUNT + 1U));
  first_gen_display_set_k1_page_hint(1U);
}

static void display_auto_idle(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'0', '0', '1', '0', '0', '1'};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "WIRE TESTER",
                                "",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_auto_wait_for_harness(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'A', 'U', 'T', 'O', ' ', ' '};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "AUTO TESTING",
                                "WAITING HARNESS",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static uint8_t display_wait_for_any_key_ms(uint16_t duration_ms)
{
  uint16_t elapsed_ms = 0U;

  while(elapsed_ms < duration_ms) {
    if(first_gen_display_key_read_raw() != FIRST_GEN_KEY_NONE) {
      while(first_gen_display_key_read_raw() != FIRST_GEN_KEY_NONE) {
        delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
      }
      g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
      return 1U;
    }
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    elapsed_ms = (uint16_t)(elapsed_ms + FIRST_GEN_PANEL_KEY_POLL_MS);
  }

  return 0U;
}

static void display_power_on_scroll(void)
{
  static const char message[] = "   WIRE TESTER   ";
  char text[FIRST_GEN_DISPLAY_DIGITS];
  uint8_t offset;
  uint8_t i;

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "WIRE TESTER",
                                "",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  while(1) {
    for(offset = 0U; offset <= (uint8_t)(sizeof(message) - 1U - FIRST_GEN_DISPLAY_DIGITS); offset++) {
      for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
        text[i] = message[offset + i];
      }
      first_gen_display_write_text6(text);
      if(display_wait_for_any_key_ms(480U) != 0U) {
        return;
      }
    }
  }
}

static void display_learn(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'0', '0', '1', '0', '0', '1'};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "LEARNING",
                                "SCANNING",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_current_idle_state(void)
{
  if(g_first_gen_recipe_valid != 0U) {
    display_auto_idle();
  } else {
    display_learn();
  }
}

static uint8_t panel_wait_learn_confirm(void)
{
  uint8_t key;

  while(g_first_gen_learn_pending != 0U) {
    display_learn_summary_page(0U);

    for(uint16_t elapsed_ms = 0U; elapsed_ms < 500U; elapsed_ms += FIRST_GEN_PANEL_KEY_POLL_MS) {
      key = first_gen_display_key_read_raw();
      if(key == FIRST_GEN_KEY_NONE) {
        g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
        delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
        continue;
      }
      if(key == g_first_gen_last_panel_key) {
        delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
        continue;
      }

      g_first_gen_last_panel_key = key;
      if(key == FIRST_GEN_KEY_SET) {
        (void)panel_result_page_next();
        panel_wait_all_keys_released(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
        continue;
      }
      if(key == FIRST_GEN_KEY_MINUS) {
        panel_wait_all_keys_released(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
        return first_gen_4051_confirm_learn_save();
      }
      if(key == FIRST_GEN_KEY_PLUS) {
        panel_reset_to_zero();
        panel_wait_all_keys_released(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
        return 0U;
      }
      if(key == FIRST_GEN_KEY_CLEAR) {
        panel_start_auto_test();
        panel_wait_all_keys_released(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
        return 0U;
      }
    }
  }

  return 0U;
}

static void panel_wait_all_keys_released(uint16_t stable_ms)
{
  uint16_t released_ms = 0U;

  while(released_ms < stable_ms) {
    if(first_gen_display_key_read_raw() == FIRST_GEN_KEY_NONE) {
      released_ms = (uint16_t)(released_ms + FIRST_GEN_PANEL_KEY_POLL_MS);
    } else {
      released_ms = 0U;
    }
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
  }

  g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
}

static void display_print_ready(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'P', 'r', 'n', 't', ' ', ' '};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "WAITING FOR PRINTING",
                                "",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_printing(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'P', 'r', 'i', 'n', 't', 'g'};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "WAITING FOR PRINTING",
                                "START PRINTING",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_print_done(void)
{
  char text[FIRST_GEN_DISPLAY_DIGITS] = {'P', 'r', 'i', 'n', 't', 'd'};

  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "WAITING FOR PRINTING",
                                "COMPLETE",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
    return;
  }

  first_gen_display_write_text6(text);
}

static void display_print_trigger_level(uint8_t level)
{
  char text[FIRST_GEN_DISPLAY_DIGITS];

  if(level != 0U) {
    text[0] = 'r';
    text[1] = 'E';
    text[2] = 'L';
    text[3] = '-';
    text[4] = '1';
    text[5] = ' ';
  } else {
    text[0] = 'P';
    text[1] = 'r';
    text[2] = 'E';
    text[3] = 'S';
    text[4] = '0';
    text[5] = ' ';
  }
  first_gen_display_write_text6(text);
}

static void display_error_code(uint16_t point)
{
  char text[FIRST_GEN_DISPLAY_DIGITS];

  if(first_gen_display_is_lcdm() != 0U) {
    (void)point;
    return;
  }

  text[0] = 'E';
  text[1] = 'r';
  text[2] = (char)('0' + ((point / 1000U) % 10U));
  text[3] = (char)('0' + ((point / 100U) % 10U));
  text[4] = (char)('0' + ((point / 10U) % 10U));
  text[5] = (char)('0' + (point % 10U));
  first_gen_display_write_text6(text);
}

static void panel_reset_to_zero(void)
{
  if(panel_auto_enabled == 0U &&
     g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_RESET &&
     panel_waiting_for_reconnect == 0U &&
     g_first_gen_learn_pending == 0U) {
    return;
  }

  panel_operation_interrupted = 1U;
  self_test_result_ready = 0U;
  learn_confirmed_hold_active = 0U;
  panel_waiting_for_reconnect = 0U;
  panel_idle_ms = 0U;
  g_first_gen_learn_pending = 0U;
  g_first_gen_last_pass = 0U;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_RESET;
  g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
  panel_reset_scan_state();
  panel_reset_print_state();
  first_gen_display_set_auto_test_blink(0U);
  panel_auto_enabled = 0U;
  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_page("",
                                "RESET",
                                "",
                                "",
                                "",
                                FIRST_GEN_DISPLAY_COLOR_BLUE,
                                FIRST_GEN_DISPLAY_COLOR_WHITE,
                                FIRST_GEN_DISPLAY_COLOR_BLUE);
  } else {
    display_current_idle_state();
  }
}

static void panel_run_self_test(void)
{
  uint16_t point;
  uint16_t end_point = FIRST_GEN_PANEL_TEST_END;

  if(first_gen_display_is_lcdm() != 0U) {
    panel_run_lcdm_self_test();
    return;
  }

  panel_auto_enabled = 0U;
  panel_last_test_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;

  display_panel_test_progress(0U);
  for(point = FIRST_GEN_PANEL_TEST_START; point <= end_point; point++) {
    if(point <= FIRST_GEN_ACTIVE_POINT_COUNT && panel_check_open_pair(point) == 0U) {
      display_pair(point, point);
      return;
    }
    display_panel_test_progress(point);
    if(panel_priority_delay_ms(FIRST_GEN_PANEL_TEST_STEP_MS) != 0U) {
      return;
    }
  }

  display_self_pass();
  self_test_result_ready = 1U;
  self_test_result_page = 2U;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
}

static void panel_start_auto_test(void)
{
  if(panel_auto_enabled != 0U && g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_AUTO_TEST) {
    return;
  }

  panel_operation_interrupted = 1U;
  self_test_result_ready = 0U;
  learn_confirmed_hold_active = 0U;
  panel_waiting_for_reconnect = 0U;
  panel_idle_ms = 0U;
  panel_last_test_mode = FIRST_GEN_PANEL_MODE_AUTO_TEST;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_AUTO_TEST;
  g_first_gen_last_pass = 0U;
  panel_reset_scan_state();
  panel_reset_print_state();
  first_gen_display_set_auto_test_blink(0U);

  if(g_first_gen_recipe_valid == 0U) {
    panel_auto_enabled = 0U;
    display_learn();
    return;
  }

  panel_auto_enabled = 1U;
  panel_display_state = FIRST_GEN_DISPLAY_UNKNOWN;
  panel_scan_tick = FIRST_GEN_SCAN_DISPLAY_PERIOD_SCANS;
  panel_scan_phase = 2U;
  first_gen_display_clear_auto_test_lines();
  if(first_gen_display_is_lcdm() == 0U) {
    display_auto_test_pair(1U);
  }
  /* Do not draw an empty AUTO result page here.  The first physical verdict
   * owns the next LCDM update: a clean scan goes straight to PASS, and any
   * open/short goes straight to NG.  Result records are filled afterwards
   * for K1 browsing without replacing that summary. */
  /* Keep K1-K4 static throughout acquisition.  The result rows themselves
   * provide the live indication without the former flashing K2 caption. */
  panel_display_state = FIRST_GEN_DISPLAY_SCAN;
}

static void panel_handle_k1_press(void)
{
  uint16_t elapsed_ms = 0U;
  uint8_t learned = 0U;

  /* Learning, self-test, and AUTO results all use this same K1 PAGE path. */
  if(panel_handle_result_page_k1() != 0U) {
    return;
  }

  while(first_gen_display_key_read_raw() == FIRST_GEN_KEY_SET) {
    if(elapsed_ms >= FIRST_GEN_PANEL_K1_LONG_MS) {
      panel_auto_enabled = 0U;
      g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_IDLE;
      (void)first_gen_4051_learn_preview();
      learned = 1U;
      break;
    }
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    elapsed_ms = (uint16_t)(elapsed_ms + FIRST_GEN_PANEL_KEY_POLL_MS);
  }

  if(learned != 0U) {
    /* Learning is entered while K1 is still down.  Keep its original
     * release guard so that the held touch cannot operate the next screen. */
    while(first_gen_display_key_read_raw() == FIRST_GEN_KEY_SET) {
      delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    }
    delay_ms(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
    g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
    return;
  }

  /* On the idle/start page K1 must accept the very first tap.  The former
   * 80 ms minimum tap and 200 ms post-release delay could discard a normal
   * quick touch before the self-test even started.  A press/release event is
   * already latched by LCDM, so release starts the self-test immediately. */
  while(first_gen_display_key_read_raw() == FIRST_GEN_KEY_SET) {
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
  }
  g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;

  if(self_test_result_ready != 0U) {
    panel_auto_enabled = 0U;
    g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_SELF_TEST;
    (void)panel_result_page_next();
    return;
  }
  panel_run_self_test();
}

/* K3 retains its normal short-press RESET behaviour.  Only a high-end LCDM
 * tester that is fully idle can reinterpret a deliberate three-second hold as
 * entry to the maintenance page.  This keeps a running scan, learning flow,
 * print handshake, and the K1 learning long-press isolated from WiFi setup. */
static uint8_t panel_settings_allowed(void)
{
  if(first_gen_display_is_lcdm() == 0U ||
     panel_auto_enabled != 0U ||
     g_first_gen_learn_pending != 0U ||
     learn_confirmed_hold_active != 0U ||
     self_test_result_ready != 0U ||
     g_first_gen_print_state != FIRST_GEN_PRINT_STATE_IDLE ||
     panel_waiting_for_reconnect != 0U ||
     (g_first_gen_panel_mode != FIRST_GEN_PANEL_MODE_IDLE &&
      g_first_gen_panel_mode != FIRST_GEN_PANEL_MODE_RESET)) {
    return 0U;
  }

  return 1U;
}

static uint8_t panel_handle_k3_press(void)
{
  uint16_t elapsed_ms = 0U;

  if(panel_settings_allowed() == 0U) {
    panel_reset_to_zero();
    if(first_gen_display_is_lcdm() != 0U) {
      tester_settings_start_saved_wifi();
    }
    return 1U;
  }

  while(first_gen_display_key_read_raw() == FIRST_GEN_KEY_PLUS) {
    if(elapsed_ms >= FIRST_GEN_PANEL_K3_LONG_MS) {
      /* Enter as soon as the hold reaches three seconds.  The old path waited
       * for the finger to release before drawing the page, which made the
       * operator think the long-press had been missed.  Settings consumes the
       * following release event as a non-action, and leave_maintenance()
       * clears the latched key before returning to the tester page. */
      g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
      if(tester_settings_begin() != 0U) {
        return 1U;
      }
      break;
    }
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    elapsed_ms = (uint16_t)(elapsed_ms + FIRST_GEN_PANEL_KEY_POLL_MS);
  }

  /* A short K3 operation remains the established reset/retry action. */
  panel_reset_to_zero();
  if(first_gen_display_is_lcdm() != 0U) {
    tester_settings_start_saved_wifi();
  }
  return 1U;
}

static uint8_t panel_handle_key(void)
{
  uint8_t key = first_gen_display_key_read_raw();

  if(key == FIRST_GEN_KEY_NONE) {
    g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
    return 0U;
  }

  if(key == g_first_gen_last_panel_key) {
    return 0U;
  }

  g_first_gen_last_panel_key = key;
  panel_idle_ms = 0U;
  /* K1 remains the page key on the confirmed learning result.  Other keys
   * intentionally leave that hold/result state. */
  if(key != FIRST_GEN_KEY_SET) {
    learn_confirmed_hold_active = 0U;
    auto_k1_double_remaining_ms = 0U;
  }
  switch(key) {
  case FIRST_GEN_KEY_SET:
    panel_handle_k1_press();
    return 1U;

  case FIRST_GEN_KEY_CLEAR:
    panel_start_auto_test();
    return 1U;

  case FIRST_GEN_KEY_PLUS:
    (void)panel_handle_k3_press();
    return 1U;

  case FIRST_GEN_KEY_MINUS:
    (void)first_gen_4051_confirm_learn_save();
    return 1U;

  default:
    return 0U;
  }
}

static uint8_t panel_priority_key_service(void)
{
  uint8_t key = first_gen_display_key_read_raw();

  if(key == FIRST_GEN_KEY_NONE) {
    g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
    return 0U;
  }

  g_first_gen_last_panel_key = key;
  panel_idle_ms = 0U;
  if(key != FIRST_GEN_KEY_SET) {
    learn_confirmed_hold_active = 0U;
    auto_k1_double_remaining_ms = 0U;
  }
  switch(key) {
  case FIRST_GEN_KEY_SET:
    panel_handle_k1_press();
    return 1U;

  case FIRST_GEN_KEY_CLEAR:
    panel_start_auto_test();
    return 1U;

  case FIRST_GEN_KEY_PLUS:
    (void)panel_handle_k3_press();
    return 1U;

  case FIRST_GEN_KEY_MINUS:
    (void)first_gen_4051_confirm_learn_save();
    return 1U;

  default:
    return 0U;
  }
}

static uint8_t panel_priority_delay_ms(uint32_t duration_ms)
{
  uint32_t elapsed_ms = 0U;

  while(elapsed_ms < duration_ms) {
    if(panel_priority_key_service() != 0U) {
      return 1U;
    }
    delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    buzzer_service(FIRST_GEN_PANEL_KEY_POLL_MS);
    panel_auto_k1_double_tick(FIRST_GEN_PANEL_KEY_POLL_MS);
    elapsed_ms += FIRST_GEN_PANEL_KEY_POLL_MS;
  }

  return 0U;
}

static uint8_t panel_problem_live_delay_ms(uint32_t duration_ms, uint16_t problem_out)
{
  uint32_t elapsed_ms = 0U;

  while(elapsed_ms < duration_ms) {
    if(panel_priority_key_service() != 0U) {
      return PANEL_PROBLEM_INTERRUPTED;
    }
    delay_ms(FIRST_GEN_PROBLEM_RECHECK_MS);
    buzzer_service(FIRST_GEN_PROBLEM_RECHECK_MS);
    elapsed_ms += FIRST_GEN_PROBLEM_RECHECK_MS;
    if(scan_problem_row_live(problem_out) != 0U) {
      return PANEL_PROBLEM_RESTORED;
    }
  }

  return PANEL_PROBLEM_CONTINUE;
}

static uint8_t panel_show_problem_blink_once(uint16_t problem_out, uint16_t problem_in)
{
  uint8_t status;

  display_pair(problem_out, problem_in);
  status = panel_problem_live_delay_ms(FIRST_GEN_SHORT_BLINK_MS, problem_out);
  if(status != PANEL_PROBLEM_CONTINUE) {
    return status;
  }

  first_gen_display_clear();
  return panel_problem_live_delay_ms(FIRST_GEN_SHORT_BLINK_MS, problem_out);
}

/* The first NG must reach the red summary immediately.  Do not scan another
 * full OUT row or rebuild LCDM records here.  The following background pass
 * gathers every fault and publishes one complete result-page map.  That map
 * stays unchanged until the next complete pass, so I002 and I003 always
 * blink in the same phase. */
static void auto_result_capture_live_fault(uint16_t problem_out,
                                           uint16_t problem_in,
                                           uint8_t problem_type)
{
  uint8_t word;
  uint8_t bit;

  if(auto_result_ready == 0U || auto_result_fault_collecting != 0U ||
     first_gen_display_is_lcdm() == 0U ||
     problem_out == 0U || problem_out > FIRST_GEN_ACTIVE_POINT_COUNT ||
     problem_in == 0U || problem_in > FIRST_GEN_ACTIVE_POINT_COUNT ||
     problem_type == FIRST_GEN_PROBLEM_NONE) {
    return;
  }

  /* A complete fault map is authoritative for K1.  Leave it in place while
   * the monitor rediscovers the same earliest NG; the next full background
   * scan will replace it only if the physical fault set really changes. */
  if(auto_result_cached_fault_type != FIRST_GEN_PROBLEM_NONE) {
    return;
  }

  if(problem_type == FIRST_GEN_PROBLEM_SHORT) {
    word = (uint8_t)((problem_in - 1U) >> 5);
    bit = (uint8_t)((problem_in - 1U) & 0x1FU);
    auto_result_matrix[problem_out - 1U][word] |= (1UL << bit);
  }

  /* Do not send any LCDM result-page commands here.  The NG summary is now
   * free to appear immediately; the following complete scan publishes every
   * fault group to K1 together in one update. */
}

static void panel_record_current_problem(uint16_t problem_out, uint16_t problem_in, uint8_t problem_type)
{
  auto_result_capture_live_fault(problem_out, problem_in, problem_type);
  g_first_gen_current_out = problem_out;
  g_first_gen_current_problem_in = problem_in;
  g_first_gen_first_fail_out = problem_out;
  g_first_gen_first_fail_in = problem_in;
  current_problem_type = problem_type;
  if(scan_cycle_has_problem == 0U || panel_ng_out == 0U || panel_ng_in == 0U) {
    panel_ng_out = problem_out;
    panel_ng_in = problem_in;
    panel_ng_tick = FIRST_GEN_NG_DISPLAY_PERIOD_SCANS;
    panel_ng_phase = 1U;
  }
  g_first_gen_last_pass = 0U;
  panel_reset_print_state();
  waiting_on_error = 1U;
}

static void panel_hold_current_problem(void)
{
  uint16_t problem_out;
  uint16_t problem_in;
  uint8_t status;

  waiting_on_error = 1U;
  if(current_problem_type == FIRST_GEN_PROBLEM_SHORT) {
    status = panel_show_problem_blink_once(g_first_gen_current_out,
                                           g_first_gen_current_problem_in);
    if(status == PANEL_PROBLEM_RESTORED) {
      current_problem_type = FIRST_GEN_PROBLEM_NONE;
      g_first_gen_first_fail_out = 0U;
      g_first_gen_first_fail_in = 0U;
      g_first_gen_current_problem_in = 0U;
      waiting_on_error = 0U;
      scan_out_point = 1U;
    }
    return;
  }

  if(expected_harness_find_next_problem(g_first_gen_current_out,
                                        g_first_gen_current_problem_in,
                                        &problem_out,
                                        &problem_in) == 0U &&
     expected_harness_find_next_problem(1U, 1U, &problem_out, &problem_in) == 0U) {
    g_first_gen_first_fail_out = 0U;
    g_first_gen_first_fail_in = 0U;
    g_first_gen_current_problem_in = 0U;
    current_problem_type = FIRST_GEN_PROBLEM_NONE;
    waiting_on_error = 0U;
    scan_out_point = 1U;
    return;
  }

  panel_record_current_problem(problem_out, problem_in, FIRST_GEN_PROBLEM_MISSING);
  display_pair(problem_out, problem_in);
}

static uint8_t recipe_flash_load(void)
{
  const first_gen_recipe_image_t *stored = (const first_gen_recipe_image_t *)FIRST_GEN_RECIPE_FLASH_ADDR;

  if(stored->magic != FIRST_GEN_RECIPE_MAGIC ||
     stored->version != FIRST_GEN_RECIPE_VERSION ||
     stored->point_count != FIRST_GEN_ACTIVE_POINT_COUNT ||
     stored->words_per_row != IO_SCAN_MATRIX_WORDS) {
    return 0U;
  }

  matrix_copy(expected_matrix, stored->matrix);
  if(matrix_crc32(expected_matrix) != stored->matrix_crc) {
    matrix_clear(expected_matrix);
    return 0U;
  }

  g_first_gen_learn_counter = stored->learn_counter;
  return 1U;
}

static uint8_t recipe_flash_save(void)
{
  const uint32_t *words;
  uint32_t address;
  uint32_t i;
  uint32_t word_count;
  flash_status_type status;

  recipe_image.magic = FIRST_GEN_RECIPE_MAGIC;
  recipe_image.version = FIRST_GEN_RECIPE_VERSION;
  recipe_image.point_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  recipe_image.words_per_row = IO_SCAN_MATRIX_WORDS;
  recipe_image.matrix_crc = matrix_crc32(expected_matrix);
  recipe_image.learn_counter = g_first_gen_learn_counter + 1U;
  recipe_image.reserved[0] = 0U;
  recipe_image.reserved[1] = 0U;
  matrix_copy(recipe_image.matrix, expected_matrix);

  flash_unlock();
  status = flash_sector_erase(FIRST_GEN_RECIPE_FLASH_ADDR);
  if(status == FLASH_OPERATE_DONE) {
    words = (const uint32_t *)&recipe_image;
    word_count = (uint32_t)(sizeof(recipe_image) / sizeof(uint32_t));
    for(i = 0U; i < word_count; i++) {
      address = FIRST_GEN_RECIPE_FLASH_ADDR + (i * sizeof(uint32_t));
      status = flash_word_program(address, words[i]);
      if(status != FLASH_OPERATE_DONE) {
        break;
      }
    }
  }
  flash_lock();

  if(status != FLASH_OPERATE_DONE) {
    return 0U;
  }

  g_first_gen_learn_counter = recipe_image.learn_counter;
  return recipe_flash_load();
}

static void scan_result_set_bit(uint16_t out_point, uint16_t in_point)
{
  uint8_t word = (uint8_t)((in_point - 1U) >> 5);
  uint8_t bit = (uint8_t)((in_point - 1U) & 0x1FU);

  scan_result.matrix[out_point - 1U][word] |= (1UL << bit);
}

static uint8_t scan_one_row(uint16_t out_point)
{
  uint16_t in_point;
  io_scan_pair_result_t pair;

  if(io_scan_begin_out_row(IO_POS_OUT(out_point)) != IO_SCAN_OK) {
    io_mux_disable_all();
    return 0U;
  }

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(io_scan_read_selected_out_in(IO_POS_IN(in_point), &pair) != IO_SCAN_OK) {
      io_mux_disable_all();
      return 0U;
    }
    scan_result.scanned_pairs++;
    if(pair.connected != 0U) {
      scan_result_set_bit(out_point, in_point);
      scan_result.connected_pairs++;
    }
  }

  return 1U;
}

/* Self-test and learning deliberately share this acquisition path.  It
 * contains no display operation, so moving from OUT047 to OUT048 never waits
 * for a second LCDM grid page to be drawn. */
static uint8_t panel_scan_full_matrix(void)
{
  io_scan_init(IO_SCAN_PROFILE_FIRST_GEN_1TH);
  io_scan_clear_result(&scan_result);
  scan_result.profile_id = IO_SCAN_PROFILE_FIRST_GEN_1TH;
  scan_result.out_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.in_count = FIRST_GEN_ACTIVE_POINT_COUNT;

  for(scan_out_point = 1U;
      scan_out_point <= FIRST_GEN_ACTIVE_POINT_COUNT;
      scan_out_point++) {
    scan_result.active_out_pos = IO_POS_OUT(scan_out_point);
    if(scan_one_row(scan_out_point) == 0U) {
      io_mux_disable_all();
      scan_out_point = 1U;
      return 0U;
    }
  }

  io_mux_disable_all();
  scan_out_point = 1U;
  return 1U;
}

static uint8_t find_row_problem(uint16_t out_point, uint16_t *problem_in, uint8_t *problem_type)
{
  uint8_t word;
  uint8_t bit;
  uint32_t expected;
  uint32_t actual;
  uint32_t missing;
  uint32_t unexpected;

  for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
    expected = expected_matrix[out_point - 1U][word];
    actual = scan_result.matrix[out_point - 1U][word];
    missing = expected & ~actual;
    unexpected = actual & ~expected;

    if(missing != 0U) {
      bit = first_set_bit_1_based(missing);
      *problem_in = (uint16_t)((word * 32U) + bit);
      *problem_type = 1U;
      return 1U;
    }
    if(unexpected != 0U) {
      bit = first_set_bit_1_based(unexpected);
      *problem_in = (uint16_t)((word * 32U) + bit);
      *problem_type = 2U;
      return 1U;
    }
  }

  *problem_in = 0U;
  *problem_type = 0U;
  return 0U;
}

static uint8_t scan_problem_row_live(uint16_t out_point)
{
  uint16_t problem_in;
  uint8_t problem_type;

  if(out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
    return 0U;
  }

  io_scan_clear_result(&scan_result);
  scan_result.profile_id = IO_SCAN_PROFILE_FIRST_GEN_1TH;
  scan_result.out_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.in_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.active_out_pos = IO_POS_OUT(out_point);

  if(scan_one_row(out_point) == 0U) {
    panel_record_current_problem(out_point, 1U, FIRST_GEN_PROBLEM_MISSING);
    return 0U;
  }
  g_first_gen_last_connected_pairs = scan_result.connected_pairs;

  if(find_row_problem(out_point, &problem_in, &problem_type) != 0U) {
    panel_record_current_problem(out_point, problem_in, problem_type);
    return 0U;
  }

  return 1U;
}

static uint8_t expected_harness_has_connection(void)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;
  io_scan_pair_result_t pair;

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((expected_matrix[out_point - 1U][word] & (1UL << bit)) == 0U) {
        continue;
      }
      if(io_scan_read_pair(IO_POS_OUT(out_point), IO_POS_IN(in_point), &pair) != IO_SCAN_OK) {
        io_mux_disable_all();
        return 0U;
      }
      if(pair.connected != 0U) {
        io_mux_disable_all();
        return 1U;
      }
    }
  }

  io_mux_disable_all();
  return 0U;
}

static uint8_t expected_harness_check_first_problem(uint16_t *problem_out, uint16_t *problem_in)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;
  io_scan_pair_result_t pair;

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((expected_matrix[out_point - 1U][word] & (1UL << bit)) == 0U) {
        continue;
      }
      /* PASS used to verify the entire learned matrix in one uninterrupted
       * block.  A K1 event arriving during that block sat in the LCDM queue
       * until the whole pass ended, which made PAGE feel much slower in PASS
       * than in NG.  Service the key between individual electrical reads. */
      if(panel_handle_key() != 0U) {
        io_mux_disable_all();
        return FIRST_GEN_SCAN_RESULT_INTERRUPTED;
      }
      if(io_scan_read_pair(IO_POS_OUT(out_point), IO_POS_IN(in_point), &pair) != IO_SCAN_OK ||
         pair.connected == 0U) {
        io_mux_disable_all();
        *problem_out = out_point;
        *problem_in = in_point;
        return FIRST_GEN_SCAN_RESULT_FAIL;
      }
    }
  }

  io_mux_disable_all();
  *problem_out = 0U;
  *problem_in = 0U;
  return FIRST_GEN_SCAN_RESULT_PASS;
}

/* A completed NG record remembers every non-learned connection that was
 * actually present.  On recovery, recheck only those previous short points
 * before allowing the quick PASS path.  This is normally one or a few reads,
 * unlike a full 94 x 94 sparse sweep.  It prevents an unchanged short from
 * flashing PASS on every monitor frame, while allowing a repaired NG to turn
 * green immediately and leaving discovery of new, unrelated shorts to the
 * background sweep. */
static uint8_t auto_result_check_known_shorts(uint16_t *problem_out,
                                              uint16_t *problem_in)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;
  uint8_t row_started;
  uint32_t known_shorts;
  io_scan_pair_result_t pair;

  if(problem_out == 0 || problem_in == 0) {
    return FIRST_GEN_SCAN_RESULT_FAIL;
  }

  for(out_point = 1U; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    row_started = 0U;
    for(word = 0U; word < IO_SCAN_MATRIX_WORDS; word++) {
      known_shorts = auto_result_record_matrix[out_point - 1U][word] &
                     ~expected_matrix[out_point - 1U][word];
      if(known_shorts == 0U) {
        continue;
      }

      for(bit = 0U; bit < 32U; bit++) {
        if((known_shorts & (1UL << bit)) == 0U) {
          continue;
        }
        in_point = (uint16_t)((word * 32U) + bit + 1U);
        if(in_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
          continue;
        }
        if(panel_handle_key() != 0U) {
          io_mux_disable_all();
          return FIRST_GEN_SCAN_RESULT_INTERRUPTED;
        }
        if(row_started == 0U) {
          if(io_scan_begin_out_row(IO_POS_OUT(out_point)) != IO_SCAN_OK) {
            io_mux_disable_all();
            *problem_out = out_point;
            *problem_in = in_point;
            return FIRST_GEN_SCAN_RESULT_FAIL;
          }
          row_started = 1U;
        }
        if(io_scan_read_selected_out_in(IO_POS_IN(in_point), &pair) != IO_SCAN_OK ||
           pair.connected != 0U) {
          io_mux_disable_all();
          *problem_out = out_point;
          *problem_in = in_point;
          return FIRST_GEN_SCAN_RESULT_FAIL;
        }
      }
    }
  }

  io_mux_disable_all();
  *problem_out = 0U;
  *problem_in = 0U;
  return FIRST_GEN_SCAN_RESULT_PASS;
}

static uint8_t expected_harness_find_next_problem(uint16_t start_out,
                                                  uint16_t start_in,
                                                  uint16_t *problem_out,
                                                  uint16_t *problem_in)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;
  io_scan_pair_result_t pair;

  for(out_point = start_out; out_point <= FIRST_GEN_ACTIVE_POINT_COUNT; out_point++) {
    for(in_point = (out_point == start_out) ? start_in : 1U;
        in_point <= FIRST_GEN_ACTIVE_POINT_COUNT;
        in_point++) {
      word = (uint8_t)((in_point - 1U) >> 5);
      bit = (uint8_t)((in_point - 1U) & 0x1FU);
      if((expected_matrix[out_point - 1U][word] & (1UL << bit)) == 0U) {
        continue;
      }
      if(io_scan_read_pair(IO_POS_OUT(out_point), IO_POS_IN(in_point), &pair) != IO_SCAN_OK ||
         pair.connected == 0U) {
        io_mux_disable_all();
        *problem_out = out_point;
        *problem_in = in_point;
        return 1U;
      }
    }
  }

  io_mux_disable_all();
  return 0U;
}

static uint8_t scan_and_check_current_row(void)
{
  uint16_t problem_in;
  uint8_t problem_type;

  io_scan_clear_result(&scan_result);
  scan_result.profile_id = IO_SCAN_PROFILE_FIRST_GEN_1TH;
  scan_result.out_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.in_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.active_out_pos = IO_POS_OUT(scan_out_point);

  g_first_gen_current_out = scan_out_point;
  g_first_gen_current_problem_in = 0U;

  if(scan_one_row(scan_out_point) == 0U) {
    panel_record_current_problem(scan_out_point, 1U, FIRST_GEN_PROBLEM_MISSING);
    g_first_gen_print_blocked_counter++;
    return 0U;
  }
  g_first_gen_last_connected_pairs = scan_result.connected_pairs;

  if(find_row_problem(scan_out_point, &problem_in, &problem_type) != 0U) {
    if(problem_type == 1U) {
      g_first_gen_missing_counter++;
    } else {
      g_first_gen_unexpected_counter++;
    }
    panel_record_current_problem(scan_out_point, problem_in, problem_type);
    g_first_gen_print_blocked_counter++;
    return 0U;
  }

  return 1U;
}

/* After the compact learned-edge precheck has succeeded, scan only matrix
 * positions that were not learned.  A connection at any of these positions
 * is an immediate short/extra-wire NG.  Learned edges are deliberately not
 * read again in this phase: they were just verified first, which makes an
 * open-circuit response proportional to the learned cable size rather than
 * the fixed 94 x 94 matrix size. */
static uint8_t scan_and_check_unlearned_row(void)
{
  uint16_t in_point;
  uint8_t word;
  uint8_t bit;
  uint8_t row_started = 0U;
  io_scan_pair_result_t pair;

  io_scan_clear_result(&scan_result);
  scan_result.profile_id = IO_SCAN_PROFILE_FIRST_GEN_1TH;
  scan_result.out_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.in_count = FIRST_GEN_ACTIVE_POINT_COUNT;
  scan_result.active_out_pos = IO_POS_OUT(scan_out_point);

  g_first_gen_current_out = scan_out_point;
  g_first_gen_current_problem_in = 0U;

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    word = (uint8_t)((in_point - 1U) >> 5);
    bit = (uint8_t)((in_point - 1U) & 0x1FU);
    if((expected_matrix[scan_out_point - 1U][word] & (1UL << bit)) != 0U) {
      continue;
    }

    /* Keep K1 responsive without querying the LCDM event queue at every one
     * of 8,836 pair reads.  A check every sixteen inputs preserves touch
     * response while removing that scan-path overhead. */
    if((((uint16_t)(in_point - 1U) % FIRST_GEN_UNLEARNED_KEY_POLL_INTERVAL) == 0U) &&
       panel_handle_key() != 0U) {
      io_mux_disable_all();
      return FIRST_GEN_SCAN_RESULT_INTERRUPTED;
    }

    if(row_started == 0U) {
      if(io_scan_begin_out_row(IO_POS_OUT(scan_out_point)) != IO_SCAN_OK) {
        io_mux_disable_all();
        panel_record_current_problem(scan_out_point, in_point, FIRST_GEN_PROBLEM_MISSING);
        g_first_gen_print_blocked_counter++;
        return FIRST_GEN_SCAN_RESULT_FAIL;
      }
      row_started = 1U;
    }

    if(io_scan_read_selected_out_in(IO_POS_IN(in_point), &pair) != IO_SCAN_OK) {
      io_mux_disable_all();
      panel_record_current_problem(scan_out_point, in_point, FIRST_GEN_PROBLEM_MISSING);
      g_first_gen_print_blocked_counter++;
      return FIRST_GEN_SCAN_RESULT_FAIL;
    }

    scan_result.scanned_pairs++;
    if(pair.connected != 0U) {
      scan_result_set_bit(scan_out_point, in_point);
      scan_result.connected_pairs++;
      g_first_gen_last_connected_pairs = scan_result.connected_pairs;
      g_first_gen_unexpected_counter++;
      panel_record_current_problem(scan_out_point, in_point, FIRST_GEN_PROBLEM_SHORT);
      g_first_gen_print_blocked_counter++;
      return FIRST_GEN_SCAN_RESULT_FAIL;
    }
  }

  g_first_gen_last_connected_pairs = scan_result.connected_pairs;
  return FIRST_GEN_SCAN_RESULT_PASS;
}

static void first_gen_print_link_init(void)
{
  /* LCDM high-end tester print traffic is exclusively PC3/PB9 WiFi UART.
   * The legacy PB6/PB7 IR machinery remains in the separate IR_PRINT_BRIDGE
   * application and is intentionally not initialized by this local tester. */
  tester_wifi_print_init();
}

static void panel_hold_print_error(void)
{
  g_first_gen_print_error_counter++;
  g_first_gen_print_waiting_for_wifi = 0U;
  g_first_gen_print_done = 0U;
  g_first_gen_print_state = FIRST_GEN_PRINT_STATE_ERROR;
  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_print_progress(FIRST_GEN_PRINT_DISPLAY_ERROR);
  } else {
    display_error_code(3U);
  }
}

static void panel_hold_pass_until_restart(void)
{
  panel_arm_print_workflow();
  g_first_gen_last_pass = 1U;
  panel_display_pass_once();

  if(first_gen_display_is_lcdm() == 0U) {
    display_print_ready();
  }
}

static void panel_arm_print_workflow(void)
{
  if(g_first_gen_recipe_valid == 0U || g_first_gen_last_pass == 0U ||
     g_first_gen_print_state != FIRST_GEN_PRINT_STATE_IDLE) {
    return;
  }

  g_first_gen_print_ready = 1U;
  g_first_gen_print_done = 0U;
  g_first_gen_print_waiting_for_wifi = 0U;
  g_first_gen_print_state = FIRST_GEN_PRINT_STATE_WAIT_HALL;
  print_hall_active_ms = 0U;
  print_event_id = 0U;
}

static uint8_t panel_service_print_event(uint16_t elapsed_ms)
{
  tester_wifi_print_event_t event;

  if(g_first_gen_print_state == FIRST_GEN_PRINT_STATE_IDLE) {
    return 0U;
  }
  if(g_first_gen_print_state == FIRST_GEN_PRINT_STATE_ERROR) {
    /* Keep the failed product locked in view.  The operator's established K3
     * reset/retry action clears this state and retains the learned recipe. */
    return 1U;
  }

  if(g_first_gen_print_state == FIRST_GEN_PRINT_STATE_WAIT_HALL) {
    /* Saved AP credentials are applied through ESP-AT in the background at
     * startup.  Never inject a print JSON frame into that raw AT exchange. */
    if(tester_settings_wifi_is_busy() != 0U) {
      return 0U;
    }
    if(g_first_gen_hall_active == 0U) {
      print_hall_active_ms = 0U;
      return 0U;
    }

    if(print_hall_active_ms < FIRST_GEN_HALL_DEBOUNCE_MS) {
      uint32_t next_ms = (uint32_t)print_hall_active_ms + elapsed_ms;
      print_hall_active_ms = (next_ms > FIRST_GEN_HALL_DEBOUNCE_MS) ?
                             FIRST_GEN_HALL_DEBOUNCE_MS : (uint16_t)next_ms;
      if(print_hall_active_ms < FIRST_GEN_HALL_DEBOUNCE_MS) {
        return 0U;
      }
    }

    /* PDF 第五部分第 4 项：Hall 有效后先完整显示 START PRINTING，
     * 然后只通过独立 WiFi 链路向打印控制器提交本次 PASS。 */
    print_event_sequence++;
    if(print_event_sequence == 0U) {
      print_event_sequence = 1U;
    }
    print_event_id = print_event_sequence;
    print_test_count++;
    g_first_gen_print_trigger_count++;
    if(first_gen_display_is_lcdm() != 0U) {
      first_gen_display_show_print_progress(FIRST_GEN_PRINT_DISPLAY_START);
    } else {
      display_printing();
    }
    if(tester_wifi_print_request(print_event_id,
                                 print_test_count,
                                 g_first_gen_learn_out_count,
                                 (uint16_t)g_first_gen_learn_connected_pairs) == 0U) {
      panel_hold_print_error();
      print_hall_active_ms = 0U;
      return 1U;
    }

    g_first_gen_print_request_counter++;
    g_first_gen_print_state = FIRST_GEN_PRINT_STATE_WAIT_WIFI_ACK;
    g_first_gen_print_waiting_for_wifi = 1U;
    return 1U;
  }

  if(g_first_gen_print_state == FIRST_GEN_PRINT_STATE_WAIT_WIFI_ACK) {
    event = tester_wifi_print_poll_event(print_event_id);
    if(event == TESTER_WIFI_PRINT_EVENT_ACK_QUEUED) {
      g_first_gen_print_ack_counter++;
      g_first_gen_print_state = FIRST_GEN_PRINT_STATE_WAIT_WIFI_DONE;
      return 1U;
    }
    if(event == TESTER_WIFI_PRINT_EVENT_ERROR) {
      panel_hold_print_error();
      return 1U;
    }
    /* A DONE received before the queue acknowledgement still proves that
     * the host accepted and printed this exact event. */
    if(event != TESTER_WIFI_PRINT_EVENT_DONE) {
      return 1U;
    }
  } else if(g_first_gen_print_state == FIRST_GEN_PRINT_STATE_WAIT_WIFI_DONE) {
    event = tester_wifi_print_poll_event(print_event_id);
    if(event == TESTER_WIFI_PRINT_EVENT_ERROR) {
      panel_hold_print_error();
      return 1U;
    }
    if(event != TESTER_WIFI_PRINT_EVENT_DONE) {
      return 1U;
    }
  } else {
    return 0U;
  }

  g_first_gen_print_waiting_for_wifi = 0U;
  g_first_gen_print_done_counter++;
  g_first_gen_print_done = 1U;
  g_first_gen_print_state = FIRST_GEN_PRINT_STATE_WAIT_REMOVE;
  printed_hold_active = 1U;
  printed_hold_out = 1U;
  if(first_gen_display_is_lcdm() != 0U) {
    first_gen_display_show_print_progress(FIRST_GEN_PRINT_DISPLAY_COMPLETE);
  } else {
    display_print_done();
  }
  return 1U;
}

/* The product is considered removed only after a complete reverse AUTO
 * sweep sees every OUT/IN combination open.  Finding any connection resets
 * the verification to OUT001; a fully unplugged fixture must pass all 94
 * rows before the next product can autostart. */
static uint8_t panel_all_connections_open_for_out(uint16_t out_point)
{
  uint16_t in_point;
  io_scan_pair_result_t pair;

  if(out_point == 0U || out_point > FIRST_GEN_ACTIVE_POINT_COUNT ||
     io_scan_begin_out_row(IO_POS_OUT(out_point)) != IO_SCAN_OK) {
    io_mux_disable_all();
    return 0U;
  }

  for(in_point = 1U; in_point <= FIRST_GEN_ACTIVE_POINT_COUNT; in_point++) {
    if(io_scan_read_selected_out_in(IO_POS_IN(in_point), &pair) != IO_SCAN_OK ||
       pair.connected != 0U) {
      io_mux_disable_all();
      return 0U;
    }
  }

  io_mux_disable_all();
  return 1U;
}

static uint8_t panel_printed_hold_service(void)
{
  if(printed_hold_active == 0U ||
     g_first_gen_print_state != FIRST_GEN_PRINT_STATE_WAIT_REMOVE) {
    return 0U;
  }

  if(panel_all_connections_open_for_out(printed_hold_out) == 0U) {
    /* One remaining learned or unexpected connection means the product has
     * not been removed.  Restart from the lowest OUT so the final all-open
     * confirmation is a complete, contiguous sweep. */
    printed_hold_out = 1U;
    return 1U;
  }

  printed_hold_out++;
  if(printed_hold_out > FIRST_GEN_ACTIVE_POINT_COUNT) {
    printed_hold_active = 0U;
    printed_hold_out = 1U;
    g_first_gen_print_ready = 0U;
    g_first_gen_print_state = FIRST_GEN_PRINT_STATE_IDLE;
    panel_auto_enabled = 0U;
    panel_waiting_for_reconnect = 1U;
    g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_AUTO_TEST;
    display_auto_wait_for_harness();
  }

  return 1U;
}

void first_gen_4051_scan_init(void)
{
  panel_monitor_timebase_init();
  scan_signal_gpio_init();
  first_gen_display_init();
  device_config_init();
  first_gen_print_link_init();
  tester_settings_init();
  if(first_gen_display_is_lcdm() != 0U) {
    tester_settings_start_saved_wifi();
  }
  io_scan_init(IO_SCAN_PROFILE_FIRST_GEN_1TH);
  io_scan_clear_result(&scan_result);
  matrix_clear(expected_matrix);
  g_first_gen_recipe_valid = recipe_flash_load();
  if(g_first_gen_recipe_valid != 0U) {
    g_first_gen_learn_out_count = count_nonzero_rows(expected_matrix);
    g_first_gen_learn_in_count = count_nonzero_inputs(expected_matrix);
    g_first_gen_learn_connected_pairs = (uint32_t)g_first_gen_learn_out_count +
                                        (uint32_t)g_first_gen_learn_in_count;
  }
  g_first_gen_print_ready = 0U;
  g_first_gen_print_waiting_for_wifi = 0U;
  g_first_gen_pass_hold_active = 0U;
  g_first_gen_print_done = 0U;
  g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_IDLE;
  g_first_gen_last_panel_key = FIRST_GEN_KEY_NONE;
  panel_auto_enabled = 0U;
  panel_last_test_mode = 0U;
  panel_waiting_for_reconnect = 0U;
  panel_reset_scan_state();
  panel_reset_print_state();
  learn_confirmed_hold_active = 0U;
  g_first_gen_print_trigger_level = io_print_trigger_level_read();
  g_first_gen_hall_active = (g_first_gen_print_trigger_level == 0U) ? 1U : 0U;
  first_gen_display_set_hall_input(g_first_gen_hall_active);
  /* The header represents the WiFi/AP link itself.  TCP print availability
   * is a separate production capability and must not make a healthy AP look
   * offline while the print-host fields are still being configured. */
  first_gen_display_set_wifi_connected(tester_wifi_print_is_ap_connected());

#if FIRST_GEN_TRIGGER_DIAG_ONLY
  g_first_gen_print_trigger_level = io_print_trigger_level_read();
  display_print_trigger_level(g_first_gen_print_trigger_level);
  return;
#endif

  display_power_on_scroll();

  if(g_first_gen_recipe_valid != 0U) {
    display_auto_idle();
  } else {
    display_learn();
  }
}

uint8_t first_gen_4051_scan_once(void)
{
  uint8_t pass;

  if(g_first_gen_recipe_valid == 0U) {
    g_first_gen_last_pass = 0U;
    g_first_gen_print_ready = 0U;
    display_learn();
    return 0U;
  }

  g_first_gen_missing_counter = 0U;
  g_first_gen_unexpected_counter = 0U;
  g_first_gen_first_fail_out = 0U;
  g_first_gen_first_fail_in = 0U;
  scan_out_point = 1U;
  waiting_on_error = 0U;

  do {
    pass = scan_and_check_current_row();
    if(pass == 0U) {
      return 0U;
    }
    scan_out_point++;
  } while(scan_out_point <= FIRST_GEN_ACTIVE_POINT_COUNT);

  g_first_gen_scan_counter++;
  g_first_gen_last_pass = 1U;
  g_first_gen_print_ready = 1U;
  display_pass();
  scan_out_point = 1U;
  return 1U;
}

static uint8_t first_gen_4051_learn_preview(void)
{
  learn_result_page = 1U;
  first_gen_display_set_auto_test_blink(0U);
  display_learn();
  learn_confirmed_hold_active = 0U;
  g_first_gen_learn_pending = 0U;

  if(panel_scan_full_matrix() == 0U) {
    g_first_gen_learn_status = 2U;
    display_error_code(1U);
    return 0U;
  }

  g_first_gen_last_connected_pairs = scan_result.connected_pairs;
  g_first_gen_learn_out_count = count_nonzero_rows(scan_result.matrix);
  g_first_gen_learn_in_count = count_nonzero_inputs(scan_result.matrix);
  g_first_gen_learn_connected_pairs = (uint32_t)g_first_gen_learn_out_count +
                                      (uint32_t)g_first_gen_learn_in_count;
  lcdm_apply_learn_component_colors();

  if(g_first_gen_learn_connected_pairs == 0U) {
    g_first_gen_recipe_valid = 0U;
    g_first_gen_print_ready = 0U;
    g_first_gen_learn_pending = 1U;
    g_first_gen_learn_status = 5U;
    return panel_wait_learn_confirm();
  }

  if(scan_result.connected_pairs > FIRST_GEN_LEARN_MAX_CONNECTED_PAIRS) {
    g_first_gen_recipe_valid = 0U;
    g_first_gen_print_ready = 0U;
    g_first_gen_learn_status = 4U;
    display_learn_summary_page(0U);
    return 0U;
  }

  g_first_gen_learn_pending = 1U;
  g_first_gen_learn_status = 5U;
  return panel_wait_learn_confirm();
}

static uint8_t first_gen_4051_confirm_learn_save(void)
{
  if(g_first_gen_learn_pending == 0U) {
    return 0U;
  }

  matrix_copy(expected_matrix, scan_result.matrix);
  if(recipe_flash_save() == 0U) {
    g_first_gen_recipe_valid = 0U;
    g_first_gen_print_ready = 0U;
    g_first_gen_learn_pending = 0U;
    g_first_gen_learn_status = 3U;
    display_error_code(2U);
    return 0U;
  }

  g_first_gen_recipe_valid = 1U;
  g_first_gen_learn_status = 1U;
  g_first_gen_last_pass = 1U;
  g_first_gen_print_ready = 0U;
  g_first_gen_learn_pending = 0U;
  scan_out_point = 1U;
  waiting_on_error = 0U;
  panel_auto_enabled = 0U;
  learn_confirmed_hold_active = 1U;
  learn_confirmed_blink_tick = 0U;
  display_learn_summary_page(1U);
  panel_wait_all_keys_released(FIRST_GEN_PANEL_K1_RELEASE_GUARD_MS);
  return 1U;
}

uint8_t first_gen_4051_learn_current_harness(void)
{
  return first_gen_4051_learn_preview();
}

void first_gen_4051_scan_service(void)
{
  uint8_t pass;
  uint8_t expected_check;
  uint8_t known_short_check;
  uint8_t priority_monitor;
  uint8_t learned_precheck_failed;
  uint16_t learned_problem_out;
  uint16_t learned_problem_in;
  uint16_t monitor_elapsed_ms;

  buzzer_service(1U);
  monitor_elapsed_ms = panel_monitor_elapsed_ms();

  /* Settings owns the LCDM event queue while active.  The normal tester
   * deliberately leaves every non-K1..K4 coordinate inert, and must not draw
   * Hall/status changes across the maintenance page. */
  if(tester_settings_is_active() != 0U) {
    tester_settings_service();
    if(tester_settings_take_reset_request() != 0U) {
      /* A K3 cancel/retry must redraw the ordinary tester page even when the
       * prior normal state was already RESET. */
      first_gen_display_leave_maintenance();
      g_first_gen_panel_mode = FIRST_GEN_PANEL_MODE_IDLE;
      panel_reset_to_zero();
      /* K3 is the available recovery action on this PCB: there is no MCU
       * wired to ESP EN.  Always restart the saved ESP-AT TCP session after
       * returning so an offline print host gets an immediate retry. */
      tester_settings_start_saved_wifi();
    }
    return;
  }

  tester_settings_network_service(monitor_elapsed_ms);

  /* PB8 is low-active.  Sampling it here makes the persistent LCDM header
   * change only on a physical Hall transition; the display backend caches
   * the result so normal 4051 scans send no header refresh traffic. */
  g_first_gen_print_trigger_level = io_print_trigger_level_read();
  g_first_gen_hall_active = (g_first_gen_print_trigger_level == 0U) ? 1U : 0U;
  first_gen_display_set_hall_input(g_first_gen_hall_active);
  /* Show AP association independently of the optional print-host TCP link. */
  first_gen_display_set_wifi_connected(tester_wifi_print_is_ap_connected());

  /* Advance the K1 double-tap window before checking a new touch.  PASS uses
   * a long uninterrupted verification sweep, and a PAGE touch deliberately
   * exits that sweep early.  Advancing the timer only at the former end of
   * the sweep left its 160 ms state armed for the next physical tap, so that
   * tap was misread as a double-tap instead of the next page. */
  panel_auto_k1_double_tick(monitor_elapsed_ms);
  panel_auto_result_summary_service(monitor_elapsed_ms);

#if FIRST_GEN_TRIGGER_DIAG_ONLY
  g_first_gen_print_trigger_level = io_print_trigger_level_read();
  display_print_trigger_level(g_first_gen_print_trigger_level);
  delay_ms(100U);
  return;
#endif

  if(panel_handle_key() != 0U) {
    return;
  }

  /* A just-completed verdict has already drawn PASS or NG.  Build the K1
   * record cache on this subsequent service pass, without replacing that
   * summary page. */
  auto_result_complete_pending_cache();

  /* K1 has priority over the result-page fault animation. */
  panel_auto_result_fault_browse_service(monitor_elapsed_ms);

  panel_display_ng_service();

  if(panel_service_print_event(monitor_elapsed_ms) != 0U) {
    return;
  }

  if(panel_printed_hold_service() != 0U) {
    return;
  }

  if(panel_waiting_for_reconnect != 0U) {
    if(g_first_gen_recipe_valid != 0U && expected_harness_has_connection() != 0U) {
      panel_start_auto_test();
      return;
    }
    (void)panel_priority_delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
    return;
  }

  if(panel_auto_enabled == 0U) {
    if(learn_confirmed_hold_active != 0U) {
      if(learn_confirmed_blink_tick == 0U) {
        display_learn_summary_page(1U);
      }
      (void)panel_priority_delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
      learn_confirmed_blink_tick = (uint16_t)(learn_confirmed_blink_tick + FIRST_GEN_PANEL_KEY_POLL_MS);
      if(learn_confirmed_blink_tick >= FIRST_GEN_LEARN_CONFIRMED_BLINK_MS) {
        learn_confirmed_blink_tick = 0U;
      }
      return;
    }
    if(self_test_result_ready != 0U && g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_SELF_TEST) {
      (void)panel_priority_delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
      return;
    }
    if(auto_result_ready != 0U &&
       g_first_gen_panel_mode == FIRST_GEN_PANEL_MODE_AUTO_TEST &&
       first_gen_display_is_lcdm() != 0U) {
      /* This is reached only when AUTO was explicitly stopped; a normal
       * AUTO PASS/NG remains panel_auto_enabled and continues monitoring. */
      (void)panel_priority_delay_ms(FIRST_GEN_PANEL_KEY_POLL_MS);
      return;
    }
    (void)panel_priority_delay_ms(FIRST_GEN_4051_SCAN_PERIOD_MS);
    if(panel_waiting_for_reconnect == 0U && g_first_gen_pass_hold_active == 0U) {
      panel_idle_ms += (FIRST_GEN_4051_SCAN_PERIOD_MS == 0U) ? 1U : FIRST_GEN_4051_SCAN_PERIOD_MS;
      if(panel_idle_ms >= FIRST_GEN_IDLE_SCROLL_MS) {
        if(g_first_gen_recipe_valid != 0U && expected_harness_has_connection() != 0U) {
          panel_idle_ms = 0U;
        } else {
          display_power_on_scroll();
          panel_idle_ms = 0U;
          display_current_idle_state();
        }
      }
    }
    return;
  }

  if(g_first_gen_recipe_valid == 0U) {
    g_first_gen_print_ready = 0U;
    display_learn();
    (void)panel_priority_delay_ms(FIRST_GEN_4051_SCAN_PERIOD_MS);
    return;
  }

  /* Every LCDM AUTO run, including its very first run after K2, uses the
   * same ordered fast path: prove the learned edges first, then sweep
   * unlearned positions for extra connections.  A later live-monitor cycle
   * may restore PASS immediately after its compact learned-edge check; the
   * very first AUTO result must wait for its full sweep so an unseen short
   * cannot flash PASS before the direct NG page.  Previously the first run
   * took the row-by-row result-rendering path until all 94 rows had been
   * drawn. Those acknowledged LCDM transfers, rather than the mux reads,
   * were the roughly four-second wait before the first PASS screen.
   *
   * The complete physical record is still assembled by the sparse sweep and
   * becomes available to K1 at its completion; it simply no longer blocks
   * the immediate PASS indication. */
  priority_monitor = (first_gen_display_is_lcdm() != 0U &&
                      auto_result_fault_collecting == 0U) ? 1U : 0U;
  learned_precheck_failed = 0U;

  if(scan_out_point == 1U && waiting_on_error == 0U &&
     (priority_monitor == 0U || auto_monitor_expected_verified == 0U)) {
    g_first_gen_missing_counter = 0U;
    g_first_gen_unexpected_counter = 0U;
    g_first_gen_first_fail_out = 0U;
    g_first_gen_first_fail_in = 0U;
    scan_cycle_has_problem = 0U;

    if(priority_monitor != 0U) {
      /* The precheck has just proved these edges, so seed the live matrix
       * with the learned result and merge only any later extra edges. */
      matrix_copy(auto_result_matrix, expected_matrix);
      expected_check = expected_harness_check_first_problem(&learned_problem_out,
                                                            &learned_problem_in);
      if(expected_check == FIRST_GEN_SCAN_RESULT_INTERRUPTED) {
        /* The PAGE action has already started drawing.  Do not make it wait
         * for the remaining PASS scan; resume this compact learned check on
         * the next service call. */
        auto_monitor_expected_verified = 0U;
        return;
      }
      if(expected_check == FIRST_GEN_SCAN_RESULT_FAIL) {
        g_first_gen_missing_counter++;
        panel_record_current_problem(learned_problem_out,
                                     learned_problem_in,
                                     FIRST_GEN_PROBLEM_MISSING);
        g_first_gen_print_blocked_counter++;
        learned_precheck_failed = 1U;
      } else {
        /* A previous NG may have been a short that does not disturb any
         * learned edge.  Recheck only the short positions retained in its
         * completed record.  If one remains, stay NG; if all are gone, the
         * learned-edge check can immediately restore PASS while the broad
         * unlearned sweep continues invisibly in the background. */
        known_short_check = FIRST_GEN_SCAN_RESULT_PASS;
        if(auto_result_ready != 0U && g_first_gen_last_pass == 0U) {
          known_short_check = auto_result_check_known_shorts(&learned_problem_out,
                                                              &learned_problem_in);
          if(known_short_check == FIRST_GEN_SCAN_RESULT_INTERRUPTED) {
            auto_monitor_expected_verified = 0U;
            return;
          }
          if(known_short_check == FIRST_GEN_SCAN_RESULT_FAIL) {
            g_first_gen_unexpected_counter++;
            panel_record_current_problem(learned_problem_out,
                                         learned_problem_in,
                                         FIRST_GEN_PROBLEM_SHORT);
            g_first_gen_print_blocked_counter++;
            learned_precheck_failed = 1U;
          }
        }

        if(learned_precheck_failed == 0U) {
          /* The first AUTO cycle has no completed physical record yet.
           * Do not publish green only because its learned edges are intact:
           * the following unlearned sweep may still discover a short. */
          if(auto_result_ready != 0U && g_first_gen_last_pass == 0U) {
            panel_show_lcdm_auto_pass(0U);
          }
          auto_monitor_expected_verified = 1U;
        }
      }
    } else {
      auto_monitor_expected_verified = 0U;
    }
  }

  if(first_gen_display_is_lcdm() == 0U &&
     g_first_gen_last_pass == 0U &&
     panel_display_state != FIRST_GEN_DISPLAY_NG) {
    display_auto_test_pair(scan_out_point);
    panel_display_state = FIRST_GEN_DISPLAY_SCAN;
  }

  if(learned_precheck_failed != 0U) {
    pass = 0U;
  } else if(priority_monitor != 0U) {
    /* The learned edges were just checked as one compact pass.  Complete all
     * remaining unlearned rows in this same service turn, rather than making
     * PASS wait for 94 separate main-loop/display polls.  The whole sparse
     * sweep contains no acknowledged LCDM drawing commands. */
    do {
      pass = scan_and_check_unlearned_row();
      if(pass == FIRST_GEN_SCAN_RESULT_INTERRUPTED) {
        /* A K1-K4 command was accepted in the PASS-only background sweep.
         * Preserve scan_out_point so the same electrical row is safely
         * retried after the foreground action. */
        return;
      }
      auto_result_merge_current_row(scan_out_point);
      if(pass == FIRST_GEN_SCAN_RESULT_PASS) {
        scan_out_point++;
      }
    } while(pass == FIRST_GEN_SCAN_RESULT_PASS && scan_out_point <= FIRST_GEN_ACTIVE_POINT_COUNT);
  } else {
    pass = scan_and_check_current_row();
    auto_result_store_current_row(scan_out_point);
  }
  /* The timer/blink service runs at the start of every service call, even if
   * a PASS sweep is interrupted by K1.  Keep the electrical row free of LCDM
   * work here so an input change is still detected at full scan speed. */
  if(pass != 0U) {
    /* Build the live record during the first test only.  Once a complete
     * result is known, continue scanning electrically in the background
     * without LCDM record traffic slowing immediate change detection. */
    if(auto_result_ready == 0U && auto_result_summary_active == 0U) {
      auto_result_update_lcdm_for_out(scan_out_point);
    }
    waiting_on_error = 0U;
    scan_out_point++;
    if(scan_out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
      /* The completed physical matrix is now authoritative.  Its formatted
       * browse cache is scheduled below, after PASS/NG has been put on the
       * LCDM, so list preparation cannot create a blank transition page. */
      if(priority_monitor != 0U && scan_cycle_has_problem == 0U) {
        /* The compact PASS monitor has verified every learned edge and every
         * unlearned position.  Its work matrix is now a complete physical
         * PASS record, so replace the previous NG record before K1 redraws. */
        matrix_copy(auto_result_record_matrix, auto_result_matrix);
      }
      if(first_gen_display_is_lcdm() != 0U) {
        /* Keep the completed physical matrix intact now, but defer result
         * record formatting until after the immediate PASS/NG summary has
         * been sent.  That is what prevents the initial blank list page. */
        auto_result_cache_pending = 1U;
      }
      g_first_gen_scan_counter++;
      scan_out_point = 1U;
      auto_monitor_expected_verified = 0U;
      auto_result_fault_collecting = 0U;
      if(first_gen_display_is_lcdm() != 0U) {
        panel_finish_lcdm_auto_result();
      } else if(scan_cycle_has_problem == 0U) {
        g_first_gen_last_pass = 1U;
        panel_hold_pass_until_restart();
      } else {
        g_first_gen_last_pass = 0U;
        panel_reset_print_state();
        panel_display_ng_once();
      }
    } else {
      (void)panel_priority_delay_ms(FIRST_GEN_PANEL_TEST_STEP_MS);
    }
  } else {
    uint8_t first_problem_in_cycle = (scan_cycle_has_problem == 0U) ? 1U : 0U;

    scan_cycle_has_problem = 1U;
    g_first_gen_last_pass = 0U;
    panel_reset_print_state();
    /* Present the first changed line of this scan cycle immediately, but
     * keep scanning.  A subsequent complete clean cycle is what safely
     * proves that the cable has recovered and may return to PASS. */
    if(first_gen_display_is_lcdm() != 0U &&
       (first_problem_in_cycle != 0U || auto_result_summary_active == 0U)) {
      panel_show_lcdm_auto_ng_now();
    }
    if(first_gen_display_is_lcdm() == 0U) {
      panel_display_ng_once();
    }
    waiting_on_error = 0U;
    if(priority_monitor != 0U) {
      /* The first lowest-address fault has already reached the NG page.
       * Follow it with one complete, non-display scan so the K1 result page
       * can mark every other open/short group as well. */
      auto_result_fault_collecting = 1U;
      auto_monitor_expected_verified = 0U;
      scan_out_point = 1U;
      io_mux_disable_all();
    } else {
      scan_out_point++;
      if(scan_out_point > FIRST_GEN_ACTIVE_POINT_COUNT) {
        g_first_gen_scan_counter++;
        scan_out_point = 1U;
        auto_result_fault_collecting = 0U;
        if(first_gen_display_is_lcdm() != 0U) {
          auto_result_cache_pending = 1U;
          panel_finish_lcdm_auto_result();
        }
      }
    }
  }
}

#if IO_APP_MODE == IO_APP_MODE_FIRST_GEN_4051_LOCAL
static uint16_t adc_read_once(adc_type *adc_x)
{
  uint32_t timeout = FIRST_GEN_4051_ADC_TIMEOUT;

  adc_flag_clear(adc_x, ADC_OCCE_FLAG | ADC_OCCO_FLAG | ADC_TCF_FLAG);
  adc_ordinary_software_trigger_enable(adc_x, TRUE);
  while(adc_flag_get(adc_x, ADC_OCCE_FLAG) == RESET) {
    if(timeout == 0U) {
      return 0U;
    }
    timeout--;
  }

  return adc_ordinary_conversion_data_get(adc_x);
}

static uint16_t active_input_point(void)
{
  uint16_t point;

  if(!IO_POS_IS_IN(g_scan_active_in_pos)) {
    return 0U;
  }

  point = IO_POS_INDEX_1_BASED(g_scan_active_in_pos);
  return (point <= FIRST_GEN_ACTIVE_POINT_COUNT) ? point : 0U;
}

uint8_t io_scan_measure_selected_pair(void)
{
  uint16_t point = active_input_point();
  uint8_t input_high;

  if(point == 0U) {
    return 0U;
  }

  if(point <= FIRST_GEN_A_HALF_POINT_COUNT) {
    input_high = (gpio_input_data_bit_read(GPIOA, GPIO_PINS_0) != RESET) ? 1U : 0U;
    g_first_gen_last_adc1 = input_high ? 4095U : 0U;
  } else {
    input_high = (gpio_input_data_bit_read(GPIOA, GPIO_PINS_2) != RESET) ? 1U : 0U;
    g_first_gen_last_adc2 = input_high ? 4095U : 0U;
  }

  return input_high;
}
#endif
