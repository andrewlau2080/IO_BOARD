#include "tester_host_wifi.h"
#include "device_config.h"
#include "tester_settings.h"

#include "tester_wifi_print.h"

#include <stdio.h>
#include <stdlib.h>
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
/* UDP 信标广播：ONLINE 后每 3s 广播自身 line_id/IP/端口（尽力而为，链路忙则
 * 下周期再试），测试机据此按 PD LINE 自动发现本控制器，无需手动填 HOST/PORT。 */
#define HOST_WIFI_BEACON_PORT       5002U
#define HOST_WIFI_BEACON_PERIOD_MS  3000UL
#define HOST_WIFI_BEACON_LINK       4U
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
  HOST_CMD_SERVER,
  HOST_CMD_BEACON_START,
  HOST_CMD_BEACON_CLOSE,
  HOST_CMD_BEACON_TX
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

volatile uint32_t g_tester_host_wifi_rx_request_count;
volatile uint32_t g_tester_host_wifi_rx_duplicate_count;
volatile uint32_t g_tester_host_wifi_rx_error_count;
volatile uint32_t g_tester_host_wifi_ack_count;
volatile uint32_t g_tester_host_wifi_done_count;
volatile uint32_t g_tester_host_wifi_error_count;
volatile uint32_t g_tester_host_wifi_reconnect_count;
volatile uint8_t g_tester_host_wifi_state;

static tester_host_wifi_state_t host_state;
static host_command_t host_waiting_command;
static uint32_t host_deadline_ms;
static uint32_t host_backoff_deadline_ms;
static uint32_t host_ip_retry_due_ms;
static uint32_t host_session_deadline_ms;
static uint8_t host_kick_pending;
static uint8_t host_kick_term_pending;
static uint32_t host_kick_deadline_ms;
static uint8_t host_first_start_pending;
static uint8_t host_beacon_pending;
static uint8_t host_beacon_tx_done;
static uint8_t host_beacon_link_open;  /* link4 UDP 链路已开（监听或广播） */
static uint8_t host_query_reply_pending;  /* 收到同线查询，待广播回复 HOST/PORT */
static uint32_t host_beacon_deadline_ms;
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
static tester_host_wifi_request_t host_request_queue[HOST_WIFI_QUEUE_DEPTH];
static uint8_t host_request_head;
static uint8_t host_request_tail;
static host_seen_t host_seen[HOST_WIFI_SEEN_DEPTH];
static uint8_t host_seen_next;
static char host_status[HOST_WIFI_STATUS_MAX];

/* ===== 第二阶段：打印请求 client 发送（测试机侧 → 打印侧 5006） ===== */
typedef enum {
  HOST_TX_IDLE = 0,
  HOST_TX_CLOSE,        /* 已发 AT+CIPCLOSE（清残留连接——对齐旧引擎 CLOSE_OLD），等响应 */
  HOST_TX_CLOSE_DELAY,  /* CIPCLOSE 响应已回：等 ESP 内部收尾（防 busy p...），到期发 CIPSTART */
  HOST_TX_CONNECT,      /* 已发 AT+CIPSTART，等 OK */
  HOST_TX_SEND,         /* 已发 AT+CIPSEND，等 '>' */
  HOST_TX_DATA,         /* 已发帧，等 SEND OK */
  HOST_TX_WAIT_REPLY    /* 等 +IPD（ACK/PRINTING/DONE/ERROR） */
} host_tx_phase_t;

static host_tx_phase_t host_tx_phase;
static char host_tx_frame[TESTER_WIFI_TX_FRAME_MAX];
static uint16_t host_tx_frame_len;
static uint32_t host_tx_event_id;
static uint8_t host_tx_acked;
static uint8_t host_tx_printing;
static uint8_t host_tx_done;
static uint8_t host_tx_failed;
static uint32_t host_tx_deadline_ms;
static uint8_t host_tx_retries;
static uint8_t host_printer_linked;  /* TCP 已连上打印侧（CIPSTART OK） */
static uint8_t host_tx_link_test;    /* 连接测试模式：CIPSTART OK 后直接 CLOSE */
static uint32_t host_link_test_deadline_ms;
static char host_tx_fail_line[64];   /* 最近一次连接测试失败的 ESP 响应行（诊断） */
static uint32_t host_tx_fail_count;  /* 连接测试失败计数（诊断） */

/* ===== 第二阶段：打印侧自动发现（UDP 广播查询 PD LINE） ===== */
typedef enum {
  HOST_DISC_IDLE = 0,
  HOST_DISC_CONNECT,   /* 已发 CIPSTART UDP 广播，等 OK */
  HOST_DISC_SEND,      /* 已发 CIPSEND，等 '>' */
  HOST_DISC_DATA,      /* 已发查询帧，等 SEND OK */
  HOST_DISC_LISTEN,    /* 已发监听 CIPSTART，等 OK */
  HOST_DISC_WAIT,      /* 等打印侧广播回复（+IPD） */
  HOST_DISC_CLOSE      /* 关链路 */
} host_disc_phase_t;

static host_disc_phase_t host_disc_phase;
static uint32_t host_disc_deadline_ms;
static char host_disc_query[64];
static uint16_t host_disc_query_len;
static char host_discovered_ip[20];
static uint16_t host_discovered_port;
static uint8_t host_discovered_count;
static uint8_t host_discovered_matched;
static char host_ip[HOST_WIFI_IP_MAX];
static char host_mac[HOST_WIFI_MAC_MAX];

static uint8_t host_time_reached(uint32_t deadline)
{
  return ((int32_t)(tester_wifi_print_now_ms() - deadline) >= 0) ? 1U : 0U;
}

