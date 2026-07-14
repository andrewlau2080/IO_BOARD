#include "lcdm_motor_ui.h"

#include "at32f45x_board.h"
#include "lcdm_tjc.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LCDM_W                  480U
#define LCDM_H                  272U
#define LCDM_KEY_Y0             112U
#define LCDM_KEY_Y1             169U
#define LCDM_KEY_H              58U
#define LCDM_KEY_W              120U
#define LCDM_KEY_LONG_MS        3000U
#define LCDM_REFRESH_MS         250U
#define LCDM_TOUCH_DEBUG_MS     2000U
#define LCDM_FONT_ID            0U

#define LCDM_BLACK              0U
#define LCDM_BLUE               31U
#define LCDM_RED                63488U
#define LCDM_GREEN              2016U
#define LCDM_WHITE              65535U
#define LCDM_GRAY               33808U
#define LCDM_DARK_GRAY          16904U
#define LCDM_NAVY               16U
#define LCDM_ROW_BG             61374U

typedef enum {
  LCDM_KEY_NONE = 0,
  LCDM_KEY_K1 = 1,
  LCDM_KEY_K2 = 2,
  LCDM_KEY_K3 = 3,
  LCDM_KEY_K4 = 4
} lcdm_key_t;

typedef enum {
  LCDM_STATE_READY = 0,
  LCDM_STATE_SELF,
  LCDM_STATE_AUTO,
  LCDM_STATE_LEARN,
  LCDM_STATE_PASS,
  LCDM_STATE_NG
} lcdm_state_id_t;

typedef struct {
  const char *state;
  const char *main_text;
  const char *ledm;
  const char *detail;
  const char *sub;
  uint16_t color;
} lcdm_screen_text_t;

typedef struct {
  uint16_t elapsed_ms;
  uint16_t monitor_step;
  uint16_t key_hold_ms;
  uint16_t touch_text_ms;
  lcdm_state_id_t state;
  uint8_t active_key;
  bool k1_long_done;
  char state_cache[16];
  char main_cache[24];
  char ledm_cache[8];
  char detail_cache[28];
  char sub_cache[64];
  char touch_text[64];
} lcdm_ui_t;

volatile uint8_t g_lcdm_motor_page;
volatile uint8_t g_lcdm_motor_selected;
volatile uint16_t g_lcdm_motor_touch_count;
volatile uint16_t g_lcdm_motor_last_x;
volatile uint16_t g_lcdm_motor_last_y;
volatile uint8_t g_lcdm_motor_last_event;
volatile uint8_t g_lcdm_motor_active_key;
volatile uint16_t g_lcdm_motor_key_press_count;
volatile uint16_t g_lcdm_motor_key_release_count;
volatile uint32_t g_lcdm_motor_refresh_count;

static lcdm_ui_t ui;

static void send_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char cmd[48];

  (void)snprintf(cmd, sizeof(cmd), "fill %u,%u,%u,%u,%u", x, y, w, h, color);
  lcdm_tjc_send_cmd(cmd);
}

static void send_text(uint16_t x,
                      uint16_t y,
                      uint16_t w,
                      uint16_t h,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t align,
                      const char *text)
{
  char cmd[320];

  if(text == 0) {
    text = "";
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 x,
                 y,
                 w,
                 h,
                 LCDM_FONT_ID,
                 color,
                 bg,
                 align,
                 text);
  lcdm_tjc_send_cmd(cmd);
}

static void lcdm_runtime_init_cmds(void)
{
  lcdm_tjc_send_cmd("bkcmd=0");
  lcdm_tjc_send_cmd("dim=100");
  lcdm_tjc_send_cmd("sendxy=1");
}

static void clear_text_cache(void)
{
  ui.state_cache[0] = '\0';
  ui.main_cache[0] = '\0';
  ui.ledm_cache[0] = '\0';
  ui.detail_cache[0] = '\0';
  ui.sub_cache[0] = '\0';
}

static const char *key_top(uint8_t key)
{
  switch(key) {
  case LCDM_KEY_K1: return "K1";
  case LCDM_KEY_K2: return "K2";
  case LCDM_KEY_K3: return "K3";
  case LCDM_KEY_K4: return "K4";
  default: return "--";
  }
}

static const char *key_bottom(uint8_t key)
{
  switch(key) {
  case LCDM_KEY_K1: return "SELF/LEARN";
  case LCDM_KEY_K2: return "AUTO";
  case LCDM_KEY_K3: return "RESET";
  case LCDM_KEY_K4: return "OK/SAVE";
  default: return "";
  }
}

