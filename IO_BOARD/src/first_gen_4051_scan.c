#include "first_gen_4051_scan.h"

#include "at32f45x.h"
#include "at32f45x_board.h"
#include "at32f45x_flash.h"
#include "io_board.h"
#include "io_scan.h"
#include "tm1637_display.h"

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

#define FIRST_GEN_4051_SCAN_PERIOD_MS 30U
#define FIRST_GEN_4051_ERROR_PERIOD_MS 120U
#define FIRST_GEN_4051_ADC_TIMEOUT    20000U

#define FIRST_GEN_RECIPE_FLASH_ADDR   0x0807F800U
#define FIRST_GEN_RECIPE_MAGIC        0x3148544CU
#define FIRST_GEN_RECIPE_VERSION      1U

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
volatile uint8_t g_first_gen_recipe_valid;
volatile uint8_t g_first_gen_learn_status;
volatile uint8_t g_first_gen_last_pass;

static io_scan_result_t scan_result;
static uint32_t expected_matrix[FIRST_GEN_4051_POINT_COUNT][IO_SCAN_MATRIX_WORDS];
static uint16_t scan_out_point = 1U;
static uint8_t waiting_on_error;

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

static void analog_gpio_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_2 | GPIO_PINS_4 | GPIO_PINS_5;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOA, &gpio_init_struct);
}

static void dac_init_fixed_output(void)
{
  crm_periph_clock_enable(CRM_DAC_PERIPH_CLOCK, TRUE);
  dac_reset();

  dac_trigger_enable(DAC1_SELECT, FALSE);
  dac_trigger_enable(DAC2_SELECT, FALSE);
  dac_wave_generate(DAC1_SELECT, DAC_WAVE_GENERATE_NONE);
  dac_wave_generate(DAC2_SELECT, DAC_WAVE_GENERATE_NONE);
  dac_output_buffer_enable(DAC1_SELECT, TRUE);
  dac_output_buffer_enable(DAC2_SELECT, TRUE);
  dac_enable(DAC1_SELECT, TRUE);
  dac_enable(DAC2_SELECT, TRUE);
  dac_dual_data_set(DAC_DUAL_12BIT_RIGHT, g_first_gen_dac_code, g_first_gen_dac_code);
}

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
  char text[TM1637_DIGITS];

  text[0] = (char)('0' + ((left / 100U) % 10U));
  text[1] = (char)('0' + ((left / 10U) % 10U));
  text[2] = (char)('0' + (left % 10U));
  text[3] = (char)('0' + ((right / 100U) % 10U));
  text[4] = (char)('0' + ((right / 10U) % 10U));
  text[5] = (char)('0' + (right % 10U));
  tm1637_display_write_text6(text);
}

static void display_pass(void)
{
  char text[TM1637_DIGITS] = {'P', 'A', 'S', 'S', ' ', ' '};

  tm1637_display_write_text6(text);
}

static void display_learn(void)
{
  char text[TM1637_DIGITS] = {'L', 'E', 'A', 'r', 'n', ' '};

  tm1637_display_write_text6(text);
}

static void display_saved(void)
{
  char text[TM1637_DIGITS] = {'S', 'A', 'U', 'E', 'd', ' '};

  tm1637_display_write_text6(text);
}

static void display_error_code(uint16_t point)
{
  char text[TM1637_DIGITS];

  text[0] = 'E';
  text[1] = (char)('0' + ((point / 10000U) % 10U));
  text[2] = (char)('0' + ((point / 1000U) % 10U));
  text[3] = (char)('0' + ((point / 100U) % 10U));
  text[4] = (char)('0' + ((point / 10U) % 10U));
  text[5] = (char)('0' + (point % 10U));
  tm1637_display_write_text6(text);
}

static uint8_t learn_key_pressed(void)
{
  uint8_t pressed = io_button_read(IO_BTN_ENTER);

  if(tm1637_key_read_raw() == TM1637_KEY_SET) {
    pressed = 1U;
  }

  return pressed;
}