static void host_set_state(tester_host_wifi_state_t state, const char *text)
{
  host_state = state;
  g_tester_host_wifi_state = (uint8_t)state;
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
    g_tester_host_wifi_error_count++;
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

static uint8_t host_json_bool_or_result(const char *json, tester_host_wifi_request_t *request)
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
                                  tester_host_wifi_request_t *request)
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

/* link4 收到的 UDP 载荷：测试机侧 PD LINE 查询（{"type":"query","line_id":"L04"}）。
 * 匹配本机 line_id 则广播回复自身 HOST/PORT；非本线查询忽略。 */
static uint8_t host_issue_or_retry(host_command_t command);
static uint8_t host_handle_query(const char *line)
{
  uint8_t link_id;
  const char *payload;
  uint16_t payload_length;
  const char *p;
  const char *end;
  char query_line[8];
  const device_config_t *store;

  if(host_parse_ipd(line, &link_id, &payload, &payload_length) == 0U ||
     link_id != HOST_WIFI_BEACON_LINK ||
     strstr(payload, "\"type\":\"query\"") == 0) {
    return 0U;
  }
  p = strstr(payload, "\"line_id\":\"");
  if(p == 0) {
    return 0U;
  }
  p += 11U;
  end = strchr(p, '"');
  if(end == 0 || (size_t)(end - p) >= sizeof(query_line)) {
    return 0U;
  }
  (void)memcpy(query_line, p, (size_t)(end - p));
  query_line[(size_t)(end - p)] = '\0';
  store = device_config_get();
  if(store == 0 || strcmp(query_line, store->line_id) != 0) {
    return 0U;  /* 非本线查询：忽略 */
  }
  /* 匹配：标记待回复；空闲时立即关监听切广播链路回复。 */
  host_query_reply_pending = 1U;
  if(host_waiting_command == HOST_CMD_NONE && host_tx_active == 0U) {
    (void)host_issue_or_retry(HOST_CMD_BEACON_CLOSE);
  }
  return 1U;
}