static const char *key_name(uint8_t key)
{
  switch(key) {
  case LCDM_KEY_K1: return "K1";
  case LCDM_KEY_K2: return "K2";
  case LCDM_KEY_K3: return "K3";
  case LCDM_KEY_K4: return "K4";
  default: return "--";
  }
}

static void draw_button(uint8_t key)
{
  uint16_t x;
  uint16_t bg;
  uint16_t sub_color;

  if((key < LCDM_KEY_K1) || (key > LCDM_KEY_K4)) {
    return;
  }

  x = (uint16_t)((key - 1U) * LCDM_KEY_W);
  bg = (ui.active_key == key) ? LCDM_BLUE : LCDM_DARK_GRAY;
  sub_color = (ui.active_key == key) ? LCDM_WHITE : LCDM_GRAY;

  send_fill(x, LCDM_KEY_Y0, LCDM_KEY_W, LCDM_KEY_H, bg);
  send_text((uint16_t)(x + 4U), 116U, 112U, 24U, LCDM_WHITE, bg, 1U, key_top(key));
  send_text((uint16_t)(x + 4U), 141U, 112U, 20U, sub_color, bg, 1U, key_bottom(key));
}

static void draw_buttons(void)
{
  draw_button(LCDM_KEY_K1);
  draw_button(LCDM_KEY_K2);
  draw_button(LCDM_KEY_K3);
  draw_button(LCDM_KEY_K4);
}

static void draw_full_frame(void)
{
  lcdm_runtime_init_cmds();
  lcdm_tjc_send_cmd("cls 65535");
  delay_ms(20U);

  send_fill(0U, 0U, LCDM_W, LCDM_H, LCDM_WHITE);
  send_fill(0U, 0U, LCDM_W, 32U, LCDM_NAVY);
  send_text(8U, 3U, 320U, 26U, LCDM_WHITE, LCDM_NAVY, 0U, "WIRE TESTER LCDM");
  send_text(360U, 3U, 110U, 26U, LCDM_WHITE, LCDM_NAVY, 2U, "TESTER");

  send_fill(16U, 42U, 448U, 66U, LCDM_ROW_BG);
  draw_buttons();
  send_fill(16U, 184U, 214U, 34U, LCDM_WHITE);
  send_fill(250U, 184U, 214U, 34U, LCDM_WHITE);
  send_fill(16U, 226U, 448U, 28U, LCDM_WHITE);

  clear_text_cache();
  lcdm_tjc_send_cmd("sendxy=1");
  g_lcdm_motor_page = 0U;
}

static void set_text_cached(char *cache,
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
  send_fill(x, y, w, h, bg);
  send_text(x, y, w, h, fg, bg, align, cache);
}

static lcdm_screen_text_t current_screen_text(void)
{
  lcdm_screen_text_t out;
  uint16_t point = (uint16_t)((ui.monitor_step % 92U) + 1U);
  static char ledm[8];
  static char detail[28];

  switch(ui.state) {
  case LCDM_STATE_SELF:
    (void)snprintf(ledm, sizeof(ledm), "A%02ub%02u", (unsigned int)point, (unsigned int)point);
    (void)snprintf(detail, sizeof(detail), "OUT%03u IN%03u", (unsigned int)point, (unsigned int)point);
    out.state = "SELF";
    out.main_text = "SELF TEST";
    out.ledm = ledm;
    out.detail = detail;
    out.sub = "CHECKING...  K3 RESET";
    out.color = LCDM_BLUE;
    break;

  case LCDM_STATE_AUTO:
    (void)snprintf(ledm, sizeof(ledm), "A%02ub%02u", (unsigned int)point, (unsigned int)point);
    (void)snprintf(detail, sizeof(detail), "OK %03u/092", (unsigned int)point);
    out.state = "AUTO";
    out.main_text = "AUTO TEST";
    out.ledm = ledm;
    out.detail = detail;
    out.sub = "RUNNING  K3 RESET";
    out.color = LCDM_BLUE;
    break;

  case LCDM_STATE_LEARN:
    out.state = "LEARN";
    out.main_text = "LEARN MODE";
    out.ledm = "LEArn";
    out.detail = "OUT048 IN048";
    out.sub = "HOLD K1 3S  K4 SAVE";
    out.color = LCDM_BLUE;
    break;

  case LCDM_STATE_PASS:
    out.state = "PASS";
    out.main_text = "PASS";
    out.ledm = "PASS";
    out.detail = "TOTAL 092 OK";
    out.sub = "PRINT READY  REMOVE HARNESS";
    out.color = LCDM_GREEN;
    break;

  case LCDM_STATE_NG:
    out.state = "NG";
    out.main_text = "NG";
    out.ledm = "001002";
    out.detail = "OUT001 IN002";
    out.sub = "SHORT CIRCUIT  K3 RESET";
    out.color = LCDM_RED;
    break;

  case LCDM_STATE_READY:
  default:
    out.state = "READY";
    out.main_text = "AUTO TEST";
    out.ledm = "AUTO";
    out.detail = "PROFILE DB50";
    out.sub = "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK";
    out.color = LCDM_BLUE;
    break;
  }

  return out;
}

