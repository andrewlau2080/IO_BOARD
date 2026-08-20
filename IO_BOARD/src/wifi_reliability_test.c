#include "wifi_reliability_test.h"

#include "at32f45x_board.h"
#include "at32f45x_clock.h"
#include "device_config.h"
#include "first_gen_display.h"
#include "io_board.h"
#include "tester_wifi_link_diag.h"
#include "tester_wifi_print.h"

#include <stdio.h>
#include <string.h>

/* 角色：1=P（TCP 服务器），0=T（TCP 客户端）。CMake 编译宏注入。 */
#ifndef WIFI_RELIABILITY_ROLE
#define WIFI_RELIABILITY_ROLE 0
#endif

/* ---- 时序参数（全部非阻塞轮询，无 SLEEP） ---- */
#define REL_STEP_MS              1U
#define REL_AT_TIMEOUT_MS        5000U
#define REL_JOIN_TIMEOUT_MS      25000U
#define REL_ACK_TIMEOUT_MS       5000U
#define REL_MAX_RETRY            3U
#define REL_HEARTBEAT_PERIOD_MS  5000U
#define REL_PORT                 8888U
#define REL_CMD_MAX              128U
#define REL_LINE_MAX             96U
#define REL_TEST_TARGET          1000U   /* 千次压力测试目标 */

/* ---- 网络配置（用户指定，2026-08-18 晚） ---- */
#define REL_AP_SSID              "TP-LINK_56C928"
#define REL_AP_PASSWORD          "1234567890"
#define REL_GATEWAY              "192.168.1.1"
#define REL_NETMASK              "255.255.255.0"
#define REL_HOST                 "192.168.1.100"   /* T 连接目标 = P 静态 IP */
#if WIFI_RELIABILITY_ROLE
#define REL_STATIC_IP            "192.168.1.100"   /* P 端静态 IP */
#else
#define REL_STATIC_IP            "192.168.1.101"   /* T 端静态 IP */
#endif

/* ---- 消息类型（DeepSeek protocol.h 原值） ---- */
#define REL_TYPE_PRINT_START      0x01
#define REL_TYPE_PRINT_PROGRESS   0x02
#define REL_TYPE_PRINT_COMPLETE   0x03
#define REL_TYPE_PRINT_ERROR      0x04
#define REL_TYPE_TEST_RESULT      0x05
#define REL_TYPE_HEARTBEAT        0x06
#define REL_TYPE_HEARTBEAT_ACK    0x07
#define REL_TYPE_STATUS_QUERY     0x08
#define REL_TYPE_STATUS_RESPONSE  0x09

/* ---- 状态机 ---- */
typedef enum {
  REL_STATE_INIT = 0,
  REL_STATE_INIT_WAIT,
  REL_STATE_AT,
  REL_STATE_ECHO,
  REL_STATE_MODE,
  REL_STATE_JOIN_AP,
  REL_STATE_IP_SET,
  REL_STATE_IP,
  REL_STATE_MUX,
#if WIFI_RELIABILITY_ROLE
  REL_STATE_SERVER,
  REL_STATE_LISTEN,
  REL_STATE_SRV_SEND,
#else
  REL_STATE_CONNECT,
  REL_STATE_ONLINE,        /* T：发 PRINT RQ */
  REL_STATE_SEND,          /* T：等 CIPSEND ">" */
  REL_STATE_WAIT_PRINTING, /* T：等 P 的 PRINTING */
  REL_STATE_WAIT_COMPLETE, /* T：等 P 的 PRINT COMPLETE */
#endif
  REL_STATE_BACKOFF,
  REL_STATE_FAIL
} rel_state_t;

/* ---- 状态变量 ---- */
static rel_state_t rel_state;
static uint32_t rel_state_deadline_ms;
static uint32_t rel_wait_ms;
static uint32_t rel_seq;
static uint8_t rel_retry_left;
static uint8_t rel_heartbeat_due;
static uint32_t rel_last_heartbeat_ms;
static uint8_t rel_station_connected;
static uint8_t rel_got_ip;
static char rel_ip[24];
static uint32_t rel_listen_idle_ms;
static uint32_t rel_last_rx_ms;
static uint8_t rel_fail_count;

