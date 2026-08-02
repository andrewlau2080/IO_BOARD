# LCDM 测试机 WiFi 打印协议

本协议仅用于 LCDM 高端测试机与 ESP32-C3 网络协处理器之间的 `PC3/PB9` 本地串口链路。它与旧机 `PB6/PB7` 红外收发完全独立，不能互相调用或复用状态机。

## 物理连接

| 方向 | AT32F455 | ESP32-C3-WROOM-02U |
|---|---|---|
| 测试机发送 | `PC3 / WIFI_TX` | `RXD0 / Pin 11` |
| 测试机接收 | `PB9 / WIFI_RX` | `TXD0 / Pin 12` |

串口基准为 `115200 8N1`，使用 LF (`0x0A`) 结束一帧。正式测试机现在与 ESP32-C3 ESP-AT 统一使用已验证的内部 HICK PLL 192 MHz 软件 UART；ESP32-C3 固件必须设为相同波特率。诊断构建会固定为 `115200`，其它本地构建可通过 CMake 的 `TESTER_WIFI_PRINT_UART_BAUDRATE` 复核。

AT32 用 GPIO 软件 UART；PB9 的双边沿 EXINT 只负责缓存 UART 边沿，解析和业务状态机仍在主循环完成，避免网络等待阻塞 4051 扫描。

## 测试机发送

Hall 有效且 PASS 后，测试机只发送一次：

```json
{"type":"print_request","event_id":27,"station":1,"test_count":27,"pairs":94,"points":188}
```

`event_id` 是本次打印任务的唯一流水号。ESP32 必须原样带回它，以免延迟的旧消息影响下一件产品。

## 打印控制器返回

控制器已经接收并可排队打印时返回：

```json
{"type":"print_ack","event_id":27,"state":"QUEUED"}
```

真正完成标签输出时返回：

```json
{"type":"print_status","event_id":27,"state":"DONE"}
```

若打印机故障或队列拒绝，返回：

```json
{"type":"print_status","event_id":27,"state":"ERROR"}
```

为方便产线联调，固件也接受紧凑调试帧：`ACK,27,QUEUED`、`DONE,27`、`ERROR,27`。

## LCDM 测试机流程

```text
PASS
  -> 等待 PB8 / HALL IN 低电平（20 ms 去抖）
  -> 显示 START PRINTING，发送 print_request
  -> 收到 print_ack QUEUED：锁定本次测试，等待打印完成
  -> 收到 print_status DONE：显示 COMPLETE
  -> 连续完整扫描 94 × 94，确认所有 OUT/IN 都开路
  -> 等待新线束；检测到学习线序后自动进入 AUTO TEST
```

在 `START PRINTING`、`COMPLETE` 和拆线确认期间，测试机不再把产品被取下的过程误判为 NG。只有 `DONE` 后，并且全矩阵都确认开路，才会允许下一件产品自动开始。