static void render_current_status(void)
{
  lcdm_screen_text_t text = current_screen_text();
  const char *sub = (ui.touch_text_ms != 0U) ? ui.touch_text : text.sub;
  uint16_t sub_color = (ui.touch_text_ms != 0U) ? LCDM_BLUE : LCDM_GRAY;

  set_text_cached(ui.state_cache, sizeof(ui.state_cache), 24U, 48U, 160U, 52U, text.color, LCDM_ROW_BG, 0U, text.state);
  set_text_cached(ui.main_cache, sizeof(ui.main_cache), 184U, 48U, 272U, 52U, text.color, LCDM_ROW_BG, 2U, text.main_text);
  set_text_cached(ui.ledm_cache, sizeof(ui.ledm_cache), 24U, 188U, 198U, 26U, LCDM_BLUE, LCDM_WHITE, 1U, text.ledm);
  set_text_cached(ui.detail_cache, sizeof(ui.detail_cache), 258U, 188U, 198U, 26U, LCDM_BLUE, LCDM_WHITE, 1U, text.detail);
  set_text_cached(ui.sub_cache, sizeof(ui.sub_cache), 24U, 229U, 432U, 22U, sub_color, LCDM_WHITE, 1U, sub);
  g_lcdm_motor_refresh_count++;
}

static uint8_t key_from_coord(uint16_t x, uint16_t y)
{
  if((x > (LCDM_W - 1U)) || (y < LCDM_KEY_Y0) || (y > LCDM_KEY_Y1)) {
    return LCDM_KEY_NONE;
  }
  if(x <= 119U) {
    return LCDM_KEY_K1;
  }
  if(x <= 239U) {
    return LCDM_KEY_K2;
  }
  if(x <= 359U) {
    return LCDM_KEY_K3;
  }
  return LCDM_KEY_K4;
}

static uint8_t key_from_raw_coord(uint16_t raw_x, uint16_t raw_y, uint16_t *mapped_x, uint16_t *mapped_y)
{
  uint8_t key;

  key = key_from_coord(raw_x, raw_y);
  if(key != LCDM_KEY_NONE) {
    *mapped_x = raw_x;
    *mapped_y = raw_y;
    return key;
  }

  if((raw_x < LCDM_H) && (raw_y < LCDM_W)) {
    key = key_from_coord(raw_y, raw_x);
    if(key != LCDM_KEY_NONE) {
      *mapped_x = raw_y;
      *mapped_y = raw_x;
      return key;
    }
  }

  *mapped_x = raw_x;
  *mapped_y = raw_y;
  return LCDM_KEY_NONE;
}

static uint8_t key_from_component(uint8_t component_id)
{
  switch(component_id) {
  case 1U:
  case 11U:
    return LCDM_KEY_K1;
  case 2U:
  case 12U:
    return LCDM_KEY_K2;
  case 3U:
  case 13U:
    return LCDM_KEY_K3;
  case 4U:
  case 14U:
    return LCDM_KEY_K4;
  default:
    return LCDM_KEY_NONE;
  }
}

