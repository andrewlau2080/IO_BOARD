#include "tester_wifi_net_diag.h"

#include "at32f45x_board.h"
#include "first_gen_display.h"
#include "tester_wifi_print.h"
#include "wifi_net_diag_config.h"

#include <stdio.h>
#include <string.h>

#define WIFI_NET_DIAG_STEP_MS              1U
#define WIFI_NET_DIAG_COMMAND_TIMEOUT_MS   2500U
#define WIFI_NET_DIAG_JOIN_TIMEOUT_MS      20000U
#define WIFI_NET_DIAG_RESULT_HOLD_MS       10000U
#define WIFI_NET_DIAG_FAIL_HOLD_MS         3000U
#define WIFI_NET_DIAG_TEXT_MAX             64U
#define WIFI_NET_DIAG_IP_MAX               20U
#define WIFI_NET_DIAG_JOIN_COMMAND_MAX     128U

typedef enum {
  WIFI_NET_COMMAND_NONE = 0,
  WIFI_NET_COMMAND_AT,
  WIFI_NET_COMMAND_GMR,
  WIFI_NET_COMMAND_MODE_QUERY,
  WIFI_NET_COMMAND_AP_QUERY,
  WIFI_NET_COMMAND_SET_STA_MODE,
  WIFI_NET_COMMAND_JOIN_AP,
  WIFI_NET_COMMAND_IP_QUERY
} wifi_net_command_t;

static wifi_net_command_t wifi_net_waiting_command;
static wifi_net_command_t wifi_net_pending_command;
static uint32_t wifi_net_wait_ms;
static uint32_t wifi_net_hold_ms;
static uint8_t wifi_net_station_connected;
static uint8_t wifi_net_got_ip;
static char wifi_net_ap[WIFI_NET_DIAG_TEXT_MAX];
static char wifi_net_ip[WIFI_NET_DIAG_IP_MAX];
static char wifi_net_version[WIFI_NET_DIAG_TEXT_MAX];

static void wifi_net_draw_wait(const char *status,
                               const char *main_text,
                               const char *result,
                               const char *detail)
{
  first_gen_display_show_page("NET-DIAG",
                              status,
                              main_text,
                              result,
                              detail,
                              FIRST_GEN_DISPLAY_COLOR_BLUE,
                              FIRST_GEN_DISPLAY_COLOR_PALE_CYAN,
                              FIRST_GEN_DISPLAY_COLOR_BLUE);
}

static void wifi_net_draw_pass(const char *status,
                               const char *main_text,
                               const char *result,
                               const char *detail)
{
  first_gen_display_show_page("NET-DIAG",
                              status,
                              main_text,
                              result,
                              detail,
                              FIRST_GEN_DISPLAY_COLOR_GREEN,
                              FIRST_GEN_DISPLAY_COLOR_GREEN,
                              FIRST_GEN_DISPLAY_COLOR_WHITE);
}

static void wifi_net_draw_fail(const char *status,
                               const char *main_text,
                               const char *result,
                               const char *detail)
{
  first_gen_display_show_page("NET-DIAG",
                              status,
                              main_text,
                              result,
                              detail,
                              FIRST_GEN_DISPLAY_COLOR_RED,
                              FIRST_GEN_DISPLAY_COLOR_PALE_PINK,
                              FIRST_GEN_DISPLAY_COLOR_RED);
}

static uint8_t wifi_net_has_join_config(void)
{
  /* An empty password is valid for an explicitly configured open test AP. */
  return (WIFI_NET_DIAG_SSID[0] != '\0') ? 1U : 0U;
}

static void wifi_net_copy_quoted(const char *line, char *out, uint8_t out_size)
{
  const char *start;
  const char *end;
  size_t length;

  if(line == 0 || out == 0 || out_size == 0U) {
    return;
  }

  start = strchr(line, '"');
  if(start == 0) {
    return;
  }
  end = strchr(start + 1, '"');
  if(end == 0) {
    return;
  }

  length = (size_t)(end - (start + 1));
  if(length >= out_size) {
    length = (size_t)(out_size - 1U);
  }
  memcpy(out, start + 1, length);
  out[length] = '\0';
}

static uint8_t wifi_net_append_char(char *text, size_t text_size, size_t *length, char value)
{
  if(text == 0 || length == 0 || (*length + 1U) >= text_size) {
    return 0U;
  }

  text[*length] = value;
  *length += 1U;
  text[*length] = '\0';
  return 1U;
}

