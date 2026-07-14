#include "first_gen_display.h"

#include "at32f45x_board.h"
#include "lcdm_tjc.h"
#include "tm1637_display.h"

#include <stdio.h>
#include <string.h>

#ifndef FIRST_GEN_DISPLAY_BACKEND_LCDM
#define FIRST_GEN_DISPLAY_BACKEND_LCDM 0
#endif

#ifndef FIRST_GEN_DISPLAY_AUTO_DETECT
#define FIRST_GEN_DISPLAY_AUTO_DETECT 1
#endif

#define LCDM_TOUCH_K1               11U
#define LCDM_TOUCH_K2               12U
#define LCDM_TOUCH_K3               13U
#define LCDM_TOUCH_K4               14U
#define LCDM_KEY_HOLD_READS         80U
#define LCDM_W                      480U
#define LCDM_H                      272U
#define LCDM_KEY_Y0                 112U
#define LCDM_KEY_Y1                 169U
#define LCDM_KEY_W                  120U
#define LCDM_KEY_H                  58U
#define LCDM_BLUE                   31U
#define LCDM_RED                    63488U
#define LCDM_GREEN                  2016U
#define LCDM_WHITE                  65535U
#define LCDM_GRAY                   33808U
#define LCDM_DARK_GRAY              16904U
#define LCDM_NAVY                   16U
#define LCDM_ROW_BG                 61374U

static uint8_t lcdm_current_key = FIRST_GEN_KEY_NONE;
static uint16_t lcdm_key_hold_reads;
static uint8_t lcdm_key_waits_release;
static uint8_t display_is_lcdm;
static char lcdm_raw_state_cache[16];
static char lcdm_raw_main_cache[24];
static char lcdm_raw_ledm_cache[8];
static char lcdm_raw_detail_cache[28];
static char lcdm_raw_sub_cache[64];
static char lcdm_raw_touch_cache[64];

volatile uint32_t g_first_gen_lcdm_touch_count;
volatile uint32_t g_first_gen_lcdm_key_press_count;
volatile uint32_t g_first_gen_lcdm_key_release_count;
volatile uint8_t g_first_gen_lcdm_last_event_type;
volatile uint8_t g_first_gen_lcdm_last_touch_event;
volatile uint8_t g_first_gen_lcdm_last_key;
volatile uint16_t g_first_gen_lcdm_last_x;
volatile uint16_t g_first_gen_lcdm_last_y;

static void text6_to_cstr(const char text[FIRST_GEN_DISPLAY_DIGITS], char out[8])
{
  uint8_t i;
  int8_t end = (int8_t)FIRST_GEN_DISPLAY_DIGITS - 1;

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    out[i] = text[i];
  }
  out[FIRST_GEN_DISPLAY_DIGITS] = '\0';

  while(end >= 0 && out[end] == ' ') {
    out[end] = '\0';
    end--;
  }
}