static uint8_t key_from_ascii(const char *text, uint8_t *touch_event)
{
  if(touch_event != 0) {
    *touch_event = 1U;
  }
  if(text == 0) {
    return LCDM_KEY_NONE;
  }

  if((strcmp(text, "K1_UP") == 0) || (strcmp(text, "key=K1_UP") == 0)) {
    if(touch_event != 0) {
      *touch_event = 0U;
    }
    return LCDM_KEY_K1;
  }
  if((strcmp(text, "K2_UP") == 0) || (strcmp(text, "key=K2_UP") == 0)) {
    if(touch_event != 0) {
      *touch_event = 0U;
    }
    return LCDM_KEY_K2;
  }
  if((strcmp(text, "K3_UP") == 0) || (strcmp(text, "key=K3_UP") == 0)) {
    if(touch_event != 0) {
      *touch_event = 0U;
    }
    return LCDM_KEY_K3;
  }
  if((strcmp(text, "K4_UP") == 0) || (strcmp(text, "key=K4_UP") == 0)) {
    if(touch_event != 0) {
      *touch_event = 0U;
    }
    return LCDM_KEY_K4;
  }

  if((strcmp(text, "K1") == 0) || (strcmp(text, "key=K1") == 0) ||
     (strcmp(text, "K1_DOWN") == 0) || (strcmp(text, "key=K1_DOWN") == 0) ||
     (strcmp(text, "SELF") == 0)) {
    return LCDM_KEY_K1;
  }
  if((strcmp(text, "K2") == 0) || (strcmp(text, "key=K2") == 0) ||
     (strcmp(text, "K2_DOWN") == 0) || (strcmp(text, "key=K2_DOWN") == 0) ||
     (strcmp(text, "AUTO") == 0)) {
    return LCDM_KEY_K2;
  }
  if((strcmp(text, "K3") == 0) || (strcmp(text, "key=K3") == 0) ||
     (strcmp(text, "K3_DOWN") == 0) || (strcmp(text, "key=K3_DOWN") == 0) ||
     (strcmp(text, "RESET") == 0)) {
    return LCDM_KEY_K3;
  }
  if((strcmp(text, "K4") == 0) || (strcmp(text, "key=K4") == 0) ||
     (strcmp(text, "K4_DOWN") == 0) || (strcmp(text, "key=K4_DOWN") == 0) ||
     (strcmp(text, "OK") == 0) || (strcmp(text, "SAVE") == 0)) {
    return LCDM_KEY_K4;
  }

  return LCDM_KEY_NONE;
}

static void set_active_key(uint8_t key)
{
  uint8_t old_key = ui.active_key;

  if(old_key == key) {
    return;
  }

  ui.active_key = key;
  g_lcdm_motor_active_key = key;
  g_lcdm_motor_selected = key;
  draw_button(old_key);
  draw_button(key);
}

static void set_state(lcdm_state_id_t state)
{
  if(ui.state == state) {
    return;
  }

  ui.state = state;
  clear_text_cache();
  render_current_status();
}

static void apply_key_press(uint8_t key)
{
  switch(key) {
  case LCDM_KEY_K1:
    ui.key_hold_ms = 0U;
    ui.k1_long_done = false;
    set_state(LCDM_STATE_SELF);
    break;
  case LCDM_KEY_K2:
    set_state(LCDM_STATE_AUTO);
    break;
  case LCDM_KEY_K3:
    ui.monitor_step = 0U;
    ui.key_hold_ms = 0U;
    ui.k1_long_done = false;
    set_state(LCDM_STATE_READY);
    break;
  case LCDM_KEY_K4:
    set_state((ui.state == LCDM_STATE_LEARN) ? LCDM_STATE_PASS : LCDM_STATE_PASS);
    break;
  default:
    break;
  }
}

static void handle_key_event(uint8_t key, uint8_t touch_event)
{
  if(touch_event == 0U) {
    g_lcdm_motor_key_release_count++;
    if((key == LCDM_KEY_NONE) || (ui.active_key == key)) {
      set_active_key(LCDM_KEY_NONE);
    }
    return;
  }

  if(touch_event != 1U || key == LCDM_KEY_NONE) {
    return;
  }

  if(ui.active_key == key) {
    return;
  }

  g_lcdm_motor_key_press_count++;
  set_active_key(key);
  apply_key_press(key);
}

static void show_coord_debug(uint16_t raw_x,
                             uint16_t raw_y,
                             uint16_t mapped_x,
                             uint16_t mapped_y,
                             uint8_t touch_event,
                             uint8_t key)
{
  (void)snprintf(ui.touch_text,
                 sizeof(ui.touch_text),
                 "T %03u,%03u -> %03u,%03u %s E%u",
                 (unsigned int)raw_x,
                 (unsigned int)raw_y,
                 (unsigned int)mapped_x,
                 (unsigned int)mapped_y,
                 key_name(key),
                 (unsigned int)touch_event);
  ui.touch_text_ms = LCDM_TOUCH_DEBUG_MS;
  ui.sub_cache[0] = '\0';
  render_current_status();
}

static void show_component_debug(uint8_t page, uint8_t component, uint8_t touch_event, uint8_t key)
{
  (void)snprintf(ui.touch_text,
                 sizeof(ui.touch_text),
                 "C page %u id %u %s E%u",
                 (unsigned int)page,
                 (unsigned int)component,
                 key_name(key),
                 (unsigned int)touch_event);
  ui.touch_text_ms = LCDM_TOUCH_DEBUG_MS;
  ui.sub_cache[0] = '\0';
  render_current_status();
}

