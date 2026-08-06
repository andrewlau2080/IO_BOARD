#include "print_host_wifi.h"

#include "print_terminal_store.h"
#include "tester_wifi_print.h"

#include <stdio.h>
#include <string.h>

#define HOST_WIFI_LINE_MAX          TESTER_WIFI_AT_LINE_MAX
#define HOST_WIFI_COMMAND_MAX       192U
#define HOST_WIFI_QUEUE_DEPTH       6U
#define HOST_WIFI_TX_QUEUE_DEPTH    8U
#define HOST_WIFI_SEEN_DEPTH        8U
#define HOST_WIFI_STATUS_MAX        40U
#define HOST_WIFI_IP_MAX            20U
#define HOST_WIFI_MAC_MAX           24U
#define HOST_WIFI_COMMAND_TIMEOUT   3000UL
#define HOST_WIFI_JOIN_TIMEOUT      25000UL
#define HOST_WIFI_IP_TIMEOUT        10000UL
#define HOST_WIFI_IP_RETRY_MS         300UL
#define HOST_WIFI_SESSION_TIMEOUT   60000UL
#define HOST_WIFI_BACKOFF_MS        1500UL
/* Post-+++ guard before the first AT of a session (same ESP-AT escape
 * convention as the tester engine: leading silence from the backoff wait,
 * trailing silence from this hold window). */
#define HOST_WIFI_ESP_KICK_HOLD_MS  1200UL
/* After power-up the ESP-AT module needs ~1.5-2 s to boot before it answers
 * AT.  Holding the first session for this grace makes the first kick+AT
 * succeed, so the print host comes ONLINE within a few seconds of power. */
#define HOST_WIFI_ESP_BOOT_GRACE_MS 2000UL
/* After the hold, the kick sends CRLF to terminate the pending "+++" line
 * (ESP-AT buffers it as an incomplete command in command mode; without CRLF
 * it merges with the AT into "+++AT" and the module answers ERROR).  This
 * second window lets the terminator's ERROR be consumed while the kick
 * window is still active. */
#define HOST_WIFI_ESP_KICK_TERM_MS  200UL
#define HOST_WIFI_TX_TIMEOUT        4500UL

typedef enum {
  HOST_CMD_NONE = 0,
  HOST_CMD_AT,
  HOST_CMD_STOP_SERVER,
  HOST_CMD_MODE,
  HOST_CMD_JOIN,
  HOST_CMD_IP,
  HOST_CMD_MUX,
  HOST_CMD_TRANS_MODE,
  HOST_CMD_SERVER
} host_command_t;

typedef struct {
  uint8_t valid;
  /* 0=queued, 1=done, 2=terminal print error, 3=printing. */
  uint8_t result_state;
  uint8_t link_id;
  uint32_t event_id;
  char device_uid[32];
} host_seen_t;

typedef struct {
  uint8_t link_id;
  uint8_t retries;
  uint16_t length;
  char payload[96];
} host_tx_item_t;

volatile uint32_t g_print_host_wifi_rx_request_count;
volatile uint32_t g_print_host_wifi_rx_duplicate_count;
volatile uint32_t g_print_host_wifi_rx_error_count;
volatile uint32_t g_print_host_wifi_ack_count;
volatile uint32_t g_print_host_wifi_done_count;
volatile uint32_t g_print_host_wifi_error_count;
volatile uint32_t g_print_host_wifi_reconnect_count;
volatile uint8_t g_print_host_wifi_state;

static print_host_wifi_state_t host_state;
static host_command_t host_waiting_command;
static uint32_t host_deadline_ms;
static uint32_t host_backoff_deadline_ms;
static uint32_t host_ip_retry_due_ms;
static uint32_t host_session_deadline_ms;
static uint8_t host_kick_pending;
static uint8_t host_kick_term_pending;
static uint32_t host_kick_deadline_ms;
static uint8_t host_first_start_pending;
static uint32_t host_first_start_deadline_ms;
static uint8_t host_ip_retry_pending;
static uint8_t host_got_ip;
static uint8_t host_tx_active;
static uint8_t host_tx_waiting_prompt;
static uint8_t host_tx_waiting_result;
static host_tx_item_t host_tx_current;
static host_tx_item_t host_tx_queue[HOST_WIFI_TX_QUEUE_DEPTH];
static uint8_t host_tx_head;
static uint8_t host_tx_tail;
static print_host_wifi_request_t host_request_queue[HOST_WIFI_QUEUE_DEPTH];
static uint8_t host_request_head;
static uint8_t host_request_tail;
static host_seen_t host_seen[HOST_WIFI_SEEN_DEPTH];
static uint8_t host_seen_next;
static char host_status[HOST_WIFI_STATUS_MAX];
static char host_ip[HOST_WIFI_IP_MAX];
static char host_mac[HOST_WIFI_MAC_MAX];

static uint8_t host_time_reached(uint32_t deadline)
{
  return ((int32_t)(tester_wifi_print_now_ms() - deadline) >= 0) ? 1U : 0U;
}