static void lcdm_raw_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char cmd[48];

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "fill %u,%u,%u,%u,%u",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)color);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_raw_xstr(uint16_t x,
                          uint16_t y,
                          uint16_t w,
                          uint16_t h,
                          uint16_t font,
                          uint16_t fg,
                          uint16_t bg,
                          uint8_t align,
                          const char *text)
{
  char cmd[192];

  if(text == 0) {
    text = "";
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 (unsigned int)x,
                 (unsigned int)y,
                 (unsigned int)w,
                 (unsigned int)h,
                 (unsigned int)font,
                 (unsigned int)fg,
                 (unsigned int)bg,
                 (unsigned int)align,
                 text);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_raw_write_cached(char *cache,
                                  uint8_t cache_len,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t w,
                                  uint16_t h,
                                  uint16_t fg,
                                  uint16_t bg,
                                  uint8_t align,
                                  const char *text)
{
  if(text == 0) {
    text = "";
  }

  if(strncmp(cache, text, cache_len) == 0) {
    return;
  }

  (void)snprintf(cache, cache_len, "%s", text);
  lcdm_raw_fill(x, y, w, h, bg);
  lcdm_raw_xstr(x, y, w, h, 0U, fg, bg, align, cache);
}

static void lcdm_raw_draw_key(uint8_t index, uint8_t active)
{
  uint16_t x = (uint16_t)(index * LCDM_KEY_W);
  uint16_t bg = (active != 0U) ? LCDM_BLUE : LCDM_DARK_GRAY;
  uint16_t sub = (active != 0U) ? LCDM_WHITE : LCDM_GRAY;
  const char *top = "K1";
  const char *bottom = "SELF/LEARN";

  if(index == 1U) {
    top = "K2";
    bottom = "AUTO";
  } else if(index == 2U) {
    top = "K3";
    bottom = "RESET";
  } else if(index == 3U) {
    top = "K4";
    bottom = "OK/SAVE";
  }

  lcdm_raw_fill(x, LCDM_KEY_Y0, LCDM_KEY_W, LCDM_KEY_H, bg);
  lcdm_raw_xstr((uint16_t)(x + 4U), 116U, 112U, 24U, 0U, LCDM_WHITE, bg, 1U, top);
  lcdm_raw_xstr((uint16_t)(x + 4U), 141U, 112U, 20U, 0U, sub, bg, 1U, bottom);
}

static void lcdm_raw_draw_keys(void)
{
  lcdm_raw_draw_key(0U, lcdm_current_key == FIRST_GEN_KEY_SET);
  lcdm_raw_draw_key(1U, lcdm_current_key == FIRST_GEN_KEY_CLEAR);
  lcdm_raw_draw_key(2U, lcdm_current_key == FIRST_GEN_KEY_PLUS);
  lcdm_raw_draw_key(3U, lcdm_current_key == FIRST_GEN_KEY_MINUS);
}

static void lcdm_raw_draw_test_frame(void)
{
  lcdm_tjc_send_cmd("cls 65535");
  lcdm_raw_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  lcdm_raw_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  lcdm_raw_xstr(8U, 3U, 320U, 26U, 0U, LCDM_WHITE, LCDM_NAVY, 0U, "WIRE TESTER LCDM");
  lcdm_raw_xstr(360U, 3U, 110U, 26U, 0U, LCDM_WHITE, LCDM_NAVY, 2U, "TESTER");
  lcdm_raw_fill(16U, 42U, 448U, 66U, LCDM_ROW_BG);
  lcdm_raw_draw_keys();
  lcdm_raw_fill(16U, 184U, 214U, 34U, LCDM_WHITE);
  lcdm_raw_fill(250U, 184U, 214U, 34U, LCDM_WHITE);
  lcdm_raw_fill(16U, 226U, 448U, 28U, LCDM_WHITE);
  lcdm_raw_state_cache[0] = '\0';
  lcdm_raw_main_cache[0] = '\0';
  lcdm_raw_ledm_cache[0] = '\0';
  lcdm_raw_detail_cache[0] = '\0';
  lcdm_raw_sub_cache[0] = '\0';
  lcdm_raw_touch_cache[0] = '\0';
  lcdm_tjc_send_cmd("sendxy=1");
}

static void lcdm_raw_update(const char *state, const char *value, uint16_t state_color)
{
  const char *state_text = state;
  const char *main_text = state;
  const char *ledm_text = value;
  const char *detail_text = value;
  const char *sub_text = "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK";

  if(state == 0) {
    state = "";
  }
  if(value == 0) {
    value = "";
  }

  if(strncmp(value, "AUTO", 4U) == 0) {
    state_text = "READY";
    main_text = "AUTO TEST";
    detail_text = "PROFILE DB50";
  } else if(strncmp(state, "SELF", 4U) == 0) {
    main_text = "SELF TEST";
    detail_text = value;
    sub_text = "CHECKING...  K3 RESET";
  } else if(strncmp(state, "AUTO", 4U) == 0 || strncmp(state, "TEST", 4U) == 0) {
    state_text = "AUTO";
    main_text = "AUTO TEST";
    detail_text = value;
    sub_text = "RUNNING  K3 RESET";
  } else if(strncmp(state, "LEARN", 5U) == 0) {
    main_text = "LEARN MODE";
    detail_text = "OUT048 IN048";
    sub_text = "HOLD K1 3S  K4 SAVE";
  } else if(strncmp(state, "PASS", 4U) == 0) {
    main_text = "PASS";
    detail_text = "TOTAL 092 OK";
    sub_text = "PRINT READY  REMOVE HARNESS";
  } else if(strncmp(state, "NG", 2U) == 0) {
    main_text = "NG";
    detail_text = value;
    sub_text = "SHORT CIRCUIT  K3 RESET";
  } else if(strncmp(state, "PRINT", 5U) == 0) {
    main_text = value;
    detail_text = "PRINT READY";
  }

  lcdm_raw_write_cached(lcdm_raw_state_cache, sizeof(lcdm_raw_state_cache), 24U, 48U, 160U, 52U, state_color, LCDM_ROW_BG, 0U, state_text);
  lcdm_raw_write_cached(lcdm_raw_main_cache, sizeof(lcdm_raw_main_cache), 184U, 48U, 272U, 52U, state_color, LCDM_ROW_BG, 2U, main_text);
  lcdm_raw_write_cached(lcdm_raw_ledm_cache, sizeof(lcdm_raw_ledm_cache), 24U, 188U, 198U, 26U, LCDM_BLUE, LCDM_WHITE, 1U, ledm_text);
  lcdm_raw_write_cached(lcdm_raw_detail_cache, sizeof(lcdm_raw_detail_cache), 258U, 188U, 198U, 26U, LCDM_BLUE, LCDM_WHITE, 1U, detail_text);
  lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, 229U, 432U, 22U, LCDM_GRAY, LCDM_WHITE, 1U, sub_text);
}

