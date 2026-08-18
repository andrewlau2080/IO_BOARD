#ifndef WIFI_RELIABILITY_TEST_H
#define WIFI_RELIABILITY_TEST_H

#include <stdint.h>

/*
 * WIFI_RELIABILITY_TEST: 双向通信可靠性验证固件（2026-08-18 晚）。
 *
 * 基于 DeepSeek printer_network 方案（ESP-IDF v5.4）适配到 AT32F455 +
 * ESP32-C3 ESP-AT 的独立验证工程：
 *   - P/T 双角色（编译宏 WIFI_RELIABILITY_ROLE=1 为 P 服务器，0 为 T 客户端）
 *   - P/T 都以 Station 模式连路由器；P=CIPSERVER(8888)，T=CIPSTART
 *   - 文本帧协议保留 DeepSeek 全部字段：type/device_id/seq_num/len/checksum/payload
 *   - 可靠机制：seq + ACK/NACK + 超时重传(3次) + 心跳(5s) + 指数退避重连
 *   - 压力测试：T 循环发 N 帧，OK/NG/重发/重连计数 LCDM 实时显示
 *   - 复用现有 LCDM 驱动（first_gen_display）+ tester_wifi_print 原始 AT 层
 *
 * 验证通过后再移植回正式产品固件。
 */

void wifi_reliability_test_init(void);
void wifi_reliability_test_service(void);

#endif
