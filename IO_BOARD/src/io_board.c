#include "io_board.h"

typedef struct {
  gpio_type *port;
  crm_periph_clock_type clock;
  uint16_t pin;
} io_pin_t;

#define PIN_A(n) {GPIOA, CRM_GPIOA_PERIPH_CLOCK, GPIO_PINS_##n}
#define PIN_B(n) {GPIOB, CRM_GPIOB_PERIPH_CLOCK, GPIO_PINS_##n}
#define PIN_C(n) {GPIOC, CRM_GPIOC_PERIPH_CLOCK, GPIO_PINS_##n}
#define PIN_D(n) {GPIOD, CRM_GPIOD_PERIPH_CLOCK, GPIO_PINS_##n}
#define PIN_E(n) {GPIOE, CRM_GPIOE_PERIPH_CLOCK, GPIO_PINS_##n}
#define PIN_H(n) {GPIOH, CRM_GPIOH_PERIPH_CLOCK, GPIO_PINS_##n}

static const io_pin_t in_a_addr[3] = {PIN_C(0), PIN_C(1), PIN_C(2)};
static const io_pin_t in_a_en[8] = {
  PIN_E(0), PIN_E(1), PIN_E(2), PIN_E(3),
  PIN_E(4), PIN_E(5), PIN_E(6), PIN_E(7)
};

static const io_pin_t in_b_addr[3] = {PIN_B(0), PIN_B(1), PIN_B(2)};
static const io_pin_t in_b_en[8] = {
  PIN_E(8), PIN_E(9), PIN_E(10), PIN_E(11),
  PIN_E(12), PIN_E(13), PIN_E(14), PIN_E(15)
};

static const io_pin_t out_a_addr[3] = {PIN_C(10), PIN_C(11), PIN_C(12)};
static const io_pin_t out_a_en[8] = {
  PIN_D(0), PIN_D(1), PIN_D(2), PIN_D(3),
  PIN_D(4), PIN_D(5), PIN_D(6), PIN_D(7)
};

static const io_pin_t out_b_addr[3] = {PIN_B(10), PIN_B(11), PIN_B(12)};
static const io_pin_t out_b_en[8] = {
  PIN_D(8), PIN_D(9), PIN_D(10), PIN_D(11),
  PIN_D(12), PIN_D(13), PIN_D(14), PIN_D(15)
};

static const io_pin_t buttons[6] = {
  PIN_C(4), PIN_C(5), PIN_C(6), PIN_C(7), PIN_C(8), PIN_C(9)
};

static const io_pin_t debug_out = PIN_H(3);
static const io_pin_t lcm_outputs[] = {
  PIN_B(4), /* LCM_RESET */
  PIN_B(6), /* LCM_CMD */
  PIN_B(7), /* LCM_CS */
  PIN_B(8), /* LCM_BL_LED */
};

static void clock_enable(const io_pin_t *pin)
{
  crm_periph_clock_enable(pin->clock, TRUE);
}

static void pin_write(const io_pin_t *pin, confirm_state state)
{
  gpio_bits_write(pin->port, pin->pin, state);
}

static void output_init(const io_pin_t *pin, confirm_state initial_state)
{
  gpio_init_type gpio_init_struct;

  clock_enable(pin);
  pin_write(pin, initial_state);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = pin->pin;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(pin->port, &gpio_init_struct);
}

static void input_pullup_init(const io_pin_t *pin)
{
  gpio_init_type gpio_init_struct;

  clock_enable(pin);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = pin->pin;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(pin->port, &gpio_init_struct);
}

static void output_group_init(const io_pin_t *pins, uint8_t count, confirm_state initial_state)
{
  uint8_t i;

  for(i = 0; i < count; i++) {
    output_init(&pins[i], initial_state);
  }
}

static void mux_disable_group(const io_pin_t *en_pins)
{
  uint8_t i;

  for(i = 0; i < 8; i++) {
    pin_write(&en_pins[i], TRUE);
  }
}

static void mux_address_write(const io_pin_t *addr_pins, uint8_t channel)
{
  uint8_t i;

  for(i = 0; i < 3; i++) {
    pin_write(&addr_pins[i], (channel & (1U << i)) ? TRUE : FALSE);
  }
}

static void mux_group_select(const io_pin_t *addr_pins, const io_pin_t *en_pins, uint8_t index)
{
  mux_disable_group(en_pins);
  mux_address_write(addr_pins, index & 0x07);
  pin_write(&en_pins[index >> 3], FALSE);
}

void io_mux_disable_all(void)
{
  mux_disable_group(in_a_en);
  mux_disable_group(in_b_en);
  mux_disable_group(out_a_en);
  mux_disable_group(out_b_en);
}

void io_mux_select(io_mux_bank_t bank, uint8_t index)
{
  if(index >= 64) {
    return;
  }

  switch(bank) {
  case IO_MUX_IN_A:
    mux_group_select(in_a_addr, in_a_en, index);
    break;
  case IO_MUX_IN_B:
    mux_group_select(in_b_addr, in_b_en, index);
    break;
  case IO_MUX_OUT_A:
    mux_group_select(out_a_addr, out_a_en, index);
    break;
  case IO_MUX_OUT_B:
    mux_group_select(out_b_addr, out_b_en, index);
    break;
  default:
    break;
  }
}

uint8_t io_button_read(io_button_t button)
{
  if(button > IO_BTN_UP) {
    return 0;
  }

  return gpio_input_data_bit_read(buttons[button].port, buttons[button].pin) == RESET;
}

void io_debug_write(uint8_t level)
{
  gpio_bits_write(debug_out.port, debug_out.pin, level ? TRUE : FALSE);
}

void io_debug_toggle(void)
{
  gpio_bits_toggle(debug_out.port, debug_out.pin);
}

void io_board_init(void)
{
  uint8_t i;

  output_group_init(in_a_addr, 3, FALSE);
  output_group_init(in_b_addr, 3, FALSE);
  output_group_init(out_a_addr, 3, FALSE);
  output_group_init(out_b_addr, 3, FALSE);

  output_group_init(in_a_en, 8, TRUE);
  output_group_init(in_b_en, 8, TRUE);
  output_group_init(out_a_en, 8, TRUE);
  output_group_init(out_b_en, 8, TRUE);

  for(i = 0; i < 6; i++) {
    input_pullup_init(&buttons[i]);
  }

  output_init(&debug_out, FALSE);
  output_init(&lcm_outputs[0], TRUE);
  output_init(&lcm_outputs[1], FALSE);
  output_init(&lcm_outputs[2], TRUE);
  output_init(&lcm_outputs[3], FALSE);

  io_mux_disable_all();
}
