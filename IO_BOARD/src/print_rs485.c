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

#ifndef PRINT_RS485_USART
#define PRINT_RS485_USART USART1
#endif

#ifndef PRINT_RS485_USART_CLOCK
#define PRINT_RS485_USART_CLOCK CRM_USART1_PERIPH_CLOCK
#endif

#ifndef PRINT_RS485_GPIO
#define PRINT_RS485_GPIO GPIOA
#endif

#ifndef PRINT_RS485_GPIO_CLOCK
#define PRINT_RS485_GPIO_CLOCK CRM_GPIOA_PERIPH_CLOCK
#endif

#ifndef PRINT_RS485_TX_PIN
#define PRINT_RS485_TX_PIN GPIO_PINS_9
#endif

#ifndef PRINT_RS485_RX_PIN
#define PRINT_RS485_RX_PIN GPIO_PINS_10
#endif

#ifndef PRINT_RS485_TX_PIN_SOURCE
#define PRINT_RS485_TX_PIN_SOURCE GPIO_PINS_SOURCE9
#endif

#ifndef PRINT_RS485_RX_PIN_SOURCE
#define PRINT_RS485_RX_PIN_SOURCE GPIO_PINS_SOURCE10
#endif

#ifndef PRINT_RS485_GPIO_MUX
#define PRINT_RS485_GPIO_MUX GPIO_MUX_7
#endif

volatile uint32_t g_print_rs485_tx_byte_count;
volatile uint32_t g_print_rs485_tx_frame_count;
volatile uint32_t g_print_rs485_reconfig_count;
volatile uint32_t g_print_rs485_rx_byte_count;
volatile uint8_t g_print_rs485_last_rx_byte;

static print_rs485_config_t rs485_config;

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

  if(config->data_bits != 7U && config->data_bits != 8U && config->data_bits != 9U) {
    config->data_bits = 8U;
  }
  if(config->stop_bits != 1U && config->stop_bits != 2U) {
    config->stop_bits = 1U;
  }
  if(config->parity > 2U) {
    config->parity = 0U;
  }
  config->direction_enabled = config->direction_enabled ? 1U : 0U;
  config->direction_active_high = config->direction_active_high ? 1U : 0U;
}

static usart_data_bit_num_type rs485_data_bits(void)
{
  if(rs485_config.data_bits == 7U) {
    return USART_DATA_7BITS;
  }
  if(rs485_config.data_bits == 9U) {
    return USART_DATA_9BITS;
  }
  return USART_DATA_8BITS;
}

static usart_stop_bit_num_type rs485_stop_bits(void)
{
  return (rs485_config.stop_bits == 2U) ? USART_STOP_2_BIT : USART_STOP_1_BIT;
}

static usart_parity_selection_type rs485_parity(void)
{
  if(rs485_config.parity == 1U) {
    return USART_PARITY_EVEN;
  }
  if(rs485_config.parity == 2U) {
    return USART_PARITY_ODD;
  }
  return USART_PARITY_NONE;
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

void print_rs485_init(void)
{
  gpio_init_type gpio_init_struct;

  if(rs485_config.baudrate == 0U) {
    rs485_config_default(&rs485_config);
  }
  rs485_config_validate(&rs485_config);

  usart_enable(PRINT_RS485_USART, FALSE);
  crm_periph_clock_enable(PRINT_RS485_GPIO_CLOCK, TRUE);
  crm_periph_clock_enable(PRINT_RS485_USART_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = PRINT_RS485_TX_PIN | PRINT_RS485_RX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(PRINT_RS485_GPIO, &gpio_init_struct);

  gpio_pin_mux_config(PRINT_RS485_GPIO, PRINT_RS485_TX_PIN_SOURCE, PRINT_RS485_GPIO_MUX);
  gpio_pin_mux_config(PRINT_RS485_GPIO, PRINT_RS485_RX_PIN_SOURCE, PRINT_RS485_GPIO_MUX);

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

  usart_init(PRINT_RS485_USART, rs485_config.baudrate, rs485_data_bits(), rs485_stop_bits());
  usart_parity_selection_config(PRINT_RS485_USART, rs485_parity());
  usart_hardware_flow_control_set(PRINT_RS485_USART, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(PRINT_RS485_USART, TRUE);
  usart_receiver_enable(PRINT_RS485_USART, TRUE);
  usart_enable(PRINT_RS485_USART, TRUE);
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

  print_rs485_set_tx(1U);
  for(i = 0U; i < len; i++) {
    while(usart_flag_get(PRINT_RS485_USART, USART_TDBE_FLAG) == RESET) {
    }
    usart_data_transmit(PRINT_RS485_USART, data[i]);
    g_print_rs485_tx_byte_count++;
  }
  while(usart_flag_get(PRINT_RS485_USART, USART_TDC_FLAG) == RESET) {
  }
  print_rs485_set_tx(0U);
  g_print_rs485_tx_frame_count++;
}

uint8_t print_rs485_poll_byte(uint8_t *out_byte)
{
  uint8_t value;

  if(out_byte == 0) {
    return 0U;
  }

  if(usart_flag_get(PRINT_RS485_USART, USART_RDBF_FLAG) == RESET) {
    return 0U;
  }

  value = (uint8_t)usart_data_receive(PRINT_RS485_USART);
  *out_byte = value;
  g_print_rs485_last_rx_byte = value;
  g_print_rs485_rx_byte_count++;
  return 1U;
}
