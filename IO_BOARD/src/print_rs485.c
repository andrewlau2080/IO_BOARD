#include "print_rs485.h"

#include "at32f45x.h"

#ifndef PRINT_RS485_USE_DIR_PIN
#define PRINT_RS485_USE_DIR_PIN 0
#endif

#ifndef PRINT_RS485_DIR_GPIO
#define PRINT_RS485_DIR_GPIO GPIOA
#endif

#ifndef PRINT_RS485_DIR_GPIO_CLOCK
#define PRINT_RS485_DIR_GPIO_CLOCK CRM_GPIOA_PERIPH_CLOCK
#endif

#ifndef PRINT_RS485_DIR_PIN
#define PRINT_RS485_DIR_PIN GPIO_PINS_1
#endif

#ifndef PRINT_RS485_GPIO
#define PRINT_RS485_GPIO GPIOB
#endif

#ifndef PRINT_RS485_GPIO_CLOCK
#define PRINT_RS485_GPIO_CLOCK CRM_GPIOB_PERIPH_CLOCK
#endif

#ifndef PRINT_RS485_TX_PIN
#define PRINT_RS485_TX_PIN GPIO_PINS_5
#endif

#ifndef PRINT_RS485_RX_PIN
#define PRINT_RS485_RX_PIN GPIO_PINS_3
#endif

#ifndef PRINT_RS485_TX_PIN_SOURCE
#define PRINT_RS485_TX_PIN_SOURCE GPIO_PINS_SOURCE5
#endif

#ifndef PRINT_RS485_RX_PIN_SOURCE
#define PRINT_RS485_RX_PIN_SOURCE GPIO_PINS_SOURCE3
#endif

volatile uint32_t g_print_rs485_tx_byte_count;
volatile uint32_t g_print_rs485_tx_frame_count;
volatile uint32_t g_print_rs485_reconfig_count;
volatile uint32_t g_print_rs485_rx_byte_count;
volatile uint8_t g_print_rs485_last_rx_byte;

static print_rs485_config_t rs485_config;
static uint32_t bit_cycles;

static void print_rs485_timebase_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t cycles_now(void)
{
  return DWT->CYCCNT;
}

static void wait_until_cycles(uint32_t target_cycles)
{
  while((int32_t)(cycles_now() - target_cycles) < 0) {
    __asm volatile("nop");
  }
}

static uint32_t baud_to_cycles(uint32_t baudrate)
{
  uint32_t cycles;

  if(baudrate == 0U) {
    baudrate = PRINT_RS485_BAUDRATE;
  }

  cycles = (system_core_clock + (baudrate / 2U)) / baudrate;
  if(cycles == 0U) {
    cycles = 1U;
  }
  return cycles;
}

static void tx_level(uint8_t high)
{
  if(high != 0U) {
    PRINT_RS485_GPIO->scr = PRINT_RS485_TX_PIN;
  } else {
    PRINT_RS485_GPIO->clr = PRINT_RS485_TX_PIN;
  }
}

static uint8_t rx_level(void)
{
  return gpio_input_data_bit_read(PRINT_RS485_GPIO, PRINT_RS485_RX_PIN) != RESET;
}

static void rs485_config_default(print_rs485_config_t *config)
{
  if(config == 0) {
    return;
  }

  config->baudrate = PRINT_RS485_BAUDRATE;
  config->data_bits = 8U;
  config->stop_bits = 1U;
  config->parity = 0U;
  config->direction_enabled = PRINT_RS485_USE_DIR_PIN ? 1U : 0U;
  config->direction_active_high = 1U;
}

static void rs485_config_validate(print_rs485_config_t *config)
{
  if(config == 0) {
    return;
  }

  switch(config->baudrate) {
  case 1200UL:
  case 2400UL:
  case 4800UL:
  case 9600UL:
  case 19200UL:
  case 38400UL:
  case 57600UL:
  case 115200UL:
    break;
  default:
    config->baudrate = PRINT_RS485_BAUDRATE;
    break;
  }

  config->data_bits = 8U;
  config->stop_bits = 1U;
  config->parity = 0U;
  config->direction_enabled = config->direction_enabled ? 1U : 0U;
  config->direction_active_high = config->direction_active_high ? 1U : 0U;
}