static void host_set_state(print_host_wifi_state_t state, const char *text)
{
  host_state = state;
  g_print_host_wifi_state = (uint8_t)state;
  if(text == 0) {
    text = "WAITING";
  }
  (void)snprintf(host_status, sizeof(host_status), "%s", text);
}

static void host_reset_tx_queue(void)
{
  host_tx_head = 0U;
  host_tx_tail = 0U;
  host_tx_active = 0U;
  host_tx_waiting_prompt = 0U;
  host_tx_waiting_result = 0U;
  memset(&host_tx_current, 0, sizeof(host_tx_current));
}

static void host_reset_request_queue(void)
{
  host_request_head = 0U;
  host_request_tail = 0U;
}

static void host_reset_seen(void)
{
  memset(host_seen, 0, sizeof(host_seen));
  host_seen_next = 0U;
}

static uint8_t host_queue_tx(uint8_t link_id, const char *payload)
{
  uint8_t next;
  size_t length;

  if(payload == 0) {
    return 0U;
  }
  length = strlen(payload);
  if(length == 0U || length >= sizeof(host_tx_queue[0].payload)) {
    return 0U;
  }
  next = (uint8_t)((host_tx_head + 1U) % HOST_WIFI_TX_QUEUE_DEPTH);
  if(next == host_tx_tail) {
    g_print_host_wifi_error_count++;
    return 0U;
  }
  host_tx_queue[host_tx_head].link_id = link_id;
  host_tx_queue[host_tx_head].retries = 0U;
  host_tx_queue[host_tx_head].length = (uint16_t)length;
  memcpy(host_tx_queue[host_tx_head].payload, payload, length + 1U);
  host_tx_head = next;
  return 1U;
}

static uint8_t host_parse_uint(const char *text, uint32_t *value)
{
  uint32_t result = 0U;
  uint8_t digits = 0U;

  if(text == 0 || value == 0) {
    return 0U;
  }
  while(*text == ' ' || *text == '\t' || *text == '"') {
    text++;
  }
  while(*text >= '0' && *text <= '9') {
    uint32_t digit = (uint32_t)(*text - '0');
    if(result > ((0xFFFFFFFFUL - digit) / 10UL)) {
      return 0U;
    }
    result = result * 10UL + digit;
    text++;
    digits++;
  }
  if(digits == 0U) {
    return 0U;
  }
  *value = result;
  return 1U;
}

static uint8_t host_json_uint(const char *json, const char *key, uint32_t *value)
{
  char needle[32];
  const char *at;

  if(json == 0 || key == 0 || value == 0) {
    return 0U;
  }
  (void)snprintf(needle, sizeof(needle), "\"%s\":", key);
  at = strstr(json, needle);
  if(at == 0) {
    return 0U;
  }
  return host_parse_uint(at + strlen(needle), value);
}

static uint8_t host_json_string(const char *json,
                                const char *key,
                                char *out,
                                uint16_t out_size)
{
  char needle[32];
  const char *at;
  const char *cursor;
  uint16_t length = 0U;

  if(json == 0 || key == 0 || out == 0 || out_size == 0U) {
    return 0U;
  }
  out[0] = '\0';
  (void)snprintf(needle, sizeof(needle), "\"%s\":", key);
  at = strstr(json, needle);
  if(at == 0) {
    return 0U;
  }
  cursor = at + strlen(needle);
  while(*cursor == ' ' || *cursor == '\t') {
    cursor++;
  }
  if(*cursor != '"') {
    return 0U;
  }
  cursor++;
  while(*cursor != '\0' && *cursor != '"') {
    char value = *cursor++;
    if(value == '\\' && *cursor != '\0') {
      value = *cursor++;
    }
    if((uint16_t)(length + 1U) >= out_size) {
      return 0U;
    }
    if((unsigned char)value < 0x20U) {
      return 0U;
    }
    out[length++] = value;
  }
  if(*cursor != '"') {
    return 0U;
  }
  out[length] = '\0';
  return 1U;
}

static uint8_t host_json_bool_or_result(const char *json, print_host_wifi_request_t *request)
{
  char result[12];
  uint32_t value;

  if(json == 0 || request == 0) {
    return 0U;
  }
  if(host_json_string(json, "result", result, sizeof(result)) != 0U) {
    request->pass = (strcmp(result, "PASS") == 0 || strcmp(result, "pass") == 0) ? 1U : 0U;
    return 1U;
  }
  if(host_json_uint(json, "pass", &value) != 0U) {
    request->pass = (value != 0U) ? 1U : 0U;
    return 1U;
  }
  request->pass = 1U;
  return 0U;
}

