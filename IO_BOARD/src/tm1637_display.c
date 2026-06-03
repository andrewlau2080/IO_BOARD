#include "tm1637_display.h"

#include "at32f45x_board.h"
#include "at32f45x_gpio.h"

#define TM1637_CLK_GPIO             GPIOA
#define TM1637_CLK_PIN              GPIO_PINS_9   /* J8 USART1_TX reused as GPIO CLK */
#define TM1637_DIO_GPIO             GPIOA
#define TM1637_DIO_PIN              GPIO_PINS_10  /* J8 USART1_RX reused as GPIO DIO */
#define TM1637_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK

#define TM1637_CMD_DATA_FIXED       0x44U
#define TM1637_CMD_READ_KEY         0x42U
#define TM1637_CMD_ADDR_BASE        0xC0U
#define TM1637_CMD_DISPLAY_BASE     0x80U
#define TM1637_DELAY_US             3U

static uint8_t display_control = 0x8CU;

static void tm_delay(void)
{
  delay_us(TM1637_DELAY_US);
}

static void clk_write(uint8_t high)
{
  gpio_bits_write(TM1637_CLK_GPIO, TM1637_CLK_PIN, high ? TRUE : FALSE);
}

static void dio_write(uint8_t high)
{
  gpio_bits_write(TM1637_DIO_GPIO, TM1637_DIO_PIN, high ? TRUE : FALSE);
}

static uint8_t dio_read(void)
{
  return gpio_input_data_bit_read(TM1637_DIO_GPIO, TM1637_DIO_PIN) ? 1U : 0U;
}

static void tm_start(void)
{
  dio_write(1U);
  clk_write(1U);
  tm_delay();
  dio_write(0U);
  tm_delay();
  clk_write(0U);
}

static void tm_stop(void)
{
  clk_write(0U);
  dio_write(0U);
  tm_delay();
  clk_write(1U);
  tm_delay();
  dio_write(1U);
  tm_delay();
}

static uint8_t tm_write_byte(uint8_t value)
{
  uint8_t i;
  uint8_t ack;

  for(i = 0U; i < 8U; i++) {
    clk_write(0U);
    dio_write((value & 0x01U) != 0U);
    tm_delay();
    clk_write(1U);
    tm_delay();
    value >>= 1;
  }

  clk_write(0U);
  dio_write(1U);
  tm_delay();
  clk_write(1U);
  tm_delay();
  ack = (dio_read() == 0U) ? 1U : 0U;
  clk_write(0U);

  return ack;
}

static void tm_command(uint8_t command)
{
  tm_start();
  (void)tm_write_byte(command);
  tm_stop();
}

uint8_t tm1637_key_read_raw(void)
{
  uint8_t i;
  uint8_t key = 0U;

  tm_start();
  (void)tm_write_byte(TM1637_CMD_READ_KEY);
  dio_write(1U);

  for(i = 0U; i < 8U; i++) {
    clk_write(0U);
    tm_delay();
    key >>= 1;
    clk_write(1U);
    tm_delay();
    if(dio_read() != 0U) {
      key |= 0x80U;
    }
  }

  clk_write(0U);
  dio_write(0U);
  tm_delay();
  clk_write(1U);
  tm_delay();
  clk_write(0U);
  dio_write(1U);
  tm_stop();

  return key;
}

void tm1637_display_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(TM1637_GPIO_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = TM1637_CLK_PIN | TM1637_DIO_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(TM1637_CLK_GPIO, &gpio_init_struct);

  clk_write(1U);
  dio_write(1U);
  tm1637_display_set_brightness(4U, 1U);
  tm1637_display_clear();
}

void tm1637_display_set_brightness(uint8_t brightness, uint8_t enabled)
{
  if(brightness > 7U) {
    brightness = 7U;
  }

  display_control = (uint8_t)(TM1637_CMD_DISPLAY_BASE | (enabled ? 0x08U : 0x00U) | brightness);
  tm_command(display_control);
}

void tm1637_display_write_raw(uint8_t position, uint8_t segments)
{
  if(position >= TM1637_DIGITS) {
    return;
  }

  tm_command(TM1637_CMD_DATA_FIXED);

  tm_start();
  (void)tm_write_byte((uint8_t)(TM1637_CMD_ADDR_BASE + position));
  (void)tm_write_byte(segments);
  tm_stop();

  tm_command(display_control);
}

void tm1637_display_write_raw6(const uint8_t segments[TM1637_DIGITS])
{
  uint8_t i;

  if(segments == 0) {
    return;
  }

  tm_command(TM1637_CMD_DATA_FIXED);
  for(i = 0U; i < TM1637_DIGITS; i++) {
    tm_start();
    (void)tm_write_byte((uint8_t)(TM1637_CMD_ADDR_BASE + i));
    (void)tm_write_byte(segments[i]);
    tm_stop();
  }
  tm_command(display_control);
}

void tm1637_display_clear(void)
{
  static const uint8_t blank[TM1637_DIGITS] = {0U, 0U, 0U, 0U, 0U, 0U};

  tm1637_display_write_raw6(blank);
}

uint8_t tm1637_display_encode_char(char ch)
{
  switch(ch) {
  case '0': return 0x3FU;
  case '1': return 0x06U;
  case '2': return 0x5BU;
  case '3': return 0x4FU;
  case '4': return 0x66U;
  case '5': return 0x6DU;
  case '6': return 0x7DU;
  case '7': return 0x07U;
  case '8': return 0x7FU;
  case '9': return 0x6FU;
  case 'A':
  case 'a': return 0x77U;
  case 'B':
  case 'b': return 0x7CU;
  case 'C':
  case 'c': return 0x39U;
  case 'D':
  case 'd': return 0x5EU;
  case 'E':
  case 'e': return 0x79U;
  case 'F':
  case 'f': return 0x71U;
  case 'G':
  case 'g': return 0x3DU;
  case 'H':
  case 'h': return 0x76U;
  case 'L':
  case 'l': return 0x38U;
  case 'N':
  case 'n': return 0x54U;
  case 'O':
  case 'o': return 0x5CU;
  case 'P':
  case 'p': return 0x73U;
  case 'S':
  case 's': return 0x6DU;
  case 'T':
  case 't': return 0x78U;
  case 'U':
  case 'u': return 0x3EU;
  case '-': return 0x40U;
  case '_': return 0x08U;
  case ' ':
  default:
    return 0x00U;
  }
}

void tm1637_display_write_text6(const char text[TM1637_DIGITS])
{
  uint8_t segments[TM1637_DIGITS];
  uint8_t i;

  if(text == 0) {
    return;
  }

  for(i = 0U; i < TM1637_DIGITS; i++) {
    segments[i] = tm1637_display_encode_char(text[i]);
  }

  tm1637_display_write_raw6(segments);
}

void tm1637_display_write_u32_6(uint32_t value)
{
  char text[TM1637_DIGITS];
  int8_t i;

  value %= 1000000UL;
  for(i = (int8_t)TM1637_DIGITS - 1; i >= 0; i--) {
    text[i] = (char)('0' + (value % 10UL));
    value /= 10UL;
  }

  tm1637_display_write_text6(text);
}