static uint8_t wifi_net_append_escaped(char *text,
                                       size_t text_size,
                                       size_t *length,
                                       const char *value)
{
  if(value == 0) {
    return 0U;
  }

  while(*value != '\0') {
    if(*value == '\r' || *value == '\n') {
      return 0U;
    }
    if(*value == '"' || *value == '\\') {
      if(wifi_net_append_char(text, text_size, length, '\\') == 0U) {
        return 0U;
      }
    }
    if(wifi_net_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }

  return 1U;
}

static uint8_t wifi_net_append_text(char *text,
                                    size_t text_size,
                                    size_t *length,
                                    const char *value)
{
  if(value == 0) {
    return 0U;
  }

  while(*value != '\0') {
    if(wifi_net_append_char(text, text_size, length, *value) == 0U) {
      return 0U;
    }
    value++;
  }
  return 1U;
}

static uint8_t wifi_net_build_join_command(char command[WIFI_NET_DIAG_JOIN_COMMAND_MAX])
{
  size_t length = 0U;

  command[0] = '\0';
  if(wifi_net_append_text(command,
                          WIFI_NET_DIAG_JOIN_COMMAND_MAX,
                          &length,
                          "AT+CWJAP=\"") == 0U ||
     wifi_net_append_escaped(command,
                             WIFI_NET_DIAG_JOIN_COMMAND_MAX,
                             &length,
                             WIFI_NET_DIAG_SSID) == 0U ||
     wifi_net_append_text(command,
                          WIFI_NET_DIAG_JOIN_COMMAND_MAX,
                          &length,
                          "\",\"") == 0U ||
     wifi_net_append_escaped(command,
                             WIFI_NET_DIAG_JOIN_COMMAND_MAX,
                             &length,
                             WIFI_NET_DIAG_PASSWORD) == 0U ||
     wifi_net_append_text(command,
                          WIFI_NET_DIAG_JOIN_COMMAND_MAX,
                          &length,
                          "\"") == 0U) {
    return 0U;
  }

  return 1U;
}

static uint32_t wifi_net_command_timeout_ms(wifi_net_command_t command)
{
  if(command == WIFI_NET_COMMAND_JOIN_AP) {
    return WIFI_NET_DIAG_JOIN_TIMEOUT_MS;
  }
  return WIFI_NET_DIAG_COMMAND_TIMEOUT_MS;
}

static void wifi_net_finish_fail(const char *result, const char *detail)
{
  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  wifi_net_pending_command = WIFI_NET_COMMAND_NONE;
  wifi_net_hold_ms = WIFI_NET_DIAG_FAIL_HOLD_MS;
  tester_wifi_print_at_end();
  wifi_net_draw_fail("NET FAIL", "ESP-AT", result, detail);
}

static void wifi_net_finish_no_ap_config(void)
{
  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  wifi_net_pending_command = WIFI_NET_COMMAND_NONE;
  wifi_net_hold_ms = WIFI_NET_DIAG_RESULT_HOLD_MS;
  tester_wifi_print_at_end();
  wifi_net_draw_wait("AP NOT SET",
                     "ESP-AT OK",
                     "NO AP",
                     "SET LOCAL SSID TO JOIN");
}

static void wifi_net_finish_pass(void)
{
  char result[WIFI_NET_DIAG_TEXT_MAX];
  char detail[WIFI_NET_DIAG_TEXT_MAX];

  (void)snprintf(result, sizeof(result), "IP %s", wifi_net_ip);
  if(wifi_net_ap[0] != '\0') {
    (void)snprintf(detail, sizeof(detail), "AP %.58s", wifi_net_ap);
  } else if(wifi_net_version[0] != '\0') {
    (void)snprintf(detail, sizeof(detail), "%s", wifi_net_version);
  } else {
    (void)snprintf(detail, sizeof(detail), "ESP-AT STA IP OK");
  }

  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  wifi_net_pending_command = WIFI_NET_COMMAND_NONE;
  wifi_net_hold_ms = WIFI_NET_DIAG_RESULT_HOLD_MS;
  tester_wifi_print_at_end();
  wifi_net_draw_pass("NET PASS", "ESP-AT + AP", result, detail);
}

static void wifi_net_issue_command(wifi_net_command_t command)
{
  const char *command_text = 0;
  const char *status = "";
  const char *main_text = "ESP-AT";
  const char *result = "WAIT";
  const char *detail = "";
  char join_command[WIFI_NET_DIAG_JOIN_COMMAND_MAX];

  switch(command) {
  case WIFI_NET_COMMAND_AT:
    command_text = "AT";
    status = "AT CHECK";
    detail = "PC3/PB9 115200 8N1";
    break;
  case WIFI_NET_COMMAND_GMR:
    command_text = "AT+GMR";
    status = "AT VERSION";
    detail = "READ ESP-AT VERSION";
    break;
  case WIFI_NET_COMMAND_MODE_QUERY:
    command_text = "AT+CWMODE?";
    status = "WIFI MODE";
    detail = "QUERY STA MODE";
    break;
  case WIFI_NET_COMMAND_AP_QUERY:
    command_text = "AT+CWJAP?";
    status = "AP STATUS";
    detail = "QUERY CURRENT AP";
    break;
  case WIFI_NET_COMMAND_SET_STA_MODE:
    command_text = "AT+CWMODE=1";
    status = "SET STA";
    detail = "CONFIGURE STATION MODE";
    break;
  case WIFI_NET_COMMAND_JOIN_AP:
    if(wifi_net_build_join_command(join_command) == 0U) {
      wifi_net_finish_fail("SSID INVALID", "CHECK LOCAL AP SETTING");
      return;
    }
    command_text = join_command;
    status = "JOIN AP";
    result = "CONNECTING";
    detail = "WAIT FOR WIFI GOT IP";
    break;
  case WIFI_NET_COMMAND_IP_QUERY:
    command_text = "AT+CIFSR";
    status = "IP CHECK";
    result = "READING";
    detail = "QUERY STATION IP";
    break;
  default:
    wifi_net_finish_fail("STATE ERROR", "INVALID DIAG COMMAND");
    return;
  }

  /* Draw before sending.  LCDM transfers can block briefly; drawing first
   * ensures no ESP response is arriving while the screen is refreshed. */
  wifi_net_draw_wait(status, main_text, result, detail);
  if(tester_wifi_print_at_send(command_text) == 0U) {
    wifi_net_finish_fail("TX NOT READY", "CHECK PC3 / ESP POWER");
    return;
  }

  wifi_net_waiting_command = command;
  wifi_net_wait_ms = 0U;
}

static void wifi_net_command_ok(void)
{
  wifi_net_command_t completed = wifi_net_waiting_command;

  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  switch(completed) {
  case WIFI_NET_COMMAND_AT:
    wifi_net_pending_command = WIFI_NET_COMMAND_GMR;
    break;
  case WIFI_NET_COMMAND_GMR:
    wifi_net_pending_command = WIFI_NET_COMMAND_MODE_QUERY;
    break;
  case WIFI_NET_COMMAND_MODE_QUERY:
    wifi_net_pending_command = WIFI_NET_COMMAND_AP_QUERY;
    break;
  case WIFI_NET_COMMAND_AP_QUERY:
    if(wifi_net_station_connected != 0U) {
      wifi_net_pending_command = WIFI_NET_COMMAND_IP_QUERY;
    } else if(wifi_net_has_join_config() != 0U) {
      wifi_net_pending_command = WIFI_NET_COMMAND_SET_STA_MODE;
    } else {
      wifi_net_finish_no_ap_config();
    }
    break;
  case WIFI_NET_COMMAND_SET_STA_MODE:
    wifi_net_pending_command = WIFI_NET_COMMAND_JOIN_AP;
    break;
  case WIFI_NET_COMMAND_JOIN_AP:
    if(wifi_net_got_ip != 0U) {
      wifi_net_station_connected = 1U;
    }
    wifi_net_pending_command = WIFI_NET_COMMAND_IP_QUERY;
    break;
  case WIFI_NET_COMMAND_IP_QUERY:
    if(wifi_net_ip[0] != '\0') {
      wifi_net_finish_pass();
    } else {
      wifi_net_finish_fail("IP EMPTY", "AP DID NOT PROVIDE STA IP");
    }
    break;
  default:
    wifi_net_finish_fail("STATE ERROR", "UNEXPECTED AT OK");
    break;
  }
}

static void wifi_net_command_error(void)
{
  wifi_net_command_t failed = wifi_net_waiting_command;

  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  if(failed == WIFI_NET_COMMAND_AP_QUERY) {
    if(wifi_net_has_join_config() != 0U) {
      wifi_net_pending_command = WIFI_NET_COMMAND_SET_STA_MODE;
    } else {
      wifi_net_finish_no_ap_config();
    }
    return;
  }

  if(failed == WIFI_NET_COMMAND_JOIN_AP) {
    wifi_net_finish_fail("JOIN ERROR", "CHECK AP SSID / PASSWORD");
  } else if(failed == WIFI_NET_COMMAND_IP_QUERY) {
    wifi_net_finish_fail("IP ERROR", "STA NOT CONNECTED");
  } else {
    wifi_net_finish_fail("AT ERROR", "CHECK ESP-AT FIRMWARE");
  }
}

static void wifi_net_process_line(const char *line)
{
  if(line == 0 || line[0] == '\0') {
    return;
  }

  if(strstr(line, "invalid header:") != 0) {
    wifi_net_finish_fail("ESP FLASH EMPTY", "REPROGRAM ESP-AT");
    return;
  }

  if(strstr(line, "AT version") != 0 || strstr(line, "ESP-AT") != 0) {
    (void)snprintf(wifi_net_version, sizeof(wifi_net_version), "%s", line);
  }
  if(strncmp(line, "+CWJAP:", 7U) == 0) {
    wifi_net_copy_quoted(line, wifi_net_ap, sizeof(wifi_net_ap));
    wifi_net_station_connected = 1U;
  }
  if(strncmp(line, "+CIFSR:STAIP", 12U) == 0) {
    wifi_net_copy_quoted(line, wifi_net_ip, sizeof(wifi_net_ip));
    wifi_net_got_ip = (wifi_net_ip[0] != '\0') ? 1U : 0U;
  }
  if(strstr(line, "WIFI GOT IP") != 0) {
    wifi_net_got_ip = 1U;
  }

  /* Echoes, asynchronous status lines, and a trailing blank response after a
   * completed command are informational once no command is outstanding. */
  if(wifi_net_waiting_command == WIFI_NET_COMMAND_NONE) {
    return;
  }

  if(strcmp(line, "OK") == 0) {
    wifi_net_command_ok();
  } else if(strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0) {
    wifi_net_command_error();
  }
}

static void wifi_net_start_cycle(void)
{
  tester_wifi_print_at_begin();
  wifi_net_waiting_command = WIFI_NET_COMMAND_NONE;
  wifi_net_pending_command = WIFI_NET_COMMAND_AT;
  wifi_net_wait_ms = 0U;
  wifi_net_hold_ms = 0U;
  wifi_net_station_connected = 0U;
  wifi_net_got_ip = 0U;
  memset(wifi_net_ap, 0, sizeof(wifi_net_ap));
  memset(wifi_net_ip, 0, sizeof(wifi_net_ip));
  memset(wifi_net_version, 0, sizeof(wifi_net_version));
  wifi_net_draw_wait("NET START", "ESP-AT", "WAIT", "115200 8N1");
}

void tester_wifi_net_diag_init(void)
{
  first_gen_display_init();
  tester_wifi_print_init();
  wifi_net_start_cycle();
}

void tester_wifi_net_diag_service(void)
{
  char line[TESTER_WIFI_AT_LINE_MAX];

  tester_wifi_print_service();
  while(tester_wifi_print_at_poll_line(line, sizeof(line)) != 0U) {
    wifi_net_process_line(line);
  }

  if(wifi_net_waiting_command != WIFI_NET_COMMAND_NONE) {
    wifi_net_wait_ms += WIFI_NET_DIAG_STEP_MS;
    if(wifi_net_wait_ms >= wifi_net_command_timeout_ms(wifi_net_waiting_command)) {
      wifi_net_finish_fail("TIMEOUT", "CHECK ESP POWER / AP");
    }
  }

  if(wifi_net_waiting_command == WIFI_NET_COMMAND_NONE &&
     wifi_net_pending_command != WIFI_NET_COMMAND_NONE) {
    wifi_net_command_t command = wifi_net_pending_command;
    wifi_net_pending_command = WIFI_NET_COMMAND_NONE;
    wifi_net_issue_command(command);
  } else if(wifi_net_waiting_command == WIFI_NET_COMMAND_NONE && wifi_net_hold_ms != 0U) {
    if(wifi_net_hold_ms <= WIFI_NET_DIAG_STEP_MS) {
      wifi_net_start_cycle();
    } else {
      wifi_net_hold_ms -= WIFI_NET_DIAG_STEP_MS;
    }
  }

  delay_ms(WIFI_NET_DIAG_STEP_MS);
}