static uint8_t host_parse_ipd(const char *line,
                              uint8_t *link_id,
                              const char **payload,
                              uint16_t *payload_length)
{
  const char *cursor;
  const char *colon;
  uint32_t link;
  uint32_t length;

  if(line == 0 || link_id == 0 || payload == 0 || payload_length == 0 ||
     strncmp(line, "+IPD,", 5U) != 0) {
    return 0U;
  }
  cursor = line + 5U;
  if(host_parse_uint(cursor, &link) == 0U || link > 4U) {
    return 0U;
  }
  cursor = strchr(cursor, ',');
  if(cursor == 0 || host_parse_uint(cursor + 1U, &length) == 0U ||
     length > (uint32_t)(HOST_WIFI_LINE_MAX - 12U)) {
    return 0U;
  }
  colon = strchr(cursor + 1U, ':');
  /* The software-UART line collector uses LF as its frame delimiter and
   * therefore removes the final LF from a normal JSON request.  ESP-AT's
   * declared +IPD length still includes that LF, so accept the actual
   * printable payload when it is one (or CR/LF two) bytes shorter. */
  if(colon == 0 || strlen(colon + 1U) > length ||
     (uint32_t)strlen(colon + 1U) + 2U < length) {
    return 0U;
  }
  *link_id = (uint8_t)link;
  *payload = colon + 1U;
  *payload_length = (uint16_t)strlen(colon + 1U);
  return 1U;
}

static uint8_t host_seen_find(uint32_t event_id, const char *uid, uint8_t *result_state)
{
  uint8_t index;

  for(index = 0U; index < HOST_WIFI_SEEN_DEPTH; index++) {
    if(host_seen[index].valid != 0U && host_seen[index].event_id == event_id &&
       strcmp(host_seen[index].device_uid, uid == 0 ? "" : uid) == 0) {
      if(result_state != 0) {
        *result_state = host_seen[index].result_state;
      }
      return 1U;
    }
  }
  return 0U;
}

static void host_seen_add(uint32_t event_id, const char *uid, uint8_t link_id)
{
  host_seen_t *entry = &host_seen[host_seen_next];

  memset(entry, 0, sizeof(*entry));
  entry->valid = 1U;
  entry->event_id = event_id;
  entry->link_id = link_id;
  (void)snprintf(entry->device_uid, sizeof(entry->device_uid), "%s", uid == 0 ? "" : uid);
  host_seen_next = (uint8_t)((host_seen_next + 1U) % HOST_WIFI_SEEN_DEPTH);
}

static host_seen_t *host_seen_get(uint32_t event_id, const char *uid)
{
  uint8_t index;

  for(index = 0U; index < HOST_WIFI_SEEN_DEPTH; index++) {
    if(host_seen[index].valid != 0U && host_seen[index].event_id == event_id &&
       strcmp(host_seen[index].device_uid, uid == 0 ? "" : uid) == 0) {
      return &host_seen[index];
    }
  }
  return 0;
}

static uint8_t host_parse_request(const char *payload,
                                  uint16_t payload_length,
                                  uint8_t link_id,
                                  print_host_wifi_request_t *request)
{
  char frame[HOST_WIFI_LINE_MAX];
  uint32_t value;
  const char *comma;

  if(payload == 0 || request == 0 || payload_length == 0U ||
     payload_length >= sizeof(frame)) {
    return 0U;
  }
  memset(request, 0, sizeof(*request));
  memcpy(frame, payload, payload_length);
  frame[payload_length] = '\0';
  request->link_id = link_id;

  if(strncmp(frame, "PRINT,", 6U) == 0) {
    /* Compact maintenance-tool form: PRINT,event,station,qty,result. */
    comma = strchr(frame + 6U, ',');
    if(comma == 0 || host_parse_uint(frame + 6U, &request->event_id) == 0U) {
      return 0U;
    }
    if(host_parse_uint(comma + 1U, &value) != 0U) {
      request->station = (uint8_t)value;
    }
    comma = strchr(comma + 1U, ',');
    if(comma != 0 && host_parse_uint(comma + 1U, &value) != 0U) {
      request->quantity = (uint16_t)value;
    }
    request->pass = (strstr(frame, "NG") == 0) ? 1U : 0U;
  } else {
    if(host_json_uint(frame, "event_id", &request->event_id) == 0U) {
      return 0U;
    }
    if(host_json_uint(frame, "station", &value) != 0U) {
      request->station = (uint8_t)value;
    } else if(host_json_uint(frame, "station_id", &value) != 0U) {
      request->station = (uint8_t)value;
    }
    if(host_json_uint(frame, "quantity", &value) != 0U ||
       host_json_uint(frame, "test_count", &value) != 0U) {
      request->quantity = (uint16_t)value;
    }
    (void)host_json_string(frame, "device_uid", request->device_uid,
                           sizeof(request->device_uid));
    (void)host_json_string(frame, "title", request->title, sizeof(request->title));
    (void)host_json_string(frame, "item", request->item, sizeof(request->item));
    (void)host_json_string(frame, "content", request->content, sizeof(request->content));
    (void)host_json_string(frame, "code", request->code, sizeof(request->code));
    (void)host_json_bool_or_result(frame, request);
  }

  if(request->station == 0U || request->station > 10U) {
    request->station = 1U;
  }
  if(request->quantity == 0U) {
    request->quantity = 1U;
  }
  return (request->event_id != 0U) ? 1U : 0U;
}