static void show_ascii_debug(const char *text, uint8_t key, uint8_t touch_event)
{
  if(text == 0) {
    text = "";
  }
  (void)snprintf(ui.touch_text,
                 sizeof(ui.touch_text),
                 "A %.38s -> %s E%u",
                 text,
                 key_name(key),
                 (unsigned int)touch_event);
  ui.touch_text_ms = LCDM_TOUCH_DEBUG_MS;
  ui.sub_cache[0] = '\0';
  render_current_status();
}

static void process_events(void)
{
  lcdm_tjc_event_t event;

  while(lcdm_tjc_poll_event(&event) != 0U) {
    uint8_t key = LCDM_KEY_NONE;
    uint8_t touch_event = event.touch_event;

    if(event.type == LCDM_TJC_EVENT_TOUCH_COORD) {
      uint16_t mapped_x;
      uint16_t mapped_y;

      key = key_from_raw_coord(event.x, event.y, &mapped_x, &mapped_y);
      g_lcdm_motor_touch_count++;
      g_lcdm_motor_last_x = event.x;
      g_lcdm_motor_last_y = event.y;
      g_lcdm_motor_last_event = event.touch_event;
      show_coord_debug(event.x, event.y, mapped_x, mapped_y, event.touch_event, key);
      handle_key_event(key, event.touch_event);
    } else if(event.type == LCDM_TJC_EVENT_TOUCH) {
      key = key_from_component(event.component_id);
      g_lcdm_motor_touch_count++;
      g_lcdm_motor_last_x = (uint16_t)event.component_id;
      g_lcdm_motor_last_y = event.page_id;
      g_lcdm_motor_last_event = event.touch_event;
      show_component_debug(event.page_id, event.component_id, event.touch_event, key);
      handle_key_event(key, event.touch_event);
    } else if(event.type == LCDM_TJC_EVENT_ASCII) {
      key = key_from_ascii(event.ascii, &touch_event);
      g_lcdm_motor_touch_count++;
      g_lcdm_motor_last_x = 0U;
      g_lcdm_motor_last_y = 0U;
      g_lcdm_motor_last_event = touch_event;
      show_ascii_debug(event.ascii, key, touch_event);
      handle_key_event(key, touch_event);
    } else {
      /* No action. */
    }
  }
}

void lcdm_motor_ui_init(void)
{
  memset(&ui, 0, sizeof(ui));
  ui.state = LCDM_STATE_READY;

  g_lcdm_motor_page = 0U;
  g_lcdm_motor_selected = LCDM_KEY_NONE;
  g_lcdm_motor_touch_count = 0U;
  g_lcdm_motor_last_x = 0U;
  g_lcdm_motor_last_y = 0U;
  g_lcdm_motor_last_event = 0U;
  g_lcdm_motor_active_key = LCDM_KEY_NONE;
  g_lcdm_motor_key_press_count = 0U;
  g_lcdm_motor_key_release_count = 0U;
  g_lcdm_motor_refresh_count = 0U;

  lcdm_tjc_init();
  lcdm_tjc_force_baudrate(LCDM_TJC_BAUDRATE);
  delay_ms(150U);
  draw_full_frame();
  render_current_status();
}

void lcdm_motor_ui_service(void)
{
  process_events();
  delay_ms(1U);

  ui.elapsed_ms++;
  if(ui.touch_text_ms != 0U) {
    ui.touch_text_ms--;
    if(ui.touch_text_ms == 0U) {
      ui.sub_cache[0] = '\0';
      render_current_status();
    }
  }

  if(ui.active_key == LCDM_KEY_K1) {
    if(ui.key_hold_ms < LCDM_KEY_LONG_MS) {
      ui.key_hold_ms++;
    }
    if((ui.key_hold_ms >= LCDM_KEY_LONG_MS) && (!ui.k1_long_done)) {
      ui.k1_long_done = true;
      set_state(LCDM_STATE_LEARN);
    }
  }

  if(ui.elapsed_ms >= LCDM_REFRESH_MS) {
    ui.elapsed_ms = 0U;
    if((ui.state == LCDM_STATE_SELF) || (ui.state == LCDM_STATE_AUTO)) {
      ui.monitor_step++;
      ui.ledm_cache[0] = '\0';
      ui.detail_cache[0] = '\0';
    }
    render_current_status();
  }
}