static void host_receive_request(const char *line)
{
  const char *payload;
  uint16_t payload_length;
  uint8_t link_id;
  tester_host_wifi_request_t request;
  uint8_t duplicate_state = 0U;
  uint8_t next;
  char response[64];

  if(host_parse_ipd(line, &link_id, &payload, &payload_length) == 0U) {
    return;
  }
  if(host_parse_request(payload, payload_length, link_id, &request) == 0U) {
    g_tester_host_wifi_rx_error_count++;
    return;
  }

  if(host_seen_find(request.event_id, request.device_uid, &duplicate_state) != 0U) {
    g_tester_host_wifi_rx_duplicate_count++;
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
    g_tester_host_wifi_rx_error_count++;
    (void)snprintf(response, sizeof(response), "ERROR,%lu\n",
                   (unsigned long)request.event_id);
    (void)host_queue_tx(request.link_id, response);
    return;
  }
  host_request_queue[host_request_head] = request;
  host_request_head = next;
  host_seen_add(request.event_id, request.device_uid, request.link_id);
  g_tester_host_wifi_rx_request_count++;
  (void)snprintf(response, sizeof(response), "ACK,%lu,QUEUED\n",
                 (unsigned long)request.event_id);
  if(host_queue_tx(request.link_id, response) != 0U) {
    g_tester_host_wifi_ack_count++;
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
  const device_config_t *config = device_config_get();
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

static uint16_t host_last_retry_line;   /* 调试：最近一次重试的调用点行号 */
static uint8_t host_ever_online;        /* 首次 ONLINE 前不报 NETWORK ERROR */
static void host_schedule_retry_at(uint16_t line);
#define host_schedule_retry() host_schedule_retry_at((uint16_t)__LINE__)
static void host_schedule_ip_retry(void);

static uint8_t host_issue_command(host_command_t command)
{
  char text[HOST_WIFI_COMMAND_MAX];
  const device_config_t *config = device_config_get();
  uint32_t timeout = HOST_WIFI_COMMAND_TIMEOUT;
  uint32_t now_ms;

  if(config == 0) {
    return 0U;
  }
  text[0] = '\0';
  switch(command) {
  case HOST_CMD_AT: (void)snprintf(text, sizeof(text), "AT"); break;
  case HOST_CMD_STOP_SERVER:
    /* 跳过 CIPSERVER=0：settings 手动测试序列（无 CIPSERVER=0）CIPSTART
     * 成功；CIPSERVER=0 会把 ESP 带入异常态，后续 CIPSTART busy/静默。 */
    (void)snprintf(text, sizeof(text), "AT");
    break;
  case HOST_CMD_MODE: (void)snprintf(text, sizeof(text), "AT+CWMODE=1"); break;
  case HOST_CMD_JOIN:
    if(host_build_join(text, sizeof(text)) == 0U) return 0U;
    timeout = HOST_WIFI_JOIN_TIMEOUT;
    break;
  case HOST_CMD_IP:
    (void)snprintf(text, sizeof(text), "AT+CIFSR");
    timeout = HOST_WIFI_IP_TIMEOUT;
    break;
  case HOST_CMD_MUX:
    /* 客户端标准 MUX=0（无 link id 的 CIPSTART/CIPSEND 格式）：settings
     * 手动测试（AT+CIPSTART="TCP","host",port）在此格式下实测成功
     * （2026-08-13 用户验证）。MUX=1 + CIPSTART=1 曾尝试照抄打印侧，
     * 但打印侧是服务器（CIPSTART=1 只用于自检回环），客户端外部连接
     * 失败且破坏 settings 手动测试。 */
    (void)snprintf(text, sizeof(text), "AT+CIPMUX=0");
    break;
  case HOST_CMD_TRANS_MODE:
    (void)snprintf(text, sizeof(text), "AT+CIPMODE=0");
    break;
  case HOST_CMD_SERVER:
    /* 客户端（MUX=0）不监听：不发 CIPSERVER——部分 ESP-AT 版本在
     * MUX=0 下会接受 CIPSERVER=1,<port> 并自动切 MUX=1，此后无 link id
     * 的 CIPSTART 全部 ERROR（item27 实测：MUX=1+CIPSERVER+CIPSTART
     * 并存被拒）。用普通 AT 保持链路活跃，推进 ONLINE。 */
    (void)snprintf(text, sizeof(text), "AT");
    break;
  case HOST_CMD_BEACON_START:
    /* 打开 link4 UDP 通配监听(5002)：接收测试机侧 PD LINE 查询
     * （udp_mode=2 纯接收，常开，不再周期广播——周期广播实测打 WiFi）。 */
    (void)snprintf(text, sizeof(text),
                   "AT+CIPSTART=%u,\"UDP\",\"0.0.0.0\",0,%u,2",
                   (unsigned int)HOST_WIFI_BEACON_LINK,
                   (unsigned int)HOST_WIFI_BEACON_PORT);
    break;
  case HOST_CMD_BEACON_CLOSE:
    (void)snprintf(text, sizeof(text), "AT+CIPCLOSE=%u",
                   (unsigned int)HOST_WIFI_BEACON_LINK);
    break;
  case HOST_CMD_BEACON_TX:
    /* 收到同线查询后临时改广播链路(255.255.255.255:5002)回复自身 HOST/PORT，
     * 回复完立即关闭并重开监听。 */
    (void)snprintf(text, sizeof(text),
                   "AT+CIPSTART=%u,\"UDP\",\"255.255.255.255\",%u,%u,0",
                   (unsigned int)HOST_WIFI_BEACON_LINK,
                   (unsigned int)HOST_WIFI_BEACON_PORT,
                   (unsigned int)HOST_WIFI_BEACON_PORT);
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
    host_set_state(TESTER_HOST_WIFI_JOINING, "JOINING WIFI");
  } else if(command == HOST_CMD_IP) {
    host_set_state(TESTER_HOST_WIFI_JOINING, "WAIT DHCP / READ IP");
  } else if(command == HOST_CMD_SERVER) {
    host_set_state(TESTER_HOST_WIFI_STARTING_SERVER, "STARTING SERVER");
  } else {
    host_set_state(TESTER_HOST_WIFI_STARTING, "WIFI STARTING");
  }
  return 1U;
}

static void host_schedule_retry_at(uint16_t line)
{
  host_last_retry_line = line;
  wifi_esp_session_note_end();  /* 会话失败：ESP 零响应则累计，超阈值 EN 复位 */
  host_waiting_command = HOST_CMD_NONE;
  host_ip_retry_pending = 0U;
  host_session_deadline_ms = 0U;
  host_kick_pending = 0U;
  tester_wifi_print_at_end();
  host_state = TESTER_HOST_WIFI_ERROR;
  g_tester_host_wifi_state = (uint8_t)host_state;
  /* Keep the operator-facing state explicit.  The backoff/reconnect counter
   * remains internal; the LCDM should say NETWORK ERROR when the controller
   * cannot maintain its AP/server session. */
  (void)snprintf(host_status, sizeof(host_status), "NETWORK ERROR");
  host_backoff_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BACKOFF_MS;
  g_tester_host_wifi_reconnect_count++;
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
  host_set_state(TESTER_HOST_WIFI_JOINING, "WAIT DHCP / RETRY IP");
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

/* 构造当前 line_id/IP/监听端口的信标 JSON 并入队（link4 CIPSEND 发送）。
 * 常开链路方案：链路已开时每周期只调本函数（CIPSEND），不再建拆 UDP。 */
static void host_queue_beacon(void)
{
  char beacon[HOST_WIFI_COMMAND_MAX];
  const device_config_t *store = device_config_get();

  (void)snprintf(beacon, sizeof(beacon),
                 "{\"type\":\"beacon\",\"line_id\":\"%s\",\"ip\":\"%s\",\"port\":%u}\n",
                 (store != 0) ? store->line_id : "",
                 (host_ip[0] != '\0') ? host_ip : "0.0.0.0",
                 (store != 0) ? 5006U : 0U);
  (void)host_queue_tx(HOST_WIFI_BEACON_LINK, beacon);
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
    host_set_state(TESTER_HOST_WIFI_ONLINE, "WIFI SERVER ONLINE");
    wifi_esp_session_mark_ok();
    host_ever_online = 1U;  /* 自连完成：此后断连才允许报 NETWORK ERROR */
    /* 禁用 UDP 监听自动开：实测 UDP 链路(mode0/2)会触发 ESP 周期
     * WIFI DISCONNECT(652f941 A/B 验证, 手动连接无 UDP 所以稳定)。
     * ONLINE 后零 UDP 操作, 保持与手动连接一致的干净状态；
     * HOST/PORT 同步作为第二件事另行设计, 不走 UDP 自动同步。 */
    host_beacon_link_open = 0U;
    host_query_reply_pending = 0U;
    host_beacon_deadline_ms = 0U;
    break;
  case HOST_CMD_BEACON_START:
    /* link4 UDP 监听已开（CIPSTART OK）：保持常开收查询。 */
    host_beacon_link_open = 1U;
    break;
  case HOST_CMD_BEACON_TX:
    /* 广播链路已开：CIPSEND 回复自身 HOST/PORT（走 host_tx 队列）。 */
    host_beacon_link_open = 1U;
    host_queue_beacon();
    break;
  case HOST_CMD_BEACON_CLOSE:
    host_beacon_link_open = 0U;
    host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
    if(host_query_reply_pending != 0U) {
      /* 刚关的是监听：切广播链路回复查询。 */
      (void)host_issue_or_retry(HOST_CMD_BEACON_TX);
    } else {
      /* 刚关的是广播（回复已发出）：重开监听收后续查询。 */
      (void)host_issue_or_retry(HOST_CMD_BEACON_START);
    }
    break;
  default:
    break;
  }
}

static uint8_t host_tx_start_current(void);

static void host_tx_start_failed(void)
{
  /* 信标 TX 失败只重置信标周期，绝不打断 ONLINE 会话；业务 TX 失败才重连 */
  if(host_tx_current.link_id == HOST_WIFI_BEACON_LINK) {
    host_beacon_pending = 0U;
    host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
  } else {
    g_tester_host_wifi_error_count++;
    host_schedule_retry();
  }
}

static void host_process_line(const char *line)
{
  if(line == 0 || line[0] == '\0') {
    return;
  }
  if(strncmp(line, "+IPD,", 5U) == 0) {
    if(host_tx_phase == HOST_TX_WAIT_REPLY) {
      /* 打印侧回执：ACK,<id>,QUEUED / PRINTING,<id> / DONE,<id> / ERROR,<id> */
      const char *p = strchr(line, ':');
      if(p != 0) {
        if(strstr(p, "DONE,") != 0) {
          host_tx_done = 1U;
        } else if(strstr(p, "PRINTING,") != 0) {
          host_tx_printing = 1U;
        } else if(strstr(p, "ACK,") != 0) {
          host_tx_acked = 1U;
        } else if(strstr(p, "ERROR,") != 0) {
          host_tx_failed = 1U;
        }
      }
      return;
    }
    if(host_handle_query(line) != 0U) {
      return;
    }
    host_receive_request(line);
    return;
  }
  if(host_tx_phase == HOST_TX_CLOSE) {
    /* CIPCLOSE 响应（OK/ERROR——无论有无残留连接）：ESP 内部可能仍在
     * 收尾，立即 CIPSTART 会撞 "busy p..."。等 CLOSE_DELAY 延时到期
     * （host_tx_service 超时分支）再发起 CIPSTART。 */
    host_tx_phase = HOST_TX_CLOSE_DELAY;
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 500UL;
    return;
  }
  if(host_tx_phase == HOST_TX_CONNECT && strcmp(line, "OK") == 0) {
    char command[64];
    host_printer_linked = 1U;  /* TCP 已连上打印侧 → 主屏深绿闪烁 */
    if(host_tx_link_test != 0U) {
      /* 周期连接测试：连接成功即保持（不 CIPCLOSE）——频繁 connect/close
       * 会把打印侧 ESP 的 CIPSERVER 打崩（实测 8008 对外 refused）。 */
      host_tx_link_test = 0U;
      host_tx_phase = HOST_TX_IDLE;
      return;
    }
    (void)snprintf(command, sizeof(command), "AT+CIPSEND=%u",
                   (unsigned int)host_tx_frame_len);
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                          (uint16_t)strlen(command));
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
    host_tx_phase = HOST_TX_SEND;
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
    return;
  }
  if(host_tx_phase == HOST_TX_SEND && strcmp(line, ">") == 0) {
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)host_tx_frame,
                                          host_tx_frame_len);
    host_tx_phase = HOST_TX_DATA;
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
    return;
  }
  if(host_tx_phase == HOST_TX_DATA && strcmp(line, "SEND OK") == 0) {
    host_tx_phase = HOST_TX_WAIT_REPLY;
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 15000UL;
    return;
  }
  if(host_tx_phase != HOST_TX_IDLE && host_tx_phase != HOST_TX_WAIT_REPLY &&
     (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0 ||
      strstr(line, "busy p...") != 0 || strstr(line, "link is not valid") != 0 ||
      strstr(line, "CONNECT FAIL") != 0 || strstr(line, "connect fail") != 0)) {
    /* 诊断：记录失败行（连接测试失败原因定位）。 */
    (void)snprintf(host_tx_fail_line, sizeof(host_tx_fail_line), "%s", line);
    host_tx_fail_count++;
    host_printer_linked = 0U;  /* 连接失败：主屏停止闪烁，真实反映未连上打印侧 */
    host_tx_phase = HOST_TX_IDLE;
    host_tx_failed = 1U;
    return;
  }
  if(host_tx_phase != HOST_TX_IDLE &&
     (strcmp(line, "CLOSED") == 0 || strcmp(line, "WIFI DISCONNECT") == 0 ||
      strcmp(line, "WIFI DISCONNECTED") == 0)) {
    host_tx_failed = 1U;
  }
  if(host_disc_phase != HOST_DISC_IDLE) {
    if(host_disc_phase == HOST_DISC_CONNECT && strcmp(line, "OK") == 0) {
      char cmd[32];
      (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u",
                     (unsigned int)host_disc_query_len);
      (void)tester_wifi_print_at_send_bytes((const uint8_t *)cmd, (uint16_t)strlen(cmd));
      (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
      host_disc_phase = HOST_DISC_SEND;
      host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
      return;
    }
    if(host_disc_phase == HOST_DISC_SEND && strcmp(line, ">") == 0) {
      (void)tester_wifi_print_at_send_bytes((const uint8_t *)host_disc_query,
                                            host_disc_query_len);
      host_disc_phase = HOST_DISC_DATA;
      host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
      return;
    }
    if(host_disc_phase == HOST_DISC_DATA && strcmp(line, "SEND OK") == 0) {
      (void)tester_wifi_print_at_send_bytes(
          (const uint8_t *)"AT+CIPSTART=\"UDP\",\"0.0.0.0\",0,5002,2\r\n", 41U);
      host_disc_phase = HOST_DISC_LISTEN;
      host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
      return;
    }
    if(host_disc_phase == HOST_DISC_LISTEN && strcmp(line, "OK") == 0) {
      host_disc_phase = HOST_DISC_WAIT;
      host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
      return;
    }
    if(host_disc_phase == HOST_DISC_CLOSE && strcmp(line, "OK") == 0) {
      host_disc_phase = HOST_DISC_IDLE;
      return;
    }
    if(host_disc_phase == HOST_DISC_WAIT && strncmp(line, "+IPD,", 5U) == 0) {
      /* 打印侧广播回复 beacon：{"type":"beacon","line_id":"...","ip":"...","port":N} */
      const char *p = strchr(line, ':');
      if(p != 0 && strstr(p, "\"type\":\"beacon\"") != 0) {
        const device_config_t *config = device_config_get();
        const char *ip = strstr(p, "\"ip\":\"");
        const char *pt = strstr(p, "\"port\":");
        const char *li = strstr(p, "\"line_id\":\"");
        const char *e;
        if(ip != 0) {
          ip += 6;
          e = strchr(ip, '"');
          if(e != 0 && (size_t)(e - ip) < sizeof(host_discovered_ip)) {
            (void)memcpy(host_discovered_ip, ip, (size_t)(e - ip));
            host_discovered_ip[(size_t)(e - ip)] = '\0';
          }
        }
        if(pt != 0) {
          host_discovered_port = (uint16_t)strtoul(pt + 7, 0, 10);
        }
        host_discovered_count++;
        if(li != 0 && config != 0) {
          li += 11;
          e = strchr(li, '"');
          if(e != 0 && (size_t)(e - li) == strlen(config->line_id) &&
             strncmp(li, config->line_id, (size_t)(e - li)) == 0) {
            host_discovered_matched = 1U;  /* PD LINE 一致才采用 */
          }
        }
        host_disc_phase = HOST_DISC_CLOSE;
        host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
        (void)tester_wifi_print_at_send_bytes((const uint8_t *)"AT+CIPCLOSE\r\n", 13U);
      }
      return;
    }
    if(host_disc_phase != HOST_DISC_WAIT &&
       (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0)) {
      host_disc_phase = HOST_DISC_IDLE;  /* 查询失败：下次再试 */
      return;
    }
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
      (void)device_config_save_wifi_mac(host_mac);
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
    if(host_tx_current.link_id == HOST_WIFI_BEACON_LINK) {
      host_beacon_tx_done = 1U;
    }
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
        host_tx_start_failed();
      }
    } else {
      host_tx_active = 0U;
      if(host_tx_current.link_id == HOST_WIFI_BEACON_LINK) {
        /* 信标发送失败（含 link is not valid 链路丢失）：下个周期重开链路，
         * 不计入业务错误。 */
        host_beacon_link_open = 0U;
        host_beacon_pending = 0U;
        host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
      } else {
        g_tester_host_wifi_error_count++;
      }
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
    } else if(host_waiting_command == HOST_CMD_BEACON_START ||
              host_waiting_command == HOST_CMD_BEACON_CLOSE) {
      /* 信标链路忙/无套接字：下个周期再试，不影响 TCP 服务。 */
      host_waiting_command = HOST_CMD_NONE;
      host_beacon_pending = 0U;
      host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
    } else if(host_waiting_command == HOST_CMD_SERVER) {
      /* CIPSERVER 在 MUX=0 下会被 ESP 拒（ERROR）——旧引擎同样失败但
       * 无妨：client 连接（CIPSTART）不依赖本地监听，照常推进 ONLINE。 */
      host_process_ok();
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
       host_state == TESTER_HOST_WIFI_JOINING) {
      return;
    }
    if(host_state == TESTER_HOST_WIFI_ONLINE) {
      /* ONLINE 期间的断连上报：只做 JOIN 级重连，不重启整个会话状态机，
       * 深绿不掉（避免每次信标周期后的断连上报把会话打回 STARTING）。 */
      host_set_state(TESTER_HOST_WIFI_JOINING, "WIFI REJOIN");
      g_tester_settings_wifi_test_passed = 0U;  /* 真断开：K2 深绿门失效，随 host 状态 */
      (void)host_issue_or_retry(HOST_CMD_JOIN);
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
  if(host_state != TESTER_HOST_WIFI_ONLINE) {
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
          host_tx_start_failed();
        }
        return;
      } else {
        host_tx_active = 0U;
        host_tx_waiting_prompt = 0U;
        host_tx_waiting_result = 0U;
        g_tester_host_wifi_error_count++;
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
    host_tx_start_failed();
  }
}