static void host_receive_request(const char *line)
{
  const char *payload;
  uint16_t payload_length;
  uint8_t link_id;
  print_host_wifi_request_t request;
  uint8_t duplicate_state = 0U;
  uint8_t next;
  char response[64];

  if(host_parse_ipd(line, &link_id, &payload, &payload_length) == 0U) {
    return;
  }
  if(host_parse_request(payload, payload_length, link_id, &request) == 0U) {
    g_print_host_wifi_rx_error_count++;
    return;
  }

  if(host_seen_find(request.event_id, request.device_uid, &duplicate_state) != 0U) {
    g_print_host_wifi_rx_duplicate_count++;
    (void)snprintf(response, sizeof(response), "ACK,%lu,QUEUED\n",
                   (unsigned long)request.event_id);
    (void)host_queue_tx(request.link_id, response);
    if(duplicate_state == 1U) {
      (void)snprintf(response, sizeof(response), "DONE,%lu\n",
                     (unsigned long)request.event_id);
      (void)host_queue_tx(request.link_id, response);
    } else if(duplicate_state == 2U) {
      (void)snprintf(response, sizeof(response), "ERROR,%lu\n",
                     (unsigned long)request.event_id);
      (void)host_queue_tx(request.link_id, response);
    } else if(duplicate_state == 3U) {
      (void)snprintf(response, sizeof(response), "PRINTING,%lu\n",
                     (unsigned long)request.event_id);
      (void)host_queue_tx(request.link_id, response);
    }
    return;
  }

  next = (uint8_t)((host_request_head + 1U) % HOST_WIFI_QUEUE_DEPTH);
  if(next == host_request_tail) {
    g_print_host_wifi_rx_error_count++;
    (void)snprintf(response, sizeof(response), "ERROR,%lu\n",
                   (unsigned long)request.event_id);
    (void)host_queue_tx(request.link_id, response);
    return;
  }
  host_request_queue[host_request_head] = request;
  host_request_head = next;
  host_seen_add(request.event_id, request.device_uid, request.link_id);
  g_print_host_wifi_rx_request_count++;
  (void)snprintf(response, sizeof(response), "ACK,%lu,QUEUED\n",
                 (unsigned long)request.event_id);
  if(host_queue_tx(request.link_id, response) != 0U) {
    g_print_host_wifi_ack_count++;
  }
}

static uint8_t host_command_append(char *text, uint16_t capacity,
                                   uint16_t *length, char value)
{
  if(text == 0 || length == 0 || (uint16_t)(*length + 1U) >= capacity) {
    return 0U;
  }
  text[*length] = value;
  *length = (uint16_t)(*length + 1U);
  text[*length] = '\0';
  return 1U;
}

static uint8_t host_command_append_text(char *text, uint16_t capacity,
                                        uint16_t *length, const char *value)
{
  if(value == 0) return 0U;
  while(*value != '\0') {
    if(host_command_append(text, capacity, length, *value++) == 0U) return 0U;
  }
  return 1U;
}

static uint8_t host_command_append_escaped(char *text, uint16_t capacity,
                                           uint16_t *length, const char *value)
{
  if(value == 0) return 0U;
  while(*value != '\0') {
    if(*value == '\r' || *value == '\n') return 0U;
    if(*value == '"' || *value == '\\') {
      if(host_command_append(text, capacity, length, '\\') == 0U) return 0U;
    }
    if(host_command_append(text, capacity, length, *value++) == 0U) return 0U;
  }
  return 1U;
}

static uint8_t host_build_join(char *command, uint16_t capacity)
{
  const print_terminal_store_config_t *config = print_terminal_store_get();
  uint16_t length = 0U;

  if(config == 0 || config->wifi_ssid[0] == '\0' || capacity == 0U) {
    return 0U;
  }
  command[0] = '\0';
  return host_command_append_text(command, capacity, &length, "AT+CWJAP=\"") != 0U &&
         host_command_append_escaped(command, capacity, &length, config->wifi_ssid) != 0U &&
         host_command_append_text(command, capacity, &length, "\",\"") != 0U &&
         host_command_append_escaped(command, capacity, &length, config->wifi_password) != 0U &&
         host_command_append(command, capacity, &length, '"') != 0U;
}

static void host_schedule_retry(void);
static void host_schedule_ip_retry(void);