/* ---- 可靠性计数（LCDM 实时显示 + 评估依据） ---- */
static uint32_t rel_tx_count;
static uint32_t rel_rx_count;
static uint32_t rel_ok_count;      /* TX 确认成功（收到匹配 ACK） */
static uint32_t rel_ng_count;      /* TX 失败（ACK 超时/不匹配） */
static uint32_t rel_rx_ok_count;   /* RX 解析成功帧数 */
static uint32_t rel_rx_ng_count;   /* RX 解析失败帧数 */
static uint32_t rel_retry_count;
static uint32_t rel_reconnect_count;
static uint32_t rel_heartbeat_count;
static uint32_t rel_test_frames_sent;

/* ---- 帧构造/解析缓冲 ---- */
static char rel_tx_frame[REL_LINE_MAX];
static char rel_rx_payload[64];
static char rel_srv_ack[REL_LINE_MAX];
static uint8_t rel_srv_link;
/* ---- 消息（用户指定闭环内容） ---- */
static char rel_msg_tx[32];    /* T：当前发送消息（PRINT RQ） */
static char rel_msg_rx[32];    /* T：当前收到消息（显示用） */

/* ---- LCDM 显示：最后发送/最后解析的内容（用户核对） ---- */
static char rel_last_tx_disp[44];
static char rel_last_rx_disp[44];
/* P 端主动发送（换着收发）：心跳帧序号 + 定时 */
static uint32_t rel_srv_seq;
static uint32_t rel_srv_timer_ms;

/* ---- LCDM 上次显示内容（防闪烁） ---- */
static uint32_t rel_draw_tick_ms;

/* ---- 简单 XOR 校验（帧内 checksum 字段，hex 2 字符） ---- */
static uint8_t rel_payload_xor(const char *payload)
{
  uint8_t sum = 0U;

  if(payload != 0) {
    while(*payload != '\0') {
      sum ^= (uint8_t)*payload;
      payload++;
    }
  }
  return sum;
}

/* ---- 构造数据帧：F,type,dev,seq,len,xor,payload ---- */
static uint16_t rel_build_frame(char *out, uint16_t out_size,
                                uint8_t type, uint8_t device_id,
                                uint32_t seq, const char *payload)
{
  int written;
  uint16_t len;

  if(payload == 0) {
    payload = "";
  }
  len = (uint16_t)strlen(payload);
  written = snprintf(out, out_size, "F,%u,%u,%lu,%u,%02X,%s",
                     (unsigned int)type,
                     (unsigned int)device_id,
                     (unsigned long)seq,
                     (unsigned int)len,
                     (unsigned int)rel_payload_xor(payload),
                     payload);
  if(written <= 0 || (uint16_t)written >= out_size) {
    return 0U;
  }
  return (uint16_t)written;
}