static void print_rs485_set_tx(uint8_t enabled)
{
#if PRINT_RS485_USE_DIR_PIN
  if(rs485_config.direction_enabled != 0U) {
    uint8_t level = enabled ? rs485_config.direction_active_high : (uint8_t)!rs485_config.direction_active_high;
    gpio_bits_write(PRINT_RS485_DIR_GPIO, PRINT_RS485_DIR_PIN, level ? TRUE : FALSE);
  }
#else
  (void)enabled;
#endif
}

static void write_byte(uint8_t value)
{
  uint8_t i;
  uint32_t target_cycles;
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();

  target_cycles = cycles_now();
  tx_level(0U);
  target_cycles += bit_cycles;
  wait_until_cycles(target_cycles);

  for(i = 0U; i < 8U; i++) {
    tx_level((value & (uint8_t)(1U << i)) ? 1U : 0U);
    target_cycles += bit_cycles;
    wait_until_cycles(target_cycles);
  }

  tx_level(1U);
  target_cycles += bit_cycles;
  wait_until_cycles(target_cycles);

  if(primask == 0U) {
    __enable_irq();
  }
}

void print_rs485_init(void)
{
  gpio_init_type gpio_init_struct;

  if(rs485_config.baudrate == 0U) {
    rs485_config_default(&rs485_config);
  }
  rs485_config_validate(&rs485_config);
  bit_cycles = baud_to_cycles(rs485_config.baudrate);
  print_rs485_timebase_init();

  crm_periph_clock_enable(PRINT_RS485_GPIO_CLOCK, TRUE);
  gpio_pin_mux_config(PRINT_RS485_GPIO, PRINT_RS485_TX_PIN_SOURCE, GPIO_MUX_0);
  gpio_pin_mux_config(PRINT_RS485_GPIO, PRINT_RS485_RX_PIN_SOURCE, GPIO_MUX_0);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = PRINT_RS485_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_bits_set(PRINT_RS485_GPIO, PRINT_RS485_TX_PIN);
  gpio_init(PRINT_RS485_GPIO, &gpio_init_struct);

  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = PRINT_RS485_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(PRINT_RS485_GPIO, &gpio_init_struct);

#if PRINT_RS485_USE_DIR_PIN
  crm_periph_clock_enable(PRINT_RS485_DIR_GPIO_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = PRINT_RS485_DIR_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(PRINT_RS485_DIR_GPIO, &gpio_init_struct);
#endif

  print_rs485_set_tx(0U);
  g_print_rs485_reconfig_count++;
}

void print_rs485_config_get(print_rs485_config_t *out_config)
{
  if(out_config == 0) {
    return;
  }

  if(rs485_config.baudrate == 0U) {
    rs485_config_default(&rs485_config);
  }
  *out_config = rs485_config;
}

void print_rs485_config_set(const print_rs485_config_t *config)
{
  if(config == 0) {
    return;
  }

  rs485_config = *config;
  rs485_config_validate(&rs485_config);
  print_rs485_init();
}

void print_rs485_config_reset_default(void)
{
  rs485_config_default(&rs485_config);
  print_rs485_init();
}

void print_rs485_write(const uint8_t *data, uint16_t len)
{
  uint16_t i;

  if(data == 0 || len == 0U) {
    return;
  }

  if(bit_cycles == 0U) {
    print_rs485_init();
  }

  print_rs485_set_tx(1U);
  for(i = 0U; i < len; i++) {
    write_byte(data[i]);
    g_print_rs485_tx_byte_count++;
  }
  print_rs485_set_tx(0U);
  g_print_rs485_tx_frame_count++;
}

uint8_t print_rs485_poll_byte(uint8_t *out_byte)
{
  uint8_t i;
  uint8_t value = 0U;
  uint32_t target_cycles;
  uint32_t primask;

  if(out_byte == 0) {
    return 0U;
  }

  if(bit_cycles == 0U) {
    print_rs485_init();
  }

  if(rx_level() != 0U) {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  target_cycles = cycles_now() + bit_cycles + (bit_cycles / 2U);
  wait_until_cycles(target_cycles);

  for(i = 0U; i < 8U; i++) {
    if(rx_level() != 0U) {
      value |= (uint8_t)(1U << i);
    }
    target_cycles += bit_cycles;
    wait_until_cycles(target_cycles);
  }

  if(primask == 0U) {
    __enable_irq();
  }

  *out_byte = value;
  g_print_rs485_last_rx_byte = value;
  g_print_rs485_rx_byte_count++;
  return 1U;
}