void tester_host_wifi_init(void)
{
  wifi_esp_en_init();  /* PA8 = ESP EN：默认运行态 */
  wifi_esp_en_reset(); /* 上电先拉低 EN 复位 ESP：干净启动 → 快速连接
                        * （约 6 秒出绿，不再等 18 秒看门狗才复位） */
  host_state = TESTER_HOST_WIFI_STOPPED;
  host_waiting_command = HOST_CMD_NONE;
  host_deadline_ms = 0U;
  host_backoff_deadline_ms = 0U;
  host_ip_retry_due_ms = 0U;
  host_session_deadline_ms = 0U;
  host_kick_pending = 0U;
  host_kick_deadline_ms = 0U;
  host_beacon_pending = 0U;
  host_beacon_tx_done = 0U;
  host_beacon_link_open = 0U;
  host_query_reply_pending = 0U;
  host_beacon_deadline_ms = 0U;
  host_first_start_pending = 1U;
  host_first_start_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_ESP_BOOT_GRACE_MS;
  host_ip_retry_pending = 0U;
  host_got_ip = 0U;
  host_reset_tx_queue();
  host_reset_request_queue();
  host_reset_seen();
  host_ip[0] = '\0';
  host_mac[0] = '\0';
  host_ever_online = 0U;  /* 上电清零：首次 ONLINE 前不报 NETWORK ERROR */
  (void)snprintf(host_status, sizeof(host_status), "WAITING");
  g_tester_host_wifi_rx_request_count = 0U;
  g_tester_host_wifi_rx_duplicate_count = 0U;
  g_tester_host_wifi_rx_error_count = 0U;
  g_tester_host_wifi_ack_count = 0U;
  g_tester_host_wifi_done_count = 0U;
  g_tester_host_wifi_error_count = 0U;
  g_tester_host_wifi_reconnect_count = 0U;
  g_tester_host_wifi_state = (uint8_t)host_state;
}