static uint8_t host_issue_command(host_command_t command)
{
  char text[HOST_WIFI_COMMAND_MAX];
  const print_terminal_store_config_t *config = print_terminal_store_get();
  uint32_t timeout = HOST_WIFI_COMMAND_TIMEOUT;
  uint32_t now_ms;

  if(config == 0) {
    return 0U;
  }
  text[0] = '\0';
  switch(command) {
  case HOST_CMD_AT: (void)snprintf(text, sizeof(text), "AT"); break;
  case HOST_CMD_STOP_SERVER: (void)snprintf(text, sizeof(text), "AT+CIPSERVER=0"); break;
  case HOST_CMD_MODE: (void)snprintf(text, sizeof(text), "AT+CWMODE=1"); break;
  case HOST_CMD_JOIN:
    if(host_build_join(text, sizeof(text)) == 0U) return 0U;
    timeout = HOST_WIFI_JOIN_TIMEOUT;
    break;
  case HOST_CMD_IP:
    (void)snprintf(text, sizeof(text), "AT+CIFSR");
    timeout = HOST_WIFI_IP_TIMEOUT;
    break;
  case HOST_CMD_MUX: (void)snprintf(text, sizeof(text), "AT+CIPMUX=1"); break;
  case HOST_CMD_TRANS_MODE: (void)snprintf(text, sizeof(text), "AT+CIPMODE=0"); break;
  case HOST_CMD_SERVER:
    (void)snprintf(text, sizeof(text), "AT+CIPSERVER=1,%u",
                   (unsigned int)config->wifi_listen_port);
    break;
  default: return 0U;
  }
  if(tester_wifi_print_at_send(text) == 0U) {
    return 0U;
  }
  now_ms = tester_wifi_print_now_ms();
  host_waiting_command = command;
  host_deadline_ms = now_ms + timeout;
  if(command == HOST_CMD_JOIN) {
    host_session_deadline_ms = now_ms + HOST_WIFI_SESSION_TIMEOUT;
    host_ip_retry_pending = 0U;
  } else if(command == HOST_CMD_IP) {
    host_ip_retry_pending = 0U;
  }
  if(command == HOST_CMD_JOIN) {
    host_set_state(PRINT_HOST_WIFI_JOINING, "JOINING WIFI");
  } else if(command == HOST_CMD_IP) {
    host_set_state(PRINT_HOST_WIFI_JOINING, "WAIT DHCP / READ IP");
  } else if(command == HOST_CMD_SERVER) {
    host_set_state(PRINT_HOST_WIFI_STARTING_SERVER, "STARTING SERVER");
  } else {
    host_set_state(PRINT_HOST_WIFI_STARTING, "WIFI STARTING");
  }
  return 1U;
}

static void host_schedule_retry(void)
{
  host_waiting_command = HOST_CMD_NONE;
  host_ip_retry_pending = 0U;
  host_session_deadline_ms = 0U;
  host_kick_pending = 0U;
  tester_wifi_print_at_end();
  host_state = PRINT_HOST_WIFI_ERROR;
  g_print_host_wifi_state = (uint8_t)host_state;
  /* Keep the operator-facing state explicit.  The backoff/reconnect counter
   * remains internal; the LCDM should say NETWORK ERROR when the controller
   * cannot maintain its AP/server session. */
  (void)snprintf(host_status, sizeof(host_status), "NETWORK ERROR");
  host_backoff_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BACKOFF_MS;
  g_print_host_wifi_reconnect_count++;
}

/* CWJAP may return OK before DHCP has populated CIFSR.  Keep the same bounded
 * polling behaviour as the validated tester-side settings flow instead of
 * tearing down a good AP association on the first 0.0.0.0 response. */
static void host_schedule_ip_retry(void)
{
  uint32_t now_ms = tester_wifi_print_now_ms();

  host_waiting_command = HOST_CMD_NONE;
  host_deadline_ms = 0U;
  if(host_session_deadline_ms == 0U ||
     (int32_t)(now_ms - host_session_deadline_ms) >= 0) {
    host_schedule_retry();
    return;
  }
  host_ip_retry_pending = 1U;
  host_ip_retry_due_ms = now_ms + HOST_WIFI_IP_RETRY_MS;
  host_set_state(PRINT_HOST_WIFI_JOINING, "WAIT DHCP / RETRY IP");
}

static uint8_t host_issue_or_retry(host_command_t command)
{
  if(host_issue_command(command) != 0U) {
    return 1U;
  }
  host_schedule_retry();
  return 0U;
}

static uint8_t host_copy_quoted(const char *line, char *out, uint16_t out_size)
{
  const char *start;
  const char *end;
  size_t length;

  if(line == 0 || out == 0 || out_size == 0U) {
    return 0U;
  }
  start = strchr(line, '"');
  if(start == 0) {
    return 0U;
  }
  end = strchr(start + 1U, '"');
  if(end == 0) {
    return 0U;
  }
  length = (size_t)(end - start - 1U);
  if(length >= out_size) {
    length = (size_t)(out_size - 1U);
  }
  memcpy(out, start + 1U, length);
  out[length] = '\0';
  return 1U;
}