static uint8_t lcdm_coord_to_key_direct(uint16_t x, uint16_t y)
{
  if((x > (LCDM_W - 1U)) || (y < LCDM_KEY_Y0) || (y > LCDM_KEY_Y1)) {
    return FIRST_GEN_KEY_NONE;
  }

  if(x <= 119U) {
    return FIRST_GEN_KEY_SET;
  }
  if(x <= 239U) {
    return FIRST_GEN_KEY_CLEAR;
  }
  if(x <= 359U) {
    return FIRST_GEN_KEY_PLUS;
  }
  return FIRST_GEN_KEY_MINUS;
}

static uint8_t lcdm_coord_to_key(uint16_t x, uint16_t y)
{
  uint8_t key = lcdm_coord_to_key_direct(x, y);

  if(key != FIRST_GEN_KEY_NONE) {
    return key;
  }

  if((x < LCDM_H) && (y < LCDM_W)) {
    return lcdm_coord_to_key_direct(y, x);
  }

  return FIRST_GEN_KEY_NONE;
}

static void lcdm_raw_show_touch(uint16_t x, uint16_t y, uint8_t key)
{
  char text[32];
  const char *key_name = "---";

  switch(key) {
  case FIRST_GEN_KEY_SET: key_name = "K1"; break;
  case FIRST_GEN_KEY_CLEAR: key_name = "K2"; break;
  case FIRST_GEN_KEY_PLUS: key_name = "K3"; break;
  case FIRST_GEN_KEY_MINUS: key_name = "K4"; break;
  default: break;
  }

  (void)snprintf(text,
                 sizeof(text),
                 "T %03u,%03u -> %s",
                 (unsigned int)x,
                 (unsigned int)y,
                 key_name);
  if(strcmp(lcdm_raw_touch_cache, text) != 0) {
    (void)snprintf(lcdm_raw_touch_cache, sizeof(lcdm_raw_touch_cache), "%s", text);
    lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, 229U, 432U, 22U, LCDM_BLUE, LCDM_WHITE, 1U, lcdm_raw_touch_cache);
  }
}

