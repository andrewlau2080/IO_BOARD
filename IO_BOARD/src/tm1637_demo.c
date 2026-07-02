#include "tm1637_demo.h"

#include "at32f45x_board.h"
#include "tm1637_display.h"

#define TM1637_DEMO_START_POINT 1U
#define TM1637_DEMO_END_POINT   92U
#define TM1637_DEMO_STEP_MS     160U
#define TM1637_DEMO_PASS_MS     1200U

volatile uint16_t g_tm1637_demo_point;
volatile uint32_t g_tm1637_demo_cycle;
volatile uint8_t g_tm1637_demo_phase;
volatile uint8_t g_tm1637_demo_last_key;

static void display_pair(uint16_t point)
{
  char text[TM1637_DIGITS];

  text[0] = 'A';
  text[1] = (char)('0' + ((point / 10U) % 10U));
  text[2] = (char)('0' + (point % 10U));
  text[3] = 'b';
  text[4] = (char)('0' + ((point / 10U) % 10U));
  text[5] = (char)('0' + (point % 10U));
  tm1637_display_write_text6(text);
}

static void display_zero(void)
{
  char text[TM1637_DIGITS] = {'0', '0', '0', '0', '0', '0'};

  tm1637_display_write_text6(text);
}

static void display_self_pass(void)
{
  char text[TM1637_DIGITS] = {'S', 't', 'P', 'A', 'S', 'S'};

  tm1637_display_write_text6(text);
}

void tm1637_demo_init(void)
{
  tm1637_display_init();
  tm1637_display_set_brightness(5U, 1U);
  g_tm1637_demo_point = TM1637_DEMO_START_POINT;
  g_tm1637_demo_cycle = 0U;
  g_tm1637_demo_phase = 0U;
  g_tm1637_demo_last_key = TM1637_KEY_NONE;
  display_zero();
  g_tm1637_demo_last_key = tm1637_key_read_raw();
  delay_ms(500U);
}

void tm1637_demo_service(void)
{
  uint16_t point;

  g_tm1637_demo_phase = 1U;
  for(point = TM1637_DEMO_START_POINT; point <= TM1637_DEMO_END_POINT; point++) {
    g_tm1637_demo_point = point;
    display_pair(point);
    g_tm1637_demo_last_key = tm1637_key_read_raw();
    delay_ms(TM1637_DEMO_STEP_MS);
  }

  g_tm1637_demo_phase = 2U;
  display_self_pass();
  g_tm1637_demo_last_key = tm1637_key_read_raw();
  delay_ms(TM1637_DEMO_PASS_MS);
  g_tm1637_demo_cycle++;
}
