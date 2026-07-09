#include "first_gen_display.h"

#include "lcdm_tjc.h"
#include "tm1637_display.h"

#include <string.h>

#ifndef FIRST_GEN_DISPLAY_BACKEND_LCDM
#define FIRST_GEN_DISPLAY_BACKEND_LCDM 0
#endif

#if FIRST_GEN_DISPLAY_BACKEND_LCDM

#define LCDM_PAGE_TESTER_MAIN       0U
#define LCDM_TOUCH_K1               11U
#define LCDM_TOUCH_K2               12U
#define LCDM_TOUCH_K3               13U
#define LCDM_TOUCH_K4               14U

static uint8_t lcdm_current_key = FIRST_GEN_KEY_NONE;

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

static void lcdm_set_main_from_text(const char *text)
{
  if(text == 0 || text[0] == '\0') {
    lcdm_tjc_set_text("tState", "IDLE");
    lcdm_tjc_set_text("tMain", "");
    return;
  }

  if(strncmp(text, "PASS", 4U) == 0) {
    lcdm_tjc_set_text("tState", "PASS");
    lcdm_tjc_set_text("tMain", "PASS");
  } else if(strncmp(text, "NG", 2U) == 0) {
    lcdm_tjc_set_text("tState", "NG");
    lcdm_tjc_set_text("tMain", "NG");
  } else if(strncmp(text, "LEArn", 5U) == 0) {
    lcdm_tjc_set_text("tState", "LEARN");
    lcdm_tjc_set_text("tMain", "LEARN");
  } else if(strncmp(text, "AUTO", 4U) == 0) {
    lcdm_tjc_set_text("tState", "READY");
    lcdm_tjc_set_text("tMain", "AUTO TEST");
  } else if(strncmp(text, "SAUEd", 5U) == 0) {
    lcdm_tjc_set_text("tState", "SAVED");
    lcdm_tjc_set_text("tMain", "SAVED");
  } else if(strncmp(text, "SELF", 4U) == 0) {
    lcdm_tjc_set_text("tState", "SELF");
    lcdm_tjc_set_text("tMain", "SELF PASS");
  } else if(strncmp(text, "Prnt", 4U) == 0) {
    lcdm_tjc_set_text("tState", "PRINT");
    lcdm_tjc_set_text("tMain", "PRINT READY");
  } else if(strncmp(text, "Printd", 6U) == 0) {
    lcdm_tjc_set_text("tState", "PRINT");
    lcdm_tjc_set_text("tMain", "PRINT DONE");
  } else if(strncmp(text, "Er", 2U) == 0) {
    lcdm_tjc_set_text("tState", "ERROR");
    lcdm_tjc_set_text("tMain", text);
  } else {
    lcdm_tjc_set_text("tMain", text);
  }
}

static uint8_t lcdm_component_to_key(uint8_t component_id)
{
  switch(component_id) {
  case LCDM_TOUCH_K1: return FIRST_GEN_KEY_SET;
  case LCDM_TOUCH_K2: return FIRST_GEN_KEY_CLEAR;
  case LCDM_TOUCH_K3: return FIRST_GEN_KEY_PLUS;
  case LCDM_TOUCH_K4: return FIRST_GEN_KEY_MINUS;
  default: return FIRST_GEN_KEY_NONE;
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

void first_gen_display_init(void)
{
  lcdm_current_key = FIRST_GEN_KEY_NONE;
  lcdm_tjc_init();
  lcdm_tjc_page(LCDM_PAGE_TESTER_MAIN);
  lcdm_tjc_set_text("tTitle", "WIRE TESTER");
  lcdm_tjc_set_text("tState", "BOOT");
  lcdm_tjc_set_text("tMain", "WIRE TESTER");
  lcdm_tjc_set_text("tLedm", "BOOT");
  lcdm_tjc_set_text("tSub", "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK");
}

void first_gen_display_clear(void)
{
  lcdm_tjc_set_text("tLedm", "");
  lcdm_tjc_set_text("tMain", "");
}

uint8_t first_gen_display_key_read_raw(void)
{
  lcdm_tjc_event_t event;
  uint8_t key;

  while(lcdm_tjc_poll_event(&event) != 0U) {
    if(event.type == LCDM_TJC_EVENT_TOUCH) {
      key = lcdm_component_to_key(event.component_id);
      if(key != FIRST_GEN_KEY_NONE) {
        lcdm_current_key = (event.touch_event != 0U) ? key : FIRST_GEN_KEY_NONE;
      }
    } else if(event.type == LCDM_TJC_EVENT_ASCII) {
      key = ascii_key_name_to_key(event.ascii);
      if(key != FIRST_GEN_KEY_NONE) {
        return key;
      }
    }
  }

  return lcdm_current_key;
}

void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS])
{
  char text[8];
  uint8_t i;

  if(segments == 0) {
    return;
  }

  for(i = 0U; i < FIRST_GEN_DISPLAY_DIGITS; i++) {
    text[i] = (segments[i] == 0U) ? ' ' : '-';
  }
  text[FIRST_GEN_DISPLAY_DIGITS] = '\0';
  lcdm_tjc_set_text("tState", "TESTING");
  lcdm_tjc_set_text("tLedm", text);
  lcdm_tjc_set_text("tMain", "TESTING");
}

void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS])
{
  char value[8];

  if(text == 0) {
    return;
  }

  text6_to_cstr(text, value);
  lcdm_tjc_set_text("tLedm", value);
  lcdm_set_main_from_text(value);
}

#else

void first_gen_display_init(void)
{
  tm1637_display_init();
}

void first_gen_display_clear(void)
{
  tm1637_display_clear();
}

uint8_t first_gen_display_key_read_raw(void)
{
  return tm1637_key_read_raw();
}

void first_gen_display_write_raw6(const uint8_t segments[FIRST_GEN_DISPLAY_DIGITS])
{
  tm1637_display_write_raw6(segments);
}

void first_gen_display_write_text6(const char text[FIRST_GEN_DISPLAY_DIGITS])
{
  tm1637_display_write_text6(text);
}

#endif