static uint8_t recipe_flash_load(void)
{
  const first_gen_recipe_image_t *stored = (const first_gen_recipe_image_t *)FIRST_GEN_RECIPE_FLASH_ADDR;

  if(stored->magic != FIRST_GEN_RECIPE_MAGIC ||
     stored->version != FIRST_GEN_RECIPE_VERSION ||
     stored->point_count != FIRST_GEN_4051_POINT_COUNT ||
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
  recipe_image.point_count = FIRST_GEN_4051_POINT_COUNT;
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

  for(in_point = 1U; in_point <= FIRST_GEN_4051_POINT_COUNT; in_point++) {
    if(io_scan_read_pair(IO_POS_OUT(out_point), IO_POS_IN(in_point), &pair) != IO_SCAN_OK) {
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

static uint8_t scan_and_check_current_row(void)
{
  uint16_t problem_in;
  uint8_t problem_type;

  io_scan_clear_result(&scan_result);
  scan_result.profile_id = IO_SCAN_PROFILE_FIRST_GEN_1TH;
  scan_result.out_count = FIRST_GEN_4051_POINT_COUNT;
  scan_result.in_count = FIRST_GEN_4051_POINT_COUNT;
  scan_result.active_out_pos = IO_POS_OUT(scan_out_point);

  g_first_gen_current_out = scan_out_point;
  g_first_gen_current_problem_in = 0U;
  display_pair(scan_out_point, 0U);

  if(scan_one_row(scan_out_point) == 0U) {
    g_first_gen_first_fail_out = scan_out_point;
    g_first_gen_first_fail_in = 1U;
    g_first_gen_current_problem_in = 1U;
    g_first_gen_last_pass = 0U;
    display_pair(scan_out_point, 1U);
    return 0U;
  }

  if(find_row_problem(scan_out_point, &problem_in, &problem_type) != 0U) {
    if(problem_type == 1U) {
      g_first_gen_missing_counter++;
    } else {
      g_first_gen_unexpected_counter++;
    }
    g_first_gen_first_fail_out = scan_out_point;
    g_first_gen_first_fail_in = problem_in;
    g_first_gen_current_problem_in = problem_in;
    g_first_gen_last_pass = 0U;
    display_pair(scan_out_point, problem_in);
    waiting_on_error = 1U;
    return 0U;
  }

  display_pair(scan_out_point, 0U);
  return 1U;
}

void first_gen_4051_scan_init(void)
{
  analog_gpio_init();
  dac_init_fixed_output();
  adc_init_inputs();
  tm1637_display_init();
  io_scan_init(IO_SCAN_PROFILE_FIRST_GEN_1TH);
  io_scan_clear_result(&scan_result);
  matrix_clear(expected_matrix);
  g_first_gen_recipe_valid = recipe_flash_load();
  scan_out_point = 1U;
  waiting_on_error = 0U;

  if(g_first_gen_recipe_valid != 0U) {
    display_pair(1U, 0U);
  } else {
    display_learn();
  }
}

uint8_t first_gen_4051_scan_once(void)
{
  uint8_t pass;

  if(g_first_gen_recipe_valid == 0U) {
    g_first_gen_last_pass = 0U;
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
  } while(scan_out_point <= FIRST_GEN_4051_POINT_COUNT);

  g_first_gen_scan_counter++;
  g_first_gen_last_pass = 1U;
  display_pass();
  scan_out_point = 1U;
  return 1U;
}

uint8_t first_gen_4051_learn_current_harness(void)
{
  dac_dual_data_set(DAC_DUAL_12BIT_RIGHT, g_first_gen_dac_code, g_first_gen_dac_code);
  display_learn();

  io_scan_init(IO_SCAN_PROFILE_FIRST_GEN_1TH);
  if(io_scan_all(&scan_result) != IO_SCAN_OK) {
    g_first_gen_learn_status = 2U;
    display_error_code(1U);
    return 0U;
  }

  matrix_copy(expected_matrix, scan_result.matrix);
  if(recipe_flash_save() == 0U) {
    g_first_gen_recipe_valid = 0U;
    g_first_gen_learn_status = 3U;
    display_error_code(2U);
    return 0U;
  }

  g_first_gen_recipe_valid = 1U;
  g_first_gen_learn_status = 1U;
  g_first_gen_last_pass = 1U;
  scan_out_point = 1U;
  waiting_on_error = 0U;
  display_saved();
  delay_ms(500U);
  display_pair(1U, 0U);
  return 1U;
}

void first_gen_4051_scan_service(void)
{
  static uint8_t learn_key_latched;
  uint8_t key_pressed;
  uint8_t pass;

  key_pressed = learn_key_pressed();
  if(key_pressed != 0U && learn_key_latched == 0U) {
    learn_key_latched = 1U;
    (void)first_gen_4051_learn_current_harness();
    return;
  }
  if(key_pressed == 0U) {
    learn_key_latched = 0U;
  }

  if(g_first_gen_recipe_valid == 0U) {
    display_learn();
    delay_ms(FIRST_GEN_4051_SCAN_PERIOD_MS);
    return;
  }

  if(scan_out_point == 1U && waiting_on_error == 0U) {
    g_first_gen_missing_counter = 0U;
    g_first_gen_unexpected_counter = 0U;
    g_first_gen_first_fail_out = 0U;
    g_first_gen_first_fail_in = 0U;
  }

  pass = scan_and_check_current_row();
  if(pass != 0U) {
    waiting_on_error = 0U;
    scan_out_point++;
    if(scan_out_point > FIRST_GEN_4051_POINT_COUNT) {
      g_first_gen_scan_counter++;
      g_first_gen_last_pass = 1U;
      display_pass();
      delay_ms(300U);
      scan_out_point = 1U;
    } else {
      delay_ms(FIRST_GEN_4051_SCAN_PERIOD_MS);
    }
  } else {
    delay_ms(waiting_on_error ? FIRST_GEN_4051_ERROR_PERIOD_MS : FIRST_GEN_4051_SCAN_PERIOD_MS);
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
  return (point <= FIRST_GEN_4051_POINT_COUNT) ? point : 0U;
}

uint8_t io_scan_measure_selected_pair(void)
{
  uint16_t point = active_input_point();
  uint16_t adc_value;

  if(point == 0U) {
    return 0U;
  }

  if(point <= 48U) {
    adc_value = adc_read_once(ADC1);
    g_first_gen_last_adc1 = adc_value;
  } else {
    adc_value = adc_read_once(ADC2);
    g_first_gen_last_adc2 = adc_value;
  }

  return (adc_value >= g_first_gen_adc_threshold) ? 1U : 0U;
}
#endif
