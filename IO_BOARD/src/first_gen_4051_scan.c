#include "first_gen_4051_scan.h"

#include "at32f45x.h"
#include "at32f45x_board.h"
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

#define FIRST_GEN_4051_SCAN_PERIOD_MS 100U
#define FIRST_GEN_4051_ADC_TIMEOUT    20000U

volatile uint16_t g_first_gen_last_adc1;
volatile uint16_t g_first_gen_last_adc2;
volatile uint16_t g_first_gen_adc_threshold = FIRST_GEN_4051_ADC_THRESHOLD_DEFAULT;
volatile uint16_t g_first_gen_dac_code = FIRST_GEN_4051_DAC_CODE_DEFAULT;
volatile uint16_t g_first_gen_first_fail_out;
volatile uint16_t g_first_gen_first_fail_in;
volatile uint32_t g_first_gen_scan_counter;
volatile uint32_t g_first_gen_missing_counter;
volatile uint32_t g_first_gen_unexpected_counter;
volatile uint8_t g_first_gen_last_pass;

static io_scan_result_t scan_result;

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

static uint8_t matrix_bit_get(uint16_t out_point, uint16_t in_point)
{
  uint8_t word = (uint8_t)((in_point - 1U) >> 5);
  uint8_t bit = (uint8_t)((in_point - 1U) & 0x1FU);

  return (scan_result.matrix[out_point - 1U][word] & (1UL << bit)) != 0U;
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

static void display_pass(void)
{
  char text[TM1637_DIGITS] = {'P', 'A', 'S', 'S', ' ', ' '};

  tm1637_display_write_text6(text);
}

void first_gen_4051_scan_init(void)
{
  analog_gpio_init();
  dac_init_fixed_output();
  adc_init_inputs();
  tm1637_display_init();
  io_scan_init(IO_SCAN_PROFILE_FIRST_GEN_1TH);
  io_scan_clear_result(&scan_result);
  display_pass();
}

uint8_t first_gen_4051_scan_once(void)
{
  uint16_t out_point;
  uint16_t in_point;
  uint8_t connected;

  dac_dual_data_set(DAC_DUAL_12BIT_RIGHT, g_first_gen_dac_code, g_first_gen_dac_code);

  g_first_gen_missing_counter = 0U;
  g_first_gen_unexpected_counter = 0U;
  g_first_gen_first_fail_out = 0U;
  g_first_gen_first_fail_in = 0U;

  if(io_scan_all(&scan_result) != IO_SCAN_OK) {
    g_first_gen_first_fail_out = 1U;
    g_first_gen_first_fail_in = 1U;
    g_first_gen_last_pass = 0U;
    display_error_code(1U);
    return 0U;
  }

  for(out_point = 1U; out_point <= FIRST_GEN_4051_POINT_COUNT; out_point++) {
    for(in_point = 1U; in_point <= FIRST_GEN_4051_POINT_COUNT; in_point++) {
      connected = matrix_bit_get(out_point, in_point);
      if(out_point == in_point) {
        if(connected == 0U) {
          g_first_gen_missing_counter++;
          if(g_first_gen_first_fail_out == 0U) {
            g_first_gen_first_fail_out = out_point;
            g_first_gen_first_fail_in = in_point;
          }
        }
      } else if(connected != 0U) {
        g_first_gen_unexpected_counter++;
        if(g_first_gen_first_fail_out == 0U) {
          g_first_gen_first_fail_out = out_point;
          g_first_gen_first_fail_in = in_point;
        }
      }
    }
  }

  g_first_gen_scan_counter++;
  g_first_gen_last_pass =
    (g_first_gen_missing_counter == 0U && g_first_gen_unexpected_counter == 0U) ? 1U : 0U;

  if(g_first_gen_last_pass != 0U) {
    display_pass();
  } else {
    display_error_code(g_first_gen_first_fail_out);
  }

  return g_first_gen_last_pass;
}

void first_gen_4051_scan_service(void)
{
  (void)first_gen_4051_scan_once();
  delay_ms(FIRST_GEN_4051_SCAN_PERIOD_MS);
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