static void host_process_ok(void)
{
  host_command_t completed = host_waiting_command;

  host_waiting_command = HOST_CMD_NONE;
  switch(completed) {
  case HOST_CMD_AT:
    (void)host_issue_or_retry(HOST_CMD_STOP_SERVER);
    break;
  case HOST_CMD_STOP_SERVER:
    (void)host_issue_or_retry(HOST_CMD_MODE);
    break;
  case HOST_CMD_MODE:
    (void)host_issue_or_retry(HOST_CMD_JOIN);
    break;
  case HOST_CMD_JOIN:
    (void)host_issue_or_retry(HOST_CMD_IP);
    break;
  case HOST_CMD_IP:
    if(host_got_ip == 0U) {
      host_schedule_ip_retry();
    } else {
      host_session_deadline_ms = 0U;
      (void)host_issue_or_retry(HOST_CMD_MUX);
    }
    break;
  case HOST_CMD_MUX:
    (void)host_issue_or_retry(HOST_CMD_TRANS_MODE);
    break;
  case HOST_CMD_TRANS_MODE:
    (void)host_issue_or_retry(HOST_CMD_SERVER);
    break;
  case HOST_CMD_SERVER:
    host_set_state(PRINT_HOST_WIFI_ONLINE, "WIFI SERVER ONLINE");
    break;
  default:
    break;
  }
}

static uint8_t host_tx_start_current(void);

static void host_process_line(const char *line)
{
  if(line == 0 || line[0] == '\0') {
    return;
  }
  if(strncmp(line, "+IPD,", 5U) == 0) {
    host_receive_request(line);
    return;
  }
  if(strstr(line, "+CIFSR:STAIP") != 0 ||
     strstr(line, "+CIPSTA:ip") != 0) {
    if(host_copy_quoted(line, host_ip, sizeof(host_ip)) != 0U) {
      host_got_ip = (host_ip[0] != '\0' && strcmp(host_ip, "0.0.0.0") != 0) ? 1U : 0U;
    }
    return;
  }
  if(strstr(line, "+CIFSR:STAMAC") != 0 ||
     strstr(line, "+CIPSTA:mac") != 0 ||
     strstr(line, "+CIPSTAMAC") != 0) {
    if(host_copy_quoted(line, host_mac, sizeof(host_mac)) != 0U) {
      /* The module MAC is not operator editable.  Persist it only when
       * it changes, so normal reconnects never consume Flash endurance. */
      (void)print_terminal_store_update_wifi_mac(host_mac);
    }
    return;
  }
  if(host_tx_active != 0U && host_tx_waiting_prompt != 0U &&
     line[0] == '>') {
    if(tester_wifi_print_at_send_bytes((const uint8_t *)host_tx_current.payload,
                                       host_tx_current.length) == 0U) {
      host_tx_active = 0U;
      host_tx_waiting_prompt = 0U;
      host_schedule_retry();
      return;
    }
    host_tx_waiting_prompt = 0U;
    host_tx_waiting_result = 1U;
    host_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_TX_TIMEOUT;
    return;
  }
  if(host_tx_active != 0U && host_tx_waiting_result != 0U &&
     strcmp(line, "SEND OK") == 0) {
    host_tx_active = 0U;
    host_tx_waiting_result = 0U;
    return;
  }
  if(host_tx_active != 0U &&
     (strcmp(line, "SEND FAIL") == 0 || strcmp(line, "ERROR") == 0)) {
    host_tx_waiting_prompt = 0U;
    host_tx_waiting_result = 0U;
    if(host_tx_current.retries < 2U) {
      host_tx_current.retries++;
      host_tx_active = 0U;
      if(host_tx_start_current() == 0U) {
        host_schedule_retry();
      }
    } else {
      host_tx_active = 0U;
      g_print_host_wifi_error_count++;
    }
    return;
  }
  if(strcmp(line, "WIFI CONNECTED") == 0) {
    /* This is progress during CWJAP, not a completed DHCP/server session. */
    return;
  }
  if(strcmp(line, "WIFI GOT IP") == 0) {
    host_got_ip = 1U;
    return;
  }
  if(strncmp(line, "+CWJAP:", 7U) == 0 &&
     host_waiting_command == HOST_CMD_JOIN && strchr(line, '"') == 0) {
    /* Numeric +CWJAP:<status> is a terminal association error. */
    host_schedule_retry();
    return;
  }
  if(host_waiting_command != HOST_CMD_NONE && strcmp(line, "OK") == 0) {
    host_process_ok();
    return;
  }
  if(host_waiting_command == HOST_CMD_STOP_SERVER &&
     (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0)) {
    /* No prior server is the normal fresh-boot case. */
    host_waiting_command = HOST_CMD_NONE;
    (void)host_issue_or_retry(HOST_CMD_MODE);
    return;
  }
  if(host_waiting_command != HOST_CMD_NONE &&
     (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0 ||
      strstr(line, "busy p...") != 0 || strstr(line, "link is not valid") != 0)) {
    if(host_waiting_command == HOST_CMD_IP) {
      host_schedule_ip_retry();
    } else {
      host_schedule_retry();
    }
    return;
  }
  if(strcmp(line, "WIFI DISCONNECT") == 0 ||
     strcmp(line, "WIFI DISCONNECTED") == 0) {
    /* A stale association is commonly reported while CWJAP replaces it.
     * Match the validated tester-side flow: let JOIN/DHCP complete and only
     * reconnect when a disconnect occurs after the IP/server phase. */
    if(host_waiting_command == HOST_CMD_STOP_SERVER ||
       host_waiting_command == HOST_CMD_MODE ||
       host_waiting_command == HOST_CMD_JOIN ||
       host_waiting_command == HOST_CMD_IP ||
       host_state == PRINT_HOST_WIFI_JOINING) {
      return;
    }
    host_schedule_retry();
    return;
  }
  /* In server mode CLOSED means a client socket ended.  The listening socket
   * remains valid, so it must not tear down the service or lose a following
   * station request. */
}

