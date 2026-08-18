# WiFi 双向通信可靠性验证记录（2026-08-19）

> **标注：WIFI测试OK专用** —— 本验证工程及其中修复的根因，
> 将来用于之前未通过的程序中调用（原固件 WiFi 打印链路问题，
> 可参照本工程的协议/解析/恢复机制移植修复）。

## 一、验证工程：WIFI_RELIABILITY_TEST（IO_APP_MODE=14）

独立验证工程，不修改原固件诊断对讲路径，用于评估 AT32+ESP-AT
WiFi 双向通信的可靠性与自愈能力。

### 角色与网络（编译宏固定，不依赖板上 Flash）

| 角色 | 板 | DAP UID | 静态 IP | 职责 |
|---|---|---|---|---|
| P（打印侧） | 807 | 08703D0500C0643C1014A807 | 192.168.1.100 | TCP 服务器（CIPSERVER 8888）+ 每 5s 发心跳帧 |
| T（测试机侧） | 407 | 913575030040A0401D149407 | 192.168.1.101 | TCP 客户端（CIPSTART）+ 无限压测发帧 |

- SSID：TP-LINK_56C928（宏 REL_AP_SSID）
- 网关：192.168.1.1、端口 8888（REL_PORT）
- 两侧规则完全一致：CIPMUX=1 + link 0（一对一也用 CIPMUX=1）
- 静态 IP 必须先于连 AP 设置（CIPSTA → CWJAP → CIFSR），否则 IP EMPTY

### 协议（文本帧，保留 DeepSeek 方案全部字段）

```
发送帧: F,<type>,<device_id>,<seq>,<len>,<xor>,<payload>
ACK:    A,<seq>,<err>
消息类型: 0x01 打印开启 / 0x05 测试结果 / 0x06 心跳 / 0x07 心跳应答 ...
```

- ACK 超时 5s、重传 3 次、心跳周期 5s
- 收帧解析：行首分派（A=ACK / F=数据帧），XOR 校验

### 屏显布局（两侧逐字一致，用户定稿）

```
第一行: TX<次数>OK<次数>NG<次数>, RX<次数>OK<次数>NG<次数>
第二行: TX <实际发送帧内容>
第三行: RX <实际接收帧内容>
```

对向一致：T 的 TX 内容 = P 的 RX 内容；P 的 TX 内容 = T 的 RX 内容。

## 二、验证结果（一对一，板上实测）

- 双向闭环：T 发帧 → P 收（RX OK）→ P 回 ACK → T 收 ACK（OK）→
  P 每 5s 主动发心跳 → T 显示接收 —— 双向对等
- 实测计数（无限压测运行中）：T TX=46 OK=27 NG=0；P TX=68 OK=27 NG=0
  —— **NG=0 零失败**
- T 连接稳定（rel_reconnect_count=0），seq 递增正常
- 无限压测：无停止数（用户要求一直测看数据）

## 三、已修复的根因（本工程挖出，全部验证）

1. **at_begin 模式 CIPSEND ">" 提示符被吞**
   tester_wifi_print.c:1188 只在非 capture 模式立即处理 ">"，
   at_begin（原始行）模式下 ">" 无 \r\n 被行缓冲卡死 →
   CIPSEND 数据永远发不出（"手工连 HOST 可以、一发送就断"的元凶）。
   修复：capture 模式下 ">" 立即入队。

2. **P 帧解析逗号跳转差一个**
   strchr(line+2, ',') 跳过 type 后的分隔逗号 → payload 前的
   最后一个逗号找不到 → 解析永远失败（P 收到 +IPD 但 rx=0）。
   修复：strchr(line+1, ',')。

3. **ACK 被 F 帧检查挡掉**
   rel_handle_ipd 先调 rel_parse_frame（F 帧检查），"A,1,0" 不是
   F 帧直接 return → T 收到 ACK 但永远不解析（ok 恒 0）。
   修复：ACK 解析移到函数开头（行首分派）。

4. **主栈 1KB 溢出 HardFault（STKOF）**
   _Min_Stack_Size=0x400，WiFi RX 边沿中断嵌套在 LCDM 长串口传输
   内 → MSP 栈溢出（CFSR=STKOF，HardFault_Handler 死循环）。
   修复：栈 0x400 → 0x2000（8KB）。

5. **T 收到 ACK 不切回 ONLINE（seq 恒 1）**
   ACK 匹配后状态仍停在 WAIT_ACK → 永远重发同一帧 seq=1。
   修复：收到匹配 ACK → 回 ONLINE 发下一帧（seq 递增）。

6. **P 回 ACK 不走 CIPSEND**
   P 直接 at_send_bytes 发 ACK（ESP 不当发送数据处理）。
   修复：标准 CIPSEND 流程（AT+CIPSEND=<link>,<len> → ">" → 数据）。

7. **两侧规则不一致**
   T 用 CIPMUX=0、P 用 CIPMUX=1 → 两侧统一 CIPMUX=1 + link 0。

## 四、EN 复位纪律（用户原则）

- EN 复位（PA8）只在**上电 init 用一次**（清 ESP 旧状态）
- 运行中不复位 ESP（"如果常用就是程序没写好"）
- 故障恢复：快速重连（CIPSTART 重发），不重启 ESP

## 五、下次任务（用户点名）

- **P 改多连接后是否稳定**：P 的 CIPSERVER+CIPMUX=1 已具备多连接
  能力（当前只连 1 个 T）；下次用多个 T 客户端同时连 P 的 8888
  压测，验证多连接下 P 收/回 ACK 稳定性、连接表残留/踢除问题。

## 六、构建方式

```sh
# P 角色（服务器）
cmake -S . -B build-rel-p -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=WIFI_RELIABILITY_TEST -DWIFI_RELIABILITY_ROLE=1
cmake --build build-rel-p -j 1

# T 角色（客户端）
cmake -S . -B build-rel-t -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=WIFI_RELIABILITY_TEST -DWIFI_RELIABILITY_ROLE=0
cmake --build build-rel-t -j 1
```

烧录（Ubuntu 侧 pyocd，--uid 严格对应 P/T）：

```sh
pyocd flash --uid <UID> -t at32f455vet7 \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  -f 1000000 --erase chip -O connect_mode=under-reset build-rel-*/io_board_at32f455.bin
```

---

记录时间：2026-08-19 晚
验证人：刘（用户）+ Hermes Agent