static void lcdm_raw_show_touch_event(const lcdm_tjc_event_t *event, uint8_t key)
{
  char text[32];
  const char *key_name = "---";

  if(event == 0) {
    return;
  }

  switch(key) {
  case FIRST_GEN_KEY_SET: key_name = "K1"; break;
  case FIRST_GEN_KEY_CLEAR: key_name = "K2"; break;
  case FIRST_GEN_KEY_PLUS: key_name = "K3"; break;
  case FIRST_GEN_KEY_MINUS: key_name = "K4"; break;
  default: break;
  }

  (void)snprintf(text,
                 sizeof(text),
                 "T P=%u C=%u E=%u %s",
                 (unsigned int)event->page_id,
                 (unsigned int)event->component_id,
                 (unsigned int)event->touch_event,
                 key_name);
  if(strcmp(lcdm_raw_touch_cache, text) != 0) {
    (void)snprintf(lcdm_raw_touch_cache, sizeof(lcdm_raw_touch_cache), "%s", text);
    lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, 229U, 432U, 22U, LCDM_BLUE, LCDM_WHITE, 1U, lcdm_raw_touch_cache);
  }
}

static void lcdm_raw_show_ascii_event(const lcdm_tjc_event_t *event)
{
  char text[32];
  uint8_t b0 = 0U;
  uint8_t b1 = 0U;
  uint8_t b2 = 0U;

  if(event == 0) {
    return;
  }

  if(event->len > 0U) {
    b0 = (uint8_t)event->ascii[0];
  }
  if(event->len > 1U) {
    b1 = (uint8_t)event->ascii[1];
  }
  if(event->len > 2U) {
    b2 = (uint8_t)event->ascii[2];
  }

  (void)snprintf(text,
                 sizeof(text),
                 "R L=%u %02X %02X %02X",
                 (unsigned int)event->len,
                 (unsigned int)b0,
                 (unsigned int)b1,
                 (unsigned int)b2);
  if(strcmp(lcdm_raw_touch_cache, text) != 0) {
    (void)snprintf(lcdm_raw_touch_cache, sizeof(lcdm_raw_touch_cache), "%s", text);
    lcdm_raw_write_cached(lcdm_raw_sub_cache, sizeof(lcdm_raw_sub_cache), 24U, 229U, 432U, 22U, LCDM_BLUE, LCDM_WHITE, 1U, lcdm_raw_touch_cache);
  }
}

static uint8_t ascii_key_name_to_key(const char *text)
{
  if(text == 0) {
    return FIRST_GEN_KEY_NONE;
  }

  if(strcmp(text, "K1") == 0 || strcmp(text, "key=K1") == 0 ||
     strcmp(text, "SELF") == 0) {
    return FIRST_GEN_KEY_SET;
  }
  if(strcmp(text, "K2") == 0 || strcmp(text, "key=K2") == 0 ||
     strcmp(text, "AUTO") == 0) {
    return FIRST_GEN_KEY_CLEAR;
  }
  if(strcmp(text, "K3") == 0 || strcmp(text, "key=K3") == 0 ||
     strcmp(text, "RESET") == 0) {
    return FIRST_GEN_KEY_PLUS;
  }
  if(strcmp(text, "K4") == 0 || strcmp(text, "key=K4") == 0 ||
     strcmp(text, "OK") == 0 || strcmp(text, "SAVE") == 0) {
    return FIRST_GEN_KEY_MINUS;
  }

  if(strcmp(text, "K1_DOWN") == 0 || strcmp(text, "key=K1_DOWN") == 0) {
    lcdm_current_key = FIRST_GEN_KEY_SET;
    return lcdm_current_key;
  }
  if(strcmp(text, "K1_UP") == 0 || strcmp(text, "key=K1_UP") == 0) {
    lcdm_current_key = FIRST_GEN_KEY_NONE;
    return lcdm_current_key;
  }

  return FIRST_GEN_KEY_NONE;
}

