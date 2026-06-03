#include "rpi_rs485.h"

#include "at32f45x.h"

#ifndef RPI_RS485_USE_DIR_PIN
#define RPI_RS485_USE_DIR_PIN 0
#endif

#if RPI_RS485_USE_DIR_PIN
#ifndef RPI_RS485_DIR_GPIO
#define RPI_RS485_DIR_GPIO GPIOA
#endif
#ifndef RPI_RS485_DIR_GPIO_CLOCK
#define RPI_RS485_DIR_GPIO_CLOCK CRM_GPIOA_PERIPH_CLOCK
#endif
#ifndef RPI_RS485_DIR_PIN
#define RPI_RS485_DIR_PIN GPIO_PINS_8
#endif
#endif

static void rs485_set_tx(uint8_t enabled)
{
#if RPI_RS485_USE_DIR_PIN
  gpio_bits_write(RPI_RS485_DIR_GPIO, RPI_RS485_DIR_PIN, enabled ? TRUE : FALSE);
#else
  (void)enabled;
#endif
}

void rpi_rs485_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_9 | GPIO_PINS_10;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(GPIOA, &gpio_init_struct);

  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE9, GPIO_MUX_7);
  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE10, GPIO_MUX_7);

#if RPI_RS485_USE_DIR_PIN
  crm_periph_clock_enable(RPI_RS485_DIR_GPIO_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = RPI_RS485_DIR_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(RPI_RS485_DIR_GPIO, &gpio_init_struct);
#endif
  rs485_set_tx(0U);

  usart_init(USART1, RPI_RS485_BAUDRATE, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(USART1, USART_PARITY_NONE);
  usart_hardware_flow_control_set(USART1, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(USART1, TRUE);
  usart_receiver_enable(USART1, TRUE);
  usart_enable(USART1, TRUE);
}

uint8_t rpi_rs485_poll_byte(uint8_t *out_byte)
{
  if(out_byte == 0) {
    return 0U;
  }

  if(usart_flag_get(USART1, USART_RDBF_FLAG) == RESET) {
    return 0U;
  }

  *out_byte = (uint8_t)usart_data_receive(USART1);
  return 1U;
}

void rpi_rs485_write(const uint8_t *data, uint16_t len)
{
  uint16_t i;

  if(data == 0 || len == 0U) {
    return;
  }

  rs485_set_tx(1U);
  for(i = 0U; i < len; i++) {
    while(usart_flag_get(USART1, USART_TDBE_FLAG) == RESET) {
    }
    usart_data_transmit(USART1, data[i]);
  }
  while(usart_flag_get(USART1, USART_TDC_FLAG) == RESET) {
  }
  rs485_set_tx(0U);
}