void tester_host_wifi_start(void)
{
  const device_config_t *config = device_config_get();

  wifi_esp_session_note_start();  /* 会话起点：记录帧计数用于无响应判定 */
  host_printer_linked = 0U;  /* 新会话：重新确认打印侧连接 */
  /* WiFi 连接只依赖 SSID（与打印侧一致）；HOST/PORT 属第二阶段通信配置，
   * 与 WiFi 连接/深绿完全分开，不参与启动校验。 */
  if(config == 0 || config->wifi_ssid[0] == '\0') {
    tester_wifi_print_at_end();
    host_waiting_command = HOST_CMD_NONE;
    host_set_state(TESTER_HOST_WIFI_CONFIG_REQUIRED, "SET WIFI SSID");
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
  host_set_state(TESTER_HOST_WIFI_STARTING, "WIFI STARTING");
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

void tester_host_wifi_restart(void)
{
  tester_host_wifi_start();
}

/* K2 手动测试通过后复用已连的 ESP：直接从 CIFSR 验证 IP（不重新 kick/
 * CWJAP——ESP 已关联同一 AP），几秒内回到 ONLINE（深绿）。与打印侧
 * "连接成功即保持"的观感一致。 */
void tester_host_wifi_resume(void)
{
  wifi_esp_session_note_start();
  host_waiting_command = HOST_CMD_NONE;
  host_kick_pending = 0U;
  host_kick_term_pending = 0U;
  host_ip_retry_pending = 0U;
  host_got_ip = 0U;
  host_ip[0] = '\0';
  host_mac[0] = '\0';
  host_set_state(TESTER_HOST_WIFI_JOINING, "WAIT DHCP / READ IP");
  if(host_issue_command(HOST_CMD_IP) == 0U) {
    host_schedule_retry();
  }
}

/* 第二阶段：发起打印请求（TCP 连打印侧 → CIPSEND 发帧 → 等回执）。
 * 帧格式（tester_wifi_build_print_frame）与打印侧 host_receive_request 匹配。 */
uint8_t tester_host_wifi_request_print(uint32_t event_id, uint32_t test_count,
                                       uint16_t pair_count, uint16_t point_count)
{
  const device_config_t *config = device_config_get();
  char command[192];

  if(event_id == 0U || config == 0 || config->service_host[0] == '\0' ||
     config->service_port == 0U || host_tx_phase != HOST_TX_IDLE ||
     host_state != TESTER_HOST_WIFI_ONLINE) {
    return 0U;
  }
  host_tx_link_test = 0U;  /* 打印请求优先于周期连接测试 */
  if(tester_wifi_build_print_frame(event_id, test_count, pair_count, point_count,
                                   host_tx_frame, &host_tx_frame_len) == 0U) {
    return 0U;
  }
  host_tx_event_id = event_id;
  host_tx_acked = 0U;
  host_tx_printing = 0U;
  host_tx_done = 0U;
  host_tx_failed = 0U;
  host_tx_retries = 0U;
  if(host_printer_linked != 0U) {
    /* 连接已保持：跳过 CIPSTART 直接 CIPSEND。 */
    host_tx_phase = HOST_TX_SEND;
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
    (void)snprintf(command, sizeof(command), "AT+CIPSEND=%u",
                   (unsigned int)host_tx_frame_len);
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                          (uint16_t)strlen(command));
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
  } else {
    host_tx_phase = HOST_TX_CONNECT;  /* 未连接：CIPSTART（MUX=0 无 link
                                        * id，settings 手动测试同格式） */
    host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
    (void)snprintf(command, sizeof(command), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   config->service_host, (unsigned int)config->service_port);
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                          (uint16_t)strlen(command));
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
  }
  return 1U;
}