static uint8_t lcdm_component_to_key(uint8_t component_id)
{
  switch(component_id) {
  case 1U:
  case LCDM_TOUCH_K1:
    return FIRST_GEN_KEY_SET;

  case 2U:
  case LCDM_TOUCH_K2:
    return FIRST_GEN_KEY_CLEAR;

  case 3U:
  case LCDM_TOUCH_K3:
    return FIRST_GEN_KEY_PLUS;

  case 4U:
  case LCDM_TOUCH_K4:
    return FIRST_GEN_KEY_MINUS;

  default:
    return FIRST_GEN_KEY_NONE;
  }
}

static uint8_t lcdm_latch_key(uint8_t key)
{
  if(key == FIRST_GEN_KEY_NONE) {
    return FIRST_GEN_KEY_NONE;
  }

  lcdm_current_key = key;
  g_first_gen_lcdm_last_key = key;
  g_first_gen_lcdm_key_press_count++;
  lcdm_key_hold_reads = 0U;
  lcdm_key_waits_release = 1U;
  lcdm_raw_draw_keys();
  return key;
}

static uint8_t lcdm_pulse_key(uint8_t key)
{
  if(key == FIRST_GEN_KEY_NONE) {
    return FIRST_GEN_KEY_NONE;
  }

  lcdm_current_key = key;
  g_first_gen_lcdm_last_key = key;
  g_first_gen_lcdm_key_press_count++;
  lcdm_key_hold_reads = LCDM_KEY_HOLD_READS;
  lcdm_key_waits_release = 0U;
  lcdm_raw_draw_keys();
  return key;
}

static void lcdm_release_key(uint8_t key)
{
  if((key == FIRST_GEN_KEY_NONE) || (lcdm_current_key == key)) {
    lcdm_current_key = FIRST_GEN_KEY_NONE;
    g_first_gen_lcdm_last_key = FIRST_GEN_KEY_NONE;
    g_first_gen_lcdm_key_release_count++;
    lcdm_key_hold_reads = 0U;
    lcdm_key_waits_release = 0U;
    lcdm_raw_draw_keys();
  }
}

static void lcdm_display_init(void)
{
  lcdm_current_key = FIRST_GEN_KEY_NONE;
  lcdm_key_hold_reads = 0U;
  lcdm_key_waits_release = 0U;
  g_first_gen_lcdm_touch_count = 0U;
  g_first_gen_lcdm_key_press_count = 0U;
  g_first_gen_lcdm_key_release_count = 0U;
  g_first_gen_lcdm_last_event_type = 0U;
  g_first_gen_lcdm_last_touch_event = 0U;
  g_first_gen_lcdm_last_key = FIRST_GEN_KEY_NONE;
  g_first_gen_lcdm_last_x = 0U;
  g_first_gen_lcdm_last_y = 0U;
  lcdm_tjc_init();
  lcdm_tjc_force_baudrate(LCDM_TJC_BAUDRATE);
  delay_ms(150U);
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("dim=100");
  lcdm_tjc_send_cmd("sendxy=1");
  lcdm_raw_draw_test_frame();
  lcdm_raw_update("READY", "AUTO", LCDM_BLUE);
}

