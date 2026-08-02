# LCDM 测试机 WiFi 打印生产协议

本文定义最终高档测试机的打印网络链路。生产目标固定为：

```text
IO_APP_MODE=FIRST_GEN_4051_LOCAL
FIRST_GEN_DISPLAY_BACKEND=LCDM
```

`WIFI_LINK_DIAG` 和 `WIFI_NET_DIAG` 只保留为独立硬件/ESP-AT 诊断固件；它们不启动
4051 测试、Hall 触发或生产打印状态机，绝不能作为量产烧录目标。

## 物理连接

| 方向 | AT32F455 | ESP32-C3-WROOM-02U |
|---|---|---|
| 测试机发送 | `PC3 / WIFI_TX` | `RXD0 / Pin 11` |
| 测试机接收 | `PB9 / WIFI_RX` | `TXD0 / Pin 12` |

串口固定为 `115200 8N1`。AT32 使用 GPIO 软件 UART；PB9 双边沿 EXINT 只缓存 UART
边沿，ESP-AT、TCP 和打印业务状态机均在主循环中推进，不阻塞 4051 扫描。

## 每台机保存的配置

配置保存在 AT32 双副本 Flash，不能来自 `WIFI_NET_DIAG_*` 编译参数：

| 字段 | 用途 |
|---|---|
| AT32 UID | 只读硬件唯一身份；随主板更换，LCDM 不能改 |
| `machine_id` / `line_id` / `station_id` | 机身、产线、工位身份；例如 `IOB-03` / `L01` / `ST03` |
| WiFi SSID / 密码 | 本测试机要加入的生产 AP |
| `print_host` | 本线打印主机的 IPv4 或 DNS 名，例如 `192.168.1.20` 或 `print-l01.local` |
| `print_port` | 打印主机的 TCP 服务端口；不是打印机原始 9100 端口，除非该端口正是实现本协议的主机服务 |

`print_host` 与 `print_port` 必须同时填写；主机地址只允许常规 IPv4/DNS 名称。生产打印
还要求 `station_id` 为 `ST01`...`ST255` 或等价正整数，避免所有测试机误用工位 1。

## ESP-AT 生产会话

启动或 K3 软件重试后，正式状态机后台执行：

```text
AT
AT+CIPCLOSE                 # 无旧连接时 ERROR 也视为正常
AT+CWMODE=1
AT+CWJAP="<saved ssid>","<saved password>"
AT+CIPMUX=0
AT+CIPMODE=0
AT+CIPSTART="TCP","<print_host>",<print_port>
```

收到 `CONNECT`/`OK` 后才进入在线状态。`WIFI DISCONNECT`、`CLOSED`、AT 命令超时、
`SEND FAIL` 或发送结果超时都会在后台重新入网/建连；扫描流程继续运行，但 Hall 触发后的
打印任务会等待网络闭环。

## TCP 数据封装

TCP 是单连接、非透传模式。每个业务包是 **一行 UTF-8 JSON，以 LF (`0x0A`) 结束**。
AT32 不把 JSON 直接当作 ESP-AT 命令，而是严格经过：

```text
AT+CIPSEND=<payload byte count>
>
<JSON>\n
SEND OK
```

ESP-AT 接收的主机数据会带 `+IPD,<length>:` 前缀；固件去掉该前缀后再解析 JSON。
打印主机发送的单条 ACK/DONE/ERROR 帧应不超过 200 字节，并必须在同一 LF 前完成。

## 测试机发送

PASS 后，等待 `PB8 / HALL IN` 低电平 20 ms。触发后 LCDM 显示 `START PRINTING`，并产生
一次唯一的 `event_id`。实际传输包包括不可伪造的 MCU UID，示例：

```json
{"type":"print_request","ver":1,"event_id":27,"device_uid":"00112233445566778899AABB","machine_id":"IOB-03","line_id":"L01","station_id":"ST03","station":3,"test_count":27,"pairs":94,"points":188}
```

`event_id` 在断线重连或 ACK/DONE 超时后保持不变。固件最多以同一 ID 重发 3 次；仍失败时
LCDM 显示红色 `NETWORK ERROR / K3 RESET / RETRY PRINT`，当前产品保持锁定，不会误开始下一件。

## 打印主机返回

打印主机必须按 `(device_uid, event_id)` 去重：若同一请求再次到达，不能再次创建标签，而是
返回已存在任务的当前状态。建议顺序：

```json
{"type":"print_ack","event_id":27,"state":"QUEUED"}
```

```json
{"type":"print_status","event_id":27,"state":"DONE"}
```

打印机故障或主机拒绝时：

```json
{"type":"print_status","event_id":27,"state":"ERROR"}
```

联调时也兼容紧凑文本帧：`ACK,27,QUEUED`、`DONE,27`、`ERROR,27`；它们同样必须以 LF
结束并通过 TCP 发送。

## 完整测试机动作

```text
PASS
  -> 等待 Hall(PB8)
  -> START PRINTING
  -> ESP-AT TCP CIPSEND(print_request)
  -> print_ack QUEUED
  -> print_status DONE
  -> COMPLETE
  -> 连续完整扫描 94 × 94，确认所有 OUT/IN 都开路
  -> 等待新线束，自动开始下一件
```

在 `START PRINTING`、`COMPLETE` 和拆线确认期间，测试机不把产品取下过程误判成 NG。只有
`DONE` 后且全矩阵开路确认成功，才允许下一件自动开始。

## LCDM 设置和验收

K3 在 `READY`/安全复位待机按住达到约 3 秒即进入设置页，无需等待松手。K2
`NETWORK TEST` 依次检查 ESP-AT、AP、STA IP 和 STA MAC；若填写了打印主机地址/端口，还会执行一次 `AT+CIPSTART`，因此 K2 的 PASS 是
“可入网且可到达打印主机”，而不是仅取得 WiFi IP。K4 保存后生产状态机从 Flash 重启连接。

详细的屏幕字段、Flash 回滚和输入协议见
[`lcdm_tester_wifi_settings.md`](lcdm_tester_wifi_settings.md)。
