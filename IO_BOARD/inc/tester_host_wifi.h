#ifndef TESTER_HOST_WIFI_H
#define TESTER_HOST_WIFI_H

#include <stdint.h>

#include "tester_wifi_print.h"

/* WiFi server used by the independent print-controller firmware.  The
 * tester remains a TCP client; this module owns the ESP-AT server socket and
 * translates print requests into a small foreground queue. */
typedef enum {
  TESTER_HOST_WIFI_STOPPED = 0,
  TESTER_HOST_WIFI_CONFIG_REQUIRED,
  TESTER_HOST_WIFI_STARTING,
  TESTER_HOST_WIFI_JOINING,
  TESTER_HOST_WIFI_STARTING_SERVER,
  TESTER_HOST_WIFI_ONLINE,
  TESTER_HOST_WIFI_ERROR
} tester_host_wifi_state_t;

typedef struct {
  uint32_t event_id;
  uint8_t link_id;
  uint8_t station;
  uint16_t quantity;
  uint8_t pass;
  char device_uid[32];
  char title[24];
  char item[24];
  char content[48];
  char code[32];
} tester_host_wifi_request_t;

extern volatile uint32_t g_tester_host_wifi_rx_request_count;
extern volatile uint32_t g_tester_host_wifi_rx_duplicate_count;
extern volatile uint32_t g_tester_host_wifi_rx_error_count;
extern volatile uint32_t g_tester_host_wifi_ack_count;
extern volatile uint32_t g_tester_host_wifi_done_count;
extern volatile uint32_t g_tester_host_wifi_error_count;
extern volatile uint32_t g_tester_host_wifi_reconnect_count;
extern volatile uint8_t g_tester_host_wifi_state;

void tester_host_wifi_init(void);
void tester_host_wifi_start(void);
void tester_host_wifi_restart(void);
void tester_host_wifi_resume(void);
/* 第二阶段：测试机侧 client 发送打印请求（TCP 连打印侧 5006，帧格式
 * 与打印侧 host_receive_request 解析一致）。返回 1=已入发送状态机。 */
uint8_t tester_host_wifi_request_print(uint32_t event_id, uint32_t test_count,
                                       uint16_t pair_count, uint16_t point_count);
/* 轮询打印事件（ACK_QUEUED/PRINTING/DONE/ERROR/NONE——按 event_id 匹配）。 */
tester_wifi_print_event_t tester_host_wifi_print_event(uint32_t event_id);
/* 打印侧已连接（TCP CIPSTART OK 后置位；WiFi 会话重启时清除）——
 * 主屏以此驱动深绿 1 秒闪烁（"已连上打印机"指示）。 */
uint8_t tester_host_wifi_printer_linked(void);
/* 自动发现打印侧（UDP 广播查询带本机 line_id；打印侧 line 匹配才回复）。 */
uint8_t tester_host_wifi_discover_start(void);
uint8_t tester_host_wifi_discovered_count(void);
uint8_t tester_host_wifi_discovered_matched(void);
const char *tester_host_wifi_discovered_ip(void);
uint16_t tester_host_wifi_discovered_port(void);
void tester_host_wifi_service(void);
uint8_t tester_host_wifi_is_online(void);
uint8_t tester_host_wifi_is_error(void);
const char *tester_host_wifi_status_text(void);
const char *tester_host_wifi_ip_text(void);
const char *tester_host_wifi_mac_text(void);
uint8_t tester_host_wifi_poll_request(tester_host_wifi_request_t *out_request);
/* Report that a queued request has entered the printer execution phase. */
uint8_t tester_host_wifi_send_printing(const tester_host_wifi_request_t *request);
uint8_t tester_host_wifi_send_done(const tester_host_wifi_request_t *request);
uint8_t tester_host_wifi_send_error(const tester_host_wifi_request_t *request);

#endif