/* 轮询打印事件（按 event_id 匹配；ACK→ACK_QUEUED，DONE/ERROR 后复位发送态）。 */
tester_wifi_print_event_t tester_host_wifi_print_event(uint32_t event_id)
{
  if(host_tx_phase == HOST_TX_IDLE || host_tx_event_id != event_id) {
    return TESTER_WIFI_PRINT_EVENT_NONE;
  }
  if(host_tx_failed != 0U) {
    host_tx_phase = HOST_TX_IDLE;
    return TESTER_WIFI_PRINT_EVENT_ERROR;
  }
  if(host_tx_done != 0U) {
    host_tx_phase = HOST_TX_IDLE;
    return TESTER_WIFI_PRINT_EVENT_DONE;
  }
  if(host_tx_printing != 0U) {
    return TESTER_WIFI_PRINT_EVENT_PRINTING;
  }
  if(host_tx_acked != 0U) {
    return TESTER_WIFI_PRINT_EVENT_ACK_QUEUED;
  }
  return TESTER_WIFI_PRINT_EVENT_NONE;
}

uint8_t tester_host_wifi_printer_linked(void)
{
  return host_printer_linked;
}

/* 触发打印侧自动发现：UDP 广播查询（带本机 line_id），打印侧监听 5002
 * 且 line 匹配时广播回复 HOST/PORT。返回 1=已入发现状态机。 */