static uint8_t host_tx_start_current(void)
{
  char command[32];

  (void)snprintf(command, sizeof(command), "AT+CIPSEND=%u,%u",
                 (unsigned int)host_tx_current.link_id,
                 (unsigned int)host_tx_current.length);
  if(tester_wifi_print_at_send(command) == 0U) {
    return 0U;
  }
  host_tx_active = 1U;
  host_tx_waiting_prompt = 1U;
  host_tx_waiting_result = 0U;
  host_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_TX_TIMEOUT;
  return 1U;
}

static void host_tx_service(void)
{
  if(host_state != PRINT_HOST_WIFI_ONLINE) {
    return;
  }
  if(host_tx_active != 0U) {
    if(host_time_reached(host_deadline_ms) != 0U) {
      if(host_tx_current.retries < 2U) {
        host_tx_current.retries++;
        host_tx_active = 0U;
        host_tx_waiting_prompt = 0U;
        host_tx_waiting_result = 0U;
        if(host_tx_start_current() == 0U) {
          host_schedule_retry();
        }
        return;
      } else {
        host_tx_active = 0U;
        host_tx_waiting_prompt = 0U;
        host_tx_waiting_result = 0U;
        g_print_host_wifi_error_count++;
      }
    } else {
      return;
    }
  }
  if(host_tx_tail == host_tx_head) {
    return;
  }
  host_tx_current = host_tx_queue[host_tx_tail];
  host_tx_tail = (uint8_t)((host_tx_tail + 1U) % HOST_WIFI_TX_QUEUE_DEPTH);
  if(host_tx_start_current() == 0U) {
    g_print_host_wifi_error_count++;
    host_schedule_retry();
  }
}

void print_host_wifi_init(void)
{
  host_state = PRINT_HOST_WIFI_STOPPED;
  host_waiting_command = HOST_CMD_NONE;
  host_deadline_ms = 0U;
  host_backoff_deadline_ms = 0U;
  host_ip_retry_due_ms = 0U;
  host_session_deadline_ms = 0U;
  host_kick_pending = 0U;
  host_kick_deadline_ms = 0U;
  host_first_start_pending = 1U;
  host_first_start_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_ESP_BOOT_GRACE_MS;
  host_ip_retry_pending = 0U;
  host_got_ip = 0U;
  host_reset_tx_queue();
  host_reset_request_queue();
  host_reset_seen();
  host_ip[0] = '\0';
  host_mac[0] = '\0';
  (void)snprintf(host_status, sizeof(host_status), "WAITING");
  g_print_host_wifi_rx_request_count = 0U;
  g_print_host_wifi_rx_duplicate_count = 0U;
  g_print_host_wifi_rx_error_count = 0U;
  g_print_host_wifi_ack_count = 0U;
  g_print_host_wifi_done_count = 0U;
  g_print_host_wifi_error_count = 0U;
  g_print_host_wifi_reconnect_count = 0U;
  g_print_host_wifi_state = (uint8_t)host_state;
}

void print_host_wifi_start(void)
{
  const print_terminal_store_config_t *config = print_terminal_store_get();

  if(config == 0 || config->wifi_ssid[0] == '\0' ||
     config->wifi_listen_port == 0U) {
    tester_wifi_print_at_end();
    host_waiting_command = HOST_CMD_NONE;
    host_set_state(PRINT_HOST_WIFI_CONFIG_REQUIRED, "SET WIFI SSID/PORT");
    return;
  }
  tester_wifi_print_at_begin();
  /* Keep recent completed IDs across a WiFi reconnect.  If a tester missed
   * DONE and retransmits after the AP returns, it receives the cached result
   * instead of a second physical label. */
  host_reset_tx_queue();
  host_got_ip = 0U;
  host_ip[0] = '\0';
  host_mac[0] = '\0';
  host_ip_retry_due_ms = 0U;
  host_session_deadline_ms = 0U;
  host_ip_retry_pending = 0U;
  host_set_state(PRINT_HOST_WIFI_STARTING, "WIFI STARTING");
  /* Kick a possibly stuck ESP before the first AT (same +++ escape as the
   * tester engine).  The backoff wait supplies the leading guard silence,
   * HOST_WIFI_ESP_KICK_HOLD_MS the trailing guard.  host_waiting_command is
   * NONE during the window, so the +++ ERROR cannot disturb the sequence;
   * this is the automatic reconnect path for a wedged or disconnected
   * module, requiring no operator or power-cycle intervention. */
  host_waiting_command = HOST_CMD_NONE;
  host_kick_pending = 1U;
  host_kick_term_pending = 0U;
  host_kick_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_ESP_KICK_HOLD_MS;
  if(tester_wifi_print_at_send_bytes((const uint8_t *)"+++", 3U) == 0U) {
    host_kick_pending = 0U;
    host_schedule_retry();
  }
}