static uint8_t lcdm_display_key_read_raw(void)
{
  lcdm_tjc_event_t event;
  uint8_t key;

  while(lcdm_tjc_poll_event(&event) != 0U) {
    g_first_gen_lcdm_touch_count++;
    g_first_gen_lcdm_last_event_type = (uint8_t)event.type;
    g_first_gen_lcdm_last_touch_event = event.touch_event;
    if(event.type == LCDM_TJC_EVENT_TOUCH) {
      key = lcdm_component_to_key(event.component_id);
      g_first_gen_lcdm_last_x = (uint16_t)event.component_id;
      g_first_gen_lcdm_last_y = event.page_id;
      lcdm_raw_show_touch_event(&event, key);
      if(event.touch_event == 0U) {
        lcdm_release_key(key);
        return FIRST_GEN_KEY_NONE;
      }
      if(key != FIRST_GEN_KEY_NONE) {
        return lcdm_latch_key(key);
      }
    } else if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      key = lcdm_coord_to_key(event.x, event.y);
      g_first_gen_lcdm_last_x = event.x;
      g_first_gen_lcdm_last_y = event.y;
      lcdm_raw_show_touch(event.x, event.y, key);
      if(event.touch_event == 0U) {
        lcdm_release_key(key);
        return FIRST_GEN_KEY_NONE;
      }
      if(key != FIRST_GEN_KEY_NONE) {
        return lcdm_latch_key(key);
      }
    } else if(event.type == LCDM_TJC_EVENT_ASCII) {
      lcdm_raw_show_ascii_event(&event);
      key = ascii_key_name_to_key(event.ascii);
      if(key != FIRST_GEN_KEY_NONE) {
        if(strstr(event.ascii, "_DOWN") != 0) {
          return lcdm_latch_key(key);
        }
        if(strstr(event.ascii, "_UP") != 0) {
          lcdm_release_key(key);
          return FIRST_GEN_KEY_NONE;
        }
        return lcdm_pulse_key(key);
      }
    }
  }

  if(lcdm_key_hold_reads != 0U) {
    lcdm_key_hold_reads--;
    return lcdm_current_key;
  }

  if(lcdm_key_waits_release == 0U) {
    lcdm_current_key = FIRST_GEN_KEY_NONE;
  }
  return lcdm_current_key;
}

void first_gen_display_init(void)
{
#if FIRST_GEN_DISPLAY_BACKEND_LCDM
  display_is_lcdm = 1U;
#elif FIRST_GEN_DISPLAY_AUTO_DETECT
  display_is_lcdm = lcdm_tjc_probe(0U);
#else
  display_is_lcdm = 0U;
#endif

  if(display_is_lcdm != 0U) {
    lcdm_display_init();
  } else {
    tm1637_display_init();
  }
}

void first_gen_display_clear(void)
{
  if(display_is_lcdm != 0U) {
    lcdm_raw_update("READY", "AUTO", LCDM_BLUE);
  } else {
    tm1637_display_clear();
  }
}

uint8_t first_gen_display_key_read_raw(void)
{
  if(display_is_lcdm != 0U) {
    return lcdm_display_key_read_raw();
  }
  return tm1637_key_read_raw();
}

void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS])
{
  char text[8];
  uint8_t i;

  if(display_is_lcdm == 0U) {
    tm1637_display_write_raw6(segments);
    return;
  }

  if(segments == 0) {
    return;
  }

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    text[i] = (segments[i] == 0U) ? ' ' : '-';
  }
  text[FIRST_GEN_DISPLAY_DIGITS] = '\0';
  lcdm_raw_update("TEST", text, LCDM_BLUE);
}

void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS])
{
  char value[8];

  if(display_is_lcdm == 0U) {
    tm1637_display_write_text6(text);
    return;
  }

  if(text == 0) {
    return;
  }

  text6_to_cstr(text, value);

  if(strncmp(value, "PASS", 4U) == 0) {
    lcdm_raw_update("PASS", value, LCDM_GREEN);
  } else if(strncmp(value, "NG", 2U) == 0 || strncmp(value, "Er", 2U) == 0) {
    lcdm_raw_update("NG", value, LCDM_RED);
  } else if(strncmp(value, "Prnt", 4U) == 0 || strncmp(value, "Printd", 6U) == 0) {
    lcdm_raw_update("PRINT", value, LCDM_BLUE);
  } else if(strncmp(value, "LEArn", 5U) == 0) {
    lcdm_raw_update("LEARN", value, LCDM_BLUE);
  } else if(strncmp(value, "AUTO", 4U) == 0) {
    lcdm_raw_update("AUTO", value, LCDM_BLUE);
  } else if(strncmp(value, "SELF", 4U) == 0) {
    lcdm_raw_update("SELF", value, LCDM_BLUE);
  } else {
    lcdm_raw_update("TEST", value, LCDM_BLUE);
  }
}