uint8_t tester_host_wifi_discover_start(void)
{
  const device_config_t *config = device_config_get();
  char command[96];
  int written;

  if(config == 0 || host_disc_phase != HOST_DISC_IDLE ||
     host_tx_phase != HOST_TX_IDLE || host_state != TESTER_HOST_WIFI_ONLINE) {
    return 0U;
  }
  written = snprintf(host_disc_query, sizeof(host_disc_query),
                     "{\"type\":\"query\",\"line_id\":\"%s\"}\n",
                     config->line_id);
  if(written <= 0 || (uint32_t)written >= sizeof(host_disc_query)) {
    return 0U;
  }
  host_disc_query_len = (uint16_t)written;
  host_discovered_count = 0U;
  host_discovered_matched = 0U;
  host_discovered_port = 0U;
  host_discovered_ip[0] = '\0';
  host_disc_phase = HOST_DISC_CONNECT;
  host_disc_deadline_ms = tester_wifi_print_now_ms() + 5000UL;
  (void)snprintf(command, sizeof(command),
                 "AT+CIPSTART=\"UDP\",\"255.255.255.255\",5002,5002,0");
  (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                        (uint16_t)strlen(command));
  (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
  return 1U;
}

uint8_t tester_host_wifi_discovered_count(void)
{
  return host_discovered_count;
}

uint8_t tester_host_wifi_discovered_matched(void)
{
  return host_discovered_matched;
}

const char *tester_host_wifi_discovered_ip(void)
{
  return host_discovered_ip;
}

uint16_t tester_host_wifi_discovered_port(void)
{
  return host_discovered_port;
}

/* 周期连接测试：ONLINE 后每 10 秒自动连一次打印侧（验证双向可达），
 * CIPSTART OK 即置 printer_linked（主屏深绿闪烁持续指示"已连上打印机"）。 */
static uint8_t host_link_test_start(void)
{
  const device_config_t *config = device_config_get();

  if(config == 0 || config->service_host[0] == '\0' || config->service_port == 0U ||
     host_tx_phase != HOST_TX_IDLE || host_disc_phase != HOST_DISC_IDLE ||
     host_state != TESTER_HOST_WIFI_ONLINE || host_printer_linked != 0U) {
    /* host_printer_linked!=0：连接已保持（测试成功不关闭），跳过测试——
     * 避免频繁 connect/close 把打印侧 ESP 的 CIPSERVER 打崩（实测打印侧
     * 被每 5 秒的连接测试打崩后 8008 对外 refused）。 */
    return 0U;
  }
  /* 直接 CIPSTART（无前置 CIPCLOSE）：CLOSE 响应后立即 CIPSTART 会让
   * ESP-AT 报 "busy p..."（上一命令内部未收尾）。settings 手动测试
   * 成功正是因为 CIPSTART 前 ESP 完全空闲（WiFi 重连序列耗时数秒）。
   * ONLINE 后 ESP 空闲，直接发起即同条件；残留连接由 ESP CIPSTO 超时
   * 自动清理。 */
  host_tx_link_test = 1U;
  host_tx_phase = HOST_TX_CONNECT;
  host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
  {
    char command[192];
    (void)snprintf(command, sizeof(command), "AT+CIPSTART=1,\"TCP\",\"%s\",%u",
                   config->service_host, (unsigned int)config->service_port);
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                          (uint16_t)strlen(command));
    (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
  }
  return 1U;
}

void tester_host_wifi_service(void)
{
  char line[HOST_WIFI_LINE_MAX];
  uint32_t now_ms;

  /* Keep the shared decoder and time base running even while the first
   * session is held by the boot grace. */
  tester_wifi_print_service();
  wifi_esp_watchdog_poll();  /* 2 分钟未 ONLINE → EN 复位兜底 */

  /* K2 手动测试运行中：host 引擎暂停发命令/poll，由 settings 状态机
   * 独占 ESP；测试结束（g_tester_settings_wifi_test_running=0）恢复。
   * 注意不能用 raw_hold 判断——host 引擎自己的 start 也经 at_begin
   * 置位 raw_hold，会自锁。 */
  if(g_tester_settings_wifi_test_running != 0U) {
    return;
  }

  if(host_first_start_pending != 0U) {
    if(host_time_reached(host_first_start_deadline_ms) == 0U) {
      return;  /* ESP-AT still booting after power-up */
    }
    host_first_start_pending = 0U;
    tester_host_wifi_start();
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
  if(host_state == TESTER_HOST_WIFI_ERROR && host_time_reached(host_backoff_deadline_ms) != 0U) {
    tester_host_wifi_start();
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
    } else if(host_waiting_command == HOST_CMD_BEACON_START ||
              host_waiting_command == HOST_CMD_BEACON_CLOSE) {
      host_waiting_command = HOST_CMD_NONE;
      host_beacon_pending = 0U;
      host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
    } else {
      host_schedule_retry();
    }
  }

  /* link4 UDP 监听：接收测试机侧 PD LINE 查询，按需广播回复 HOST/PORT。
   * 空闲时保证监听链路开着；回复流程由命令 OK/tx_done 驱动。 */
  if(host_state == TESTER_HOST_WIFI_ONLINE) {
    /* 打印发送状态机超时推进（CIPSTART/CIPSEND/等回执）。 */
    if(host_tx_phase != HOST_TX_IDLE && host_waiting_command == HOST_CMD_NONE &&
       host_time_reached(host_tx_deadline_ms) != 0U) {
      if(host_tx_phase == HOST_TX_CLOSE_DELAY) {
        /* CLOSE 响应后的延时结束：ESP 已空闲，发起 CIPSTART（对齐
         * settings 手动测试——CIPSTART 前 ESP 完全空闲才成功）。 */
        const device_config_t *config = device_config_get();
        char command[192];
        host_tx_phase = HOST_TX_CONNECT;
        host_tx_deadline_ms = tester_wifi_print_now_ms() + 8000UL;
        if(config != 0 && config->service_host[0] != '\0' && config->service_port != 0U) {
          (void)snprintf(command, sizeof(command), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                         config->service_host, (unsigned int)config->service_port);
          (void)tester_wifi_print_at_send_bytes((const uint8_t *)command,
                                                (uint16_t)strlen(command));
          (void)tester_wifi_print_at_send_bytes((const uint8_t *)"\r\n", 2U);
        } else {
          host_tx_phase = HOST_TX_IDLE;
          host_tx_failed = 1U;
        }
      } else {
        host_tx_phase = HOST_TX_IDLE;
        host_tx_failed = 1U;
      }
    }
    /* 自动发现超时推进（查询/监听窗口）。 */
    if(host_disc_phase != HOST_DISC_IDLE &&
       host_time_reached(host_disc_deadline_ms) != 0U) {
      host_disc_phase = HOST_DISC_IDLE;
    }
    /* 周期连接测试：完全禁用——实测每 5 秒的 CIPSTART/CIPCLOSE 会把
     * 打印侧 ESP 的 CIPSERVER 打崩（8008 对外 refused）且测试侧 ESP
     * 自身也会挂死（静默）。打印请求时才连接（偶发，一次连接多次发送）。 */
    if(0 && host_tx_phase == HOST_TX_IDLE && host_disc_phase == HOST_DISC_IDLE &&
       host_time_reached(host_link_test_deadline_ms) != 0U) {
      host_link_test_deadline_ms = tester_wifi_print_now_ms() + 60000UL;  /* 60s：连接保持时无需频繁测试 */
      (void)host_link_test_start();
    }
    if(host_query_reply_pending != 0U && host_waiting_command == HOST_CMD_NONE &&
       host_tx_active == 0U) {
      /* 查询待回复（链路忙时延迟）：关监听 → 切广播链路回复。 */
      (void)host_issue_command(HOST_CMD_BEACON_CLOSE);
    } else if(host_beacon_tx_done != 0U && host_waiting_command == HOST_CMD_NONE) {
      /* 回复已发出（CIPSEND SEND OK）：关广播，随后重开监听。 */
      host_beacon_tx_done = 0U;
      host_query_reply_pending = 0U;
      if(host_issue_command(HOST_CMD_BEACON_CLOSE) == 0U) {
        host_beacon_link_open = 0U;
        host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
      }
    } else if(0 && host_beacon_link_open == 0U && host_beacon_deadline_ms != 0U &&
              host_time_reached(host_beacon_deadline_ms) != 0U) {
      /* UDP 信标监听已禁用（零 UDP）：MUX=0 单连接下 UDP 监听(CIPSTART=4
       * UDP 5002)占用唯一连接槽，导致 TCP CIPSTART 全部 busy p... 失败
       * （实测：settings 手动测试无 UDP 监听故成功）。 */
      if(host_issue_command(HOST_CMD_BEACON_START) == 0U) {
        host_beacon_deadline_ms = tester_wifi_print_now_ms() + HOST_WIFI_BEACON_PERIOD_MS;
      }
    }
  }
  host_tx_service();
}

uint8_t tester_host_wifi_is_online(void)
{
  return (host_state == TESTER_HOST_WIFI_ONLINE) ? 1U : 0U;
}

uint8_t tester_host_wifi_is_error(void)
{
  /* 首次 ONLINE 之前不报错：上电自连过程中（可能重试数次）不显示
   * NETWORK ERROR，等自连完成或确认失败后再报。 */
  return (host_state == TESTER_HOST_WIFI_ERROR && host_ever_online != 0U) ? 1U : 0U;
}

const char *tester_host_wifi_status_text(void)
{
  return host_status;
}

const char *tester_host_wifi_ip_text(void)
{
  return host_ip;
}

const char *tester_host_wifi_mac_text(void)
{
  return host_mac;
}

uint8_t tester_host_wifi_poll_request(tester_host_wifi_request_t *out_request)
{
  if(out_request == 0 || host_request_tail == host_request_head) {
    return 0U;
  }
  *out_request = host_request_queue[host_request_tail];
  host_request_tail = (uint8_t)((host_request_tail + 1U) % HOST_WIFI_QUEUE_DEPTH);
  return 1U;
}

static uint8_t host_send_result(const tester_host_wifi_request_t *request,
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
    g_tester_host_wifi_done_count++;
  } else if(strcmp(prefix, "ERROR") == 0) {
    g_tester_host_wifi_error_count++;
  }
  return queued;
}

uint8_t tester_host_wifi_send_printing(const tester_host_wifi_request_t *request)
{
  return host_send_result(request, "PRINTING");
}

uint8_t tester_host_wifi_send_done(const tester_host_wifi_request_t *request)
{
  return host_send_result(request, "DONE");
}

uint8_t tester_host_wifi_send_error(const tester_host_wifi_request_t *request)
{
  return host_send_result(request, "ERROR");
}