void print_host_wifi_restart(void)
{
  print_host_wifi_start();
}

void print_host_wifi_service(void)
{
  char line[HOST_WIFI_LINE_MAX];
  uint32_t now_ms;

  /* Keep the shared decoder and time base running even while the first
   * session is held by the boot grace. */
  tester_wifi_print_service();

  if(host_first_start_pending != 0U) {
    if(host_time_reached(host_first_start_deadline_ms) == 0U) {
      return;  /* ESP-AT still booting after power-up */
    }
    host_first_start_pending = 0U;
    print_host_wifi_start();
    return;
  }

  while(tester_wifi_print_at_poll_line(line, sizeof(line)) != 0U) {
    host_process_line(line);
  }
  if(host_kick_pending != 0U && host_time_reached(host_kick_deadline_ms) != 0U) {
    if(host_kick_term_pending == 0U) {
      /* Phase 1: terminate the pending "+++" line (see the define above).
       * The terminator's ERROR is consumed below while the kick window is
       * still open, so it cannot be mistaken for an AT failure. */
      host_kick_term_pending = 1U;
      (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
      host_kick_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_ESP_KICK_TERM_MS;
    } else {
      host_kick_pending = 0U;
      host_kick_term_pending = 0U;
      if(host_issue_command(HOST_CMD_AT) == 0U) {
        host_schedule_retry();
      }
    }
    return;
  }
  if(host_state == PRINT_HOST_WIFI_ERROR && host_time_reached(host_backoff_deadline_ms) != 0U) {
    print_host_wifi_start();
    return;
  }
  now_ms = tester_wifi_print_now_ms();
  if(host_ip_retry_pending != 0U &&
     (int32_t)(now_ms - host_ip_retry_due_ms) >= 0) {
    host_ip_retry_pending = 0U;
    if(host_session_deadline_ms != 0U &&
       (int32_t)(now_ms - host_session_deadline_ms) >= 0) {
      host_schedule_retry();
    } else if(host_issue_command(HOST_CMD_IP) == 0U) {
      host_schedule_retry();
    }
    return;
  }
  if(host_waiting_command != HOST_CMD_NONE && host_time_reached(host_deadline_ms) != 0U) {
    if(host_waiting_command == HOST_CMD_IP) {
      host_schedule_ip_retry();
    } else {
      host_schedule_retry();
    }
  }
  host_tx_service();
}

uint8_t print_host_wifi_is_online(void)
{
  return (host_state == PRINT_HOST_WIFI_ONLINE) ? 1U : 0U;
}

uint8_t print_host_wifi_is_error(void)
{
  return (host_state == PRINT_HOST_WIFI_ERROR) ? 1U : 0U;
}

const char *print_host_wifi_status_text(void)
{
  return host_status;
}

const char *print_host_wifi_ip_text(void)
{
  return host_ip;
}

const char *print_host_wifi_mac_text(void)
{
  return host_mac;
}

uint8_t print_host_wifi_poll_request(print_host_wifi_request_t *out_request)
{
  if(out_request == 0 || host_request_tail == host_request_head) {
    return 0U;
  }
  *out_request = host_request_queue[host_request_tail];
  host_request_tail = (uint8_t)((host_request_tail + 1U) % HOST_WIFI_QUEUE_DEPTH);
  return 1U;
}

static uint8_t host_send_result(const print_host_wifi_request_t *request,
                                const char *prefix)
{
  char response[64];
  host_seen_t *seen;
  uint8_t queued;

  if(request == 0 || prefix == 0) {
    return 0U;
  }
  (void)snprintf(response, sizeof(response), "%s,%lu\n", prefix,
                 (unsigned long)request->event_id);
  queued = host_queue_tx(request->link_id, response);
  seen = host_seen_get(request->event_id, request->device_uid);
  if(seen != 0) {
    if(strcmp(prefix, "DONE") == 0) {
      seen->result_state = 1U;
    } else if(strcmp(prefix, "PRINTING") == 0) {
      seen->result_state = 3U;
    } else {
      seen->result_state = 2U;
    }
  }
  if(strcmp(prefix, "DONE") == 0) {
    g_print_host_wifi_done_count++;
  } else if(strcmp(prefix, "ERROR") == 0) {
    g_print_host_wifi_error_count++;
  }
  return queued;
}

uint8_t print_host_wifi_send_printing(const print_host_wifi_request_t *request)
{
  return host_send_result(request, "PRINTING");
}

uint8_t print_host_wifi_send_done(const print_host_wifi_request_t *request)
{
  return host_send_result(request, "DONE");
}

uint8_t print_host_wifi_send_error(const print_host_wifi_request_t *request)
{
  return host_send_result(request, "ERROR");
}