/* ---- 解析数据帧：F,type,dev,seq,len,xor,payload ---- */
static uint8_t rel_parse_frame(const char *line,
                               uint8_t *type,
                               uint8_t *device_id,
                               uint32_t *seq,
                               char *payload, uint16_t payload_size)
{
  unsigned int type_v;
  unsigned int dev_v;
  unsigned long seq_v;
  unsigned int len_v;
  unsigned int xor_v;
  const char *cursor;

  if(line == 0 || line[0] != 'F') {
    return 0U;
  }
  if(sscanf(line, "F,%u,%u,%lu,%u,%x",
            &type_v, &dev_v, &seq_v, &len_v, &xor_v) != 5) {
    return 0U;
  }
  /* 从 'F' 后开始：跳过 F 和逗号 #1（type 后的分隔），共跳 6 个逗号
   * 到达 payload 前（F,type,dev,seq,len,xor,payload） */
  cursor = strchr(line + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor = strchr(cursor + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor = strchr(cursor + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor = strchr(cursor + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor = strchr(cursor + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor = strchr(cursor + 1, ',');
  if(cursor == 0) {
    return 0U;
  }
  cursor++;
  if(payload != 0 && payload_size != 0U) {
    (void)snprintf(payload, payload_size, "%s", cursor);
  }
  /* 校验 payload XOR */
  if(payload != 0) {
    if((unsigned int)rel_payload_xor(payload) != xor_v) {
      return 0U;
    }
  }
  *type = (uint8_t)type_v;
  *device_id = (uint8_t)dev_v;
  *seq = (uint32_t)seq_v;
  return 1U;
}

/* ---- LCDM 绘制（防闪烁：内容变化才重绘） ---- */
static void rel_draw(const char *status,
                     const char *main_text,
                     const char *result,
                     const char *detail,
                     uint16_t color)
{
  first_gen_display_show_page("REL-TEST", status, main_text, result, detail,
                              color, FIRST_GEN_DISPLAY_COLOR_WHITE, color);
}

static void rel_draw_stats(void)
{
  char status[40];
  char main_text[40];
  char result[40];
  char detail[40];
  static uint32_t last_snap[6];
  static char last_tx_show[44];
  static char last_rx_show[44];
  uint32_t snap[6];
  const char *tx_disp = (rel_last_tx_disp[0] != '\0') ? rel_last_tx_disp : "--";
  const char *rx_disp = (rel_last_rx_disp[0] != '\0') ? rel_last_rx_disp : "--";

  /* 用户指定布局（两侧完全一致）：
   *   第一行: Test Qty: <OK> OK; <NG> NG;
   *   第二行: TX <内容>
   *   第三行: RX <内容> */
  (void)snprintf(status, sizeof(status),
                 "Test Qty: %lu OK; %lu NG;",
                 (unsigned long)rel_ok_count,
                 (unsigned long)rel_ng_count);
  (void)snprintf(main_text, sizeof(main_text), "TX %s", tx_disp);
  (void)snprintf(result, sizeof(result), "RX %s", rx_disp);
  (void)snprintf(detail, sizeof(detail), "%s", "");
  snap[0] = rel_tx_count;
  snap[1] = rel_ok_count;
  snap[2] = rel_ng_count;
  snap[3] = rel_rx_count;
  snap[4] = rel_rx_ok_count;
  snap[5] = rel_rx_ng_count;

  /* 防闪：计数和 TX/RX 内容都没变 → 不重绘（减 LCDM 阻塞，ACK 更快）。 */
  if(snap[0] == last_snap[0] && snap[1] == last_snap[1] &&
     snap[2] == last_snap[2] && snap[3] == last_snap[3] &&
     snap[4] == last_snap[4] && snap[5] == last_snap[5] &&
     strcmp(rel_last_tx_disp, last_tx_show) == 0 &&
     strcmp(rel_last_rx_disp, last_rx_show) == 0) {
    return;
  }
  last_snap[0] = snap[0];
  last_snap[1] = snap[1];
  last_snap[2] = snap[2];
  last_snap[3] = snap[3];
  last_snap[4] = snap[4];
  last_snap[5] = snap[5];
  (void)snprintf(last_tx_show, sizeof(last_tx_show), "%s", rel_last_tx_disp);
  (void)snprintf(last_rx_show, sizeof(last_rx_show), "%s", rel_last_rx_disp);
  rel_draw(status, main_text, result, detail,
           (rel_ng_count == 0U) ? FIRST_GEN_DISPLAY_COLOR_GREEN :
           FIRST_GEN_DISPLAY_COLOR_RED);
}

/* 配置总览页：SSID / 本机 IP / 网关 / 端口 / HOST 一屏显示（核对用） */
static void rel_draw_config(void)
{
  char status[40];
  char main_text[40];
  char result[40];
  char detail[40];

  (void)snprintf(status, sizeof(status), "SSID %s", REL_AP_SSID);
  (void)snprintf(main_text, sizeof(main_text), "IP %s", REL_STATIC_IP);
#if WIFI_RELIABILITY_ROLE
  (void)snprintf(result, sizeof(result), "PORT %u (SERVER)", (unsigned int)REL_PORT);
  (void)snprintf(detail, sizeof(detail), "GW %s", REL_GATEWAY);
#else
  (void)snprintf(result, sizeof(result), "PORT %u (TO P)", (unsigned int)REL_PORT);
  (void)snprintf(detail, sizeof(detail), "HOST %s", "192.168.1.100");
#endif
  rel_draw(status, main_text, result, detail, FIRST_GEN_DISPLAY_COLOR_BLUE);
}

static void rel_draw_connecting(const char *step, const char *detail)
{
  rel_draw("CONNECTING", step, "ESP-AT", detail,
           FIRST_GEN_DISPLAY_COLOR_BLUE);
}

/* ---- 推进到下一状态 ---- */
static void rel_issue_state(rel_state_t state)
{
  char command[REL_CMD_MAX];
  uint32_t timeout_ms = REL_AT_TIMEOUT_MS;

  command[0] = '\0';
  switch(state) {
  case REL_STATE_AT:
    (void)snprintf(command, sizeof(command), "AT");
    rel_draw_connecting("AT CHECK", "PC3/PB9 115200 8N1");
    break;
  case REL_STATE_ECHO:
    (void)snprintf(command, sizeof(command), "ATE0");
    rel_draw_connecting("ECHO OFF", "DISABLE AT ECHO");
    break;
  case REL_STATE_MODE:
    (void)snprintf(command, sizeof(command), "AT+CWMODE=1");
    rel_draw_connecting("SET STA", "STATION MODE");
    break;
  case REL_STATE_JOIN_AP:
    /* SSID/密码都用用户指定宏（不依赖板上 Flash 配置） */
    (void)snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"",
                   REL_AP_SSID, REL_AP_PASSWORD);
    timeout_ms = REL_JOIN_TIMEOUT_MS;
    rel_draw_connecting("JOIN AP", REL_AP_SSID);
    break;
  case REL_STATE_IP_SET:
    /* 静态 IP（用户指定：P=192.168.1.100 / T=192.168.1.101） */
    (void)snprintf(command, sizeof(command),
                   "AT+CIPSTA=\"%s\",\"%s\",\"%s\"",
                   REL_STATIC_IP, REL_GATEWAY, REL_NETMASK);
    rel_draw_connecting("SET IP", REL_STATIC_IP);
    break;
  case REL_STATE_IP:
    (void)snprintf(command, sizeof(command), "AT+CIFSR");
    rel_draw_connecting("IP CHECK", "READING STATION IP");
    break;
  case REL_STATE_MUX:
    /* 两侧规则一致：都 CIPMUX=1（一对一也用 link 0） */
    (void)snprintf(command, sizeof(command), "AT+CIPMUX=1");
    rel_draw_connecting("SET MUX", "MULTI CONNECT MODE");
    break;
#if WIFI_RELIABILITY_ROLE
  case REL_STATE_SERVER:
    (void)snprintf(command, sizeof(command), "AT+CIPSERVER=1,%u",
                   (unsigned int)REL_PORT);
    rel_draw_connecting("START SRV", "TCP SERVER 8888");
    break;
#else
  case REL_STATE_CONNECT:
    /* 一对一：link 0 固定（与 P 侧 CIPSEND 规则一致） */
    (void)snprintf(command, sizeof(command),
                   "AT+CIPSTART=0,\"TCP\",\"%s\",%u",
                   REL_HOST, (unsigned int)REL_PORT);
    rel_draw_connecting("TCP CONN", REL_HOST);
    break;
#endif
  default:
    return;
  }

  if(tester_wifi_print_at_send(command) == 0U) {
    rel_state = REL_STATE_FAIL;
    rel_draw("TX NOT READY", "FAIL", "ESP AT BUSY", "CHECK PC3/PB9",
             FIRST_GEN_DISPLAY_COLOR_RED);
    return;
  }
  rel_state = state;
  rel_state_deadline_ms = tester_wifi_print_now_ms() + timeout_ms;
}

/* ---- AT 命令 OK 处理：推进链 ---- */
static void rel_command_ok(void)
{
  switch(rel_state) {
  case REL_STATE_AT:
    rel_issue_state(REL_STATE_ECHO);
    break;
  case REL_STATE_ECHO:
    rel_issue_state(REL_STATE_MODE);
    break;
  case REL_STATE_MODE:
    rel_issue_state(REL_STATE_IP_SET);
    break;
  case REL_STATE_IP_SET:
    rel_issue_state(REL_STATE_JOIN_AP);
    break;
  case REL_STATE_JOIN_AP:
    if(rel_got_ip != 0U) {
      rel_station_connected = 1U;
    }
    rel_issue_state(REL_STATE_IP);
    break;
  case REL_STATE_IP:
    if(rel_ip[0] == '\0') {
      rel_state = REL_STATE_FAIL;
      rel_draw("IP EMPTY", "FAIL", "NO STA IP", "AP DID NOT PROVIDE IP",
               FIRST_GEN_DISPLAY_COLOR_RED);
      return;
    }
    rel_issue_state(REL_STATE_MUX);
    break;
  case REL_STATE_MUX:
#if WIFI_RELIABILITY_ROLE
    rel_issue_state(REL_STATE_SERVER);
#else
    rel_issue_state(REL_STATE_CONNECT);
#endif
    break;
#if WIFI_RELIABILITY_ROLE
  case REL_STATE_SERVER:
    rel_state = REL_STATE_LISTEN;
    rel_state_deadline_ms = 0U;
    rel_last_rx_ms = 0U;
    rel_listen_idle_ms = 0U;
    rel_draw_stats();
    break;
#else
  case REL_STATE_CONNECT:
    rel_state = REL_STATE_ONLINE;
    rel_state_deadline_ms = 0U;
    rel_seq = 0U;
    rel_retry_left = REL_MAX_RETRY;
    rel_last_heartbeat_ms = tester_wifi_print_now_ms();
    rel_draw_stats();
    break;
#endif
  default:
    break;
  }
}

/* ---- 发送消息（T 端 CIPSEND，文本消息） ---- */
#if !WIFI_RELIABILITY_ROLE
static void rel_tx_send_frame(void)
{
  char command[REL_CMD_MAX];
  uint16_t frame_len = (uint16_t)strlen(rel_msg_tx);

  if(frame_len == 0U) {
    return;
  }
  (void)snprintf(command, sizeof(command), "AT+CIPSEND=0,%u",
                 (unsigned int)frame_len);
  if(tester_wifi_print_at_send(command) == 0U) {
    return;
  }
  rel_state = REL_STATE_SEND;
  rel_state_deadline_ms = tester_wifi_print_now_ms() + REL_AT_TIMEOUT_MS;
}

/* 发送 payload（CIPSEND prompt 后）→ 等 P 的 PRINTING */
static void rel_tx_payload(void)
{
  if(tester_wifi_print_at_send_bytes((const uint8_t *)rel_msg_tx,
                                     (uint16_t)strlen(rel_msg_tx)) == 0U) {
    rel_state = REL_STATE_FAIL;
    return;
  }
  rel_tx_count++;
  (void)snprintf(rel_last_tx_disp, sizeof(rel_last_tx_disp), "%s", rel_msg_tx);
  rel_state = REL_STATE_WAIT_PRINTING;
  rel_state_deadline_ms = tester_wifi_print_now_ms() + 10000U;  /* 等 PRINTING 10s */
}
#endif

/* ---- +IPD 消息处理（P/T 按消息内容分派） ---- */
static void rel_handle_ipd(const char *line)
{
#if WIFI_RELIABILITY_ROLE
  /* P 端：收到 T 的 PRINT RQ → 显示 → 发 PRINTING → 3 秒后 PRINT COMPLETE */
  rel_last_rx_ms = tester_wifi_print_now_ms();
  (void)snprintf(rel_last_rx_disp, sizeof(rel_last_rx_disp), "%s", line);
  rel_rx_count++;
  rel_rx_ok_count++;
  if(strncmp(line, "PRINT RQ", 8U) == 0) {
    /* 收到打印请求：回 PRINTING，3 秒后发 PRINT COMPLETE */
    (void)snprintf(rel_srv_ack, sizeof(rel_srv_ack), "PRINTING");
    rel_srv_timer_ms = tester_wifi_print_now_ms() + 3000U;  /* 3 秒后 COMPLETE */
    /* 提取 +IPD,<link>,<len>: 的 link 供 CIPSEND 使用 */
    if(strncmp(line, "PRINT RQ", 8U) == 0) {
      /* link 由 service 的 +IPD 解析段传入（此处 line 是 payload） */
      rel_srv_link = 0U;
    }
    {
      char cipsend_cmd[REL_CMD_MAX];
      (void)snprintf(cipsend_cmd, sizeof(cipsend_cmd),
                     "AT+CIPSEND=%u,%u",
                     (unsigned int)rel_srv_link,
                     (unsigned int)strlen(rel_srv_ack));
      if(tester_wifi_print_at_send(cipsend_cmd) != 0U) {
        rel_state = REL_STATE_SRV_SEND;
        rel_state_deadline_ms = tester_wifi_print_now_ms() + REL_AT_TIMEOUT_MS;
      }
    }
  }
  rel_draw_stats();
#else
  /* T 端：按消息内容分派 */
  (void)snprintf(rel_last_rx_disp, sizeof(rel_last_rx_disp), "%s", line);
  (void)snprintf(rel_msg_rx, sizeof(rel_msg_rx), "%s", line);
  rel_rx_count++;
  rel_rx_ok_count++;
  if(strncmp(line, "PRINTING", 8U) == 0) {
    /* 收到 PRINTING → 等 PRINT COMPLETE（15 秒） */
    rel_state = REL_STATE_WAIT_COMPLETE;
    rel_state_deadline_ms = tester_wifi_print_now_ms() + 15000U;
  } else if(strncmp(line, "PRINT COMPLETE", 14U) == 0) {
    /* 收到 PRINT COMPLETE → 本次闭合完成 = OK 1 次 → 下一轮 */
    rel_ok_count++;
    rel_state = REL_STATE_ONLINE;
    rel_state_deadline_ms = 0U;
  } else {
    rel_ng_count++;   /* 内容不对 → NG */
    rel_state = REL_STATE_ONLINE;
    rel_state_deadline_ms = 0U;
  }
  rel_draw_stats();
#endif
}

/* ---- 初始化 ---- */
void wifi_reliability_test_init(void)
{
  tester_wifi_clock_config();
  delay_init();
  io_board_init();
  first_gen_display_init();
  tester_wifi_print_init();
  tester_wifi_print_at_begin();
  wifi_esp_en_init();
  /* 显式 EN 复位（PA8）：确保 ESP 干净 boot（用户要求自复位，不手工） */
  wifi_esp_en_reset();

  rel_state = REL_STATE_INIT;
  rel_draw_connecting("INIT", "ESP EN RESET");
}

/* ---- 服务循环（非阻塞状态机） ---- */
void wifi_reliability_test_service(void)
{
  char line[REL_LINE_MAX];
  uint32_t now_ms;
  uint32_t elapsed;
  uint32_t next;

  /* 必须维护 tester_wifi_print 时钟与 AT 边缘接收 */
  tester_wifi_print_service();
  now_ms = tester_wifi_print_now_ms();
  elapsed = (uint32_t)rel_wait_ms;

  next = elapsed + REL_STEP_MS;
  rel_wait_ms = (next > 1000U) ? 0U : next;

  /* 轮询 AT 响应行 */
  while(tester_wifi_print_at_poll_line(line, sizeof(line)) != 0U) {
    if(strncmp(line, "+IPD", 4U) == 0) {
      /* 提取 +IPD 后的 payload 行（下一行是数据；这里行内可能带数据） */
      const char *colon = strchr(line, ':');
      if(colon != 0) {
        rel_handle_ipd(colon + 1);
      }
      continue;
    }
    if(strncmp(line, "WIFI ", 5U) == 0) {
      continue;
    }
    if(strncmp(line, "+CIFSR", 6U) == 0) {
      /* 只解析 +CIFSR:STAIP,"192.168.1.100"（STAMAC 行忽略） */
      if(strstr(line, "STAIP") != 0) {
        const char *q1 = strchr(line, '"');
        const char *q2 = (q1 != 0) ? strchr(q1 + 1, '"') : 0;
        if(q1 != 0 && q2 != 0 && (size_t)(q2 - q1 - 1) < sizeof(rel_ip)) {
          (void)snprintf(rel_ip, sizeof(rel_ip), "%.*s",
                         (int)(q2 - q1 - 1), q1 + 1);
        }
      }
      continue;
    }
    if(strcmp(line, "OK") == 0 || strcmp(line, "SEND OK") == 0) {
#if WIFI_RELIABILITY_ROLE
      if(rel_state == REL_STATE_LISTEN) {
        continue;
      }
#endif
      rel_command_ok();
      continue;
    }
    if(strcmp(line, "ERROR") == 0 || strcmp(line, "SEND FAIL") == 0) {
      if(rel_state == REL_STATE_JOIN_AP) {
        /* CWJAP 失败：显示原因（SSID/密码/AP 不可达） */
        rel_state = REL_STATE_FAIL;
        rel_state_deadline_ms = 0U;
        rel_draw("AP JOIN FAIL", "FAIL", "CWJAP ERROR", REL_AP_SSID,
                 FIRST_GEN_DISPLAY_COLOR_RED);
#if !WIFI_RELIABILITY_ROLE
      } else if(rel_state == REL_STATE_SEND) {
        rel_state = REL_STATE_FAIL;
        rel_state_deadline_ms = 0U;
        rel_draw("SEND FAIL", "FAIL", "CIPSEND ERROR", "CHECK LINK",
                 FIRST_GEN_DISPLAY_COLOR_RED);
#endif
      } else {
        rel_command_ok();
      }
      continue;
    }
    if(strncmp(line, "CONNECT", 7U) == 0) {
      rel_command_ok();
      continue;
    }
#if WIFI_RELIABILITY_ROLE
    if(rel_state == REL_STATE_SRV_SEND && strcmp(line, ">") == 0) {
      /* CIPSEND prompt：发 ACK/HEARTBEAT_ACK 数据，回 LISTEN */
      (void)tester_wifi_print_at_send_bytes(
          (const uint8_t *)rel_srv_ack, (uint16_t)strlen(rel_srv_ack));
      (void)snprintf(rel_last_tx_disp, sizeof(rel_last_tx_disp), "%s",
                     rel_srv_ack);
      rel_tx_count++;   /* P 发送计数（与 T 侧 SENT 一致） */
      rel_state = REL_STATE_LISTEN;
      rel_state_deadline_ms = 0U;
      rel_last_rx_ms = tester_wifi_print_now_ms();
      continue;
    }
#else
    if(rel_state == REL_STATE_SEND && strcmp(line, ">") == 0) {
      rel_tx_payload();
      continue;
    }
#endif
    if(strncmp(line, "CLOSED", 6U) == 0) {
      rel_reconnect_count++;
      rel_state = REL_STATE_BACKOFF;
      rel_state_deadline_ms = now_ms + 2000U;
      continue;
    }
  }

  /* 超时检查 */
  if(rel_state_deadline_ms != 0U &&
     tester_wifi_print_now_ms() >= rel_state_deadline_ms) {
    rel_state_deadline_ms = 0U;
    switch(rel_state) {
    case REL_STATE_AT:
      /* ESP-AT boot 时间不确定（5-12s）：AT 超时重发，不 FAIL */
      rel_issue_state(REL_STATE_AT);
      break;
    case REL_STATE_ECHO:
    case REL_STATE_MODE:
    case REL_STATE_JOIN_AP:
    case REL_STATE_IP_SET:
    case REL_STATE_IP:
    case REL_STATE_MUX:
#if WIFI_RELIABILITY_ROLE
    case REL_STATE_SERVER:
#else
    case REL_STATE_CONNECT:
      /* 快速重连：CONNECT 超时直接重发（ESP 保持运行，不重启） */
      rel_reconnect_count++;
      rel_issue_state(REL_STATE_CONNECT);
      break;
#endif
#if WIFI_RELIABILITY_ROLE
    case REL_STATE_SRV_SEND:
      /* CIPSEND 超时：发送失败记 NG，回 LISTEN（下一帧再试） */
      rel_ng_count++;
      rel_draw_stats();
      rel_state = REL_STATE_LISTEN;
      rel_state_deadline_ms = 0U;
      rel_last_rx_ms = tester_wifi_print_now_ms();
      break;
#endif
#if !WIFI_RELIABILITY_ROLE
    case REL_STATE_SEND:
      rel_state = REL_STATE_FAIL;
      rel_draw("TX TIMEOUT", "FAIL", "CIPSEND HANG", "CHECK ESP",
               FIRST_GEN_DISPLAY_COLOR_RED);
      break;
    case REL_STATE_WAIT_PRINTING:
      /* 等 PRINTING 超时（10 秒）→ NG → 重发 PRINT RQ */
      rel_ng_count++;
      rel_draw_stats();
      rel_state = REL_STATE_ONLINE;
      rel_state_deadline_ms = 0U;
      break;
    case REL_STATE_WAIT_COMPLETE:
      /* 等 PRINT COMPLETE 超时（15 秒）→ NG → 重发 PRINT RQ */
      rel_ng_count++;
      rel_draw_stats();
      rel_state = REL_STATE_ONLINE;
      rel_state_deadline_ms = 0U;
      break;
#endif
    default:
      break;
    }
  }

  /* 状态推进 */
  switch(rel_state) {
  case REL_STATE_INIT:
    /* 先显示配置总览 4 秒（SSID/IP/PORT/GW 核对），再等 ESP boot 后开始 */
    if(rel_state_deadline_ms == 0U) {
      rel_draw_config();
      rel_state_deadline_ms = tester_wifi_print_now_ms() + 4000U;
    } else if(tester_wifi_print_now_ms() >= rel_state_deadline_ms) {
      rel_state_deadline_ms = tester_wifi_print_now_ms() + 8000U;
      rel_state = REL_STATE_INIT_WAIT;
    }
    break;
  case REL_STATE_INIT_WAIT:
    /* 等 ESP-AT boot 完成（8 秒，v4.1.1 boot + AT ready 需 5-8s）再开始 */
    if(tester_wifi_print_now_ms() >= rel_state_deadline_ms) {
      rel_state_deadline_ms = 0U;
      rel_issue_state(REL_STATE_AT);
    }
    break;
#if WIFI_RELIABILITY_ROLE
  case REL_STATE_LISTEN:
    /* P 服务器稳定在线。收到 PRINT RQ → 已回 PRINTING（rel_handle_ipd），
     * 3 秒定时到 → 发 PRINT COMPLETE（完成一次打印闭环） */
    if(rel_srv_timer_ms != 0U &&
       tester_wifi_print_now_ms() >= rel_srv_timer_ms) {
      rel_srv_timer_ms = 0U;
      (void)snprintf(rel_srv_ack, sizeof(rel_srv_ack), "PRINT COMPLETE");
      {
        char cipsend_cmd[REL_CMD_MAX];
        (void)snprintf(cipsend_cmd, sizeof(cipsend_cmd),
                       "AT+CIPSEND=%u,%u",
                       (unsigned int)rel_srv_link,
                       (unsigned int)strlen(rel_srv_ack));
        if(tester_wifi_print_at_send(cipsend_cmd) != 0U) {
          rel_state = REL_STATE_SRV_SEND;
          rel_state_deadline_ms = tester_wifi_print_now_ms() + REL_AT_TIMEOUT_MS;
        }
      }
    }
    break;
#endif
#if !WIFI_RELIABILITY_ROLE
  case REL_STATE_ONLINE:
    /* T：发 PRINT RQ（一轮闭合的开始），不停循环 */
    (void)snprintf(rel_msg_tx, sizeof(rel_msg_tx), "PRINT RQ");
    rel_tx_send_frame();
    break;
#endif
  case REL_STATE_BACKOFF:
    if(tester_wifi_print_now_ms() >= rel_state_deadline_ms) {
      rel_state_deadline_ms = 0U;
      rel_issue_state(REL_STATE_AT);
    }
    break;
  case REL_STATE_FAIL:
    /* 纯快速重连（EN 只上电用一次，运行中不复位 ESP——用户原则） */
    if(rel_state_deadline_ms == 0U) {
      rel_state_deadline_ms = tester_wifi_print_now_ms() + 2000U;
    } else if(tester_wifi_print_now_ms() >= rel_state_deadline_ms) {
      rel_draw_connecting("RETRY", "FAST RECONNECT");
      rel_state_deadline_ms = tester_wifi_print_now_ms() + 3000U;
      rel_state = REL_STATE_INIT_WAIT;
    }
    break;
  default:
    break;
  }
}
