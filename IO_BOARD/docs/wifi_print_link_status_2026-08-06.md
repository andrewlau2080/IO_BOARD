# WiFi 打印链路：状态与关键决策档案（2026-08-06）

> 本文档记录 2026-08-06 双板 WiFi 打印衔接的所有关键决策、机制、验证结果与待办，
> 供后续会话（含 Codex）无缝接手。结论先行，表格优先。
> 关联文档：`tester_wifi_print_protocol.md`（协议）、`print_terminal_lcdm_plan.md`（打印侧 UI 规划）、
> `lcdm_tester_wifi_settings.md`（测试机设置页）。

## 一、当前固件状态（HEAD: `9aa5daa`，2026-08-06 20:26）

| 角色 | 构建目录 | ELF 入口 | 烧录状态 | DAP 唯一ID | 保存配置 |
|---|---|---|---|---|---|
| 测试机 1# | `build-update-tester` | `0x080145bd` | 已烧录（500kHz 全镜像校验 PASS） | 尾号 407 `913575030040A0401D149407` | SSID STDDG 在槽 A/B（0x0807E800/0x0807F000） |
| 打印侧 2# | `build-update-print-host` | `0x0800b7e1` | 已烧录（500kHz 全镜像校验 PASS） | 尾号 807 `08703D0500C0643C1014A807` | 配置 0x0807D800（PST1 魔数） |

当前运行态（2026-08-06 晚实测）：两侧均 ONLINE（测试机 engine=14、打印侧 host_state=5），
reconnect=0、rx_error=0（192MHz 解码干净）。**双 DAP 严禁混淆**（见 AGENTS.md）。

## 二、关键决策与机制（按时间顺序）

### 1. 两侧统一 192MHz 时钟（commit `14ec8bd`）
- **问题**：测试机原 8MHz HICK，ESP 软件 UART 仅 69 cycles/bit，生产会话解码 100% 损坏
  （rx_error 数百、0 完整帧），无法完成 JOIN；打印侧 192MHz（1666 cycles/bit）解码干净。
- **决策**：测试机改用与打印侧相同的 `tester_wifi_clock_config()`（内部 HICK PLL 192MHz）。
- **时钟影响审计**（改前完成，结论：全部随 `system_core_clock` 缩放，安全）：
  - 4051 扫描：`delay_us(IO_SCAN_SETTLE_US)` 走 SysTick（`fac_us = system_core_clock/1e6`）+ GPIO 数字读；ADC 块整体 `#if 0` 未用
  - LCDM（lcdm_tjc）：波特率由 PCLK 推导，打印侧同模块 192MHz 已验证
  - IR/line_comm：`g_cycles_per_us = system_core_clock/1e6`
  - 蜂鸣器：ms 模式 GPIO（有源蜂鸣器）；面板时基：DWT `cycles_per_ms`
  - Flash：`tester_wifi_clock_config()` 内部已设 `FLASH_WAIT_CYCLE_5`
- 注意：42f4db1 曾因"192M 影响扫描"退回 8MHz —— 当时根因实为 ESP 启动流量 + LCDM 绘制的
  ISR 争用（学习表绘制时矩阵显示异常），非时钟本身；192MHz 下 ISR 开销仅 8MHz 的 1/24。

### 2. 上电自动连接（commit `14ec8bd`）
- 移除 `wifi_start_deferred` 延迟机制（42f4db1 引入）：init、K3 短按、K3 无设置权限、
  设置 K3 退出 4 处全部改调 `tester_wifi_print_start()`，上电即自动连接，断网自动重连
  （引擎 BACKOFF→KICK→会话 循环），无需手动触发。行为与打印主机一致。

### 3. 两阶段踢活修复（commit `741429d`，Codex/Bardeen 提交）
- **根因（板上实锤）**：踢活发 `+++` 无 CRLF，命令模式下 ESP-AT 缓存为未完成命令，
  随后 AT 到达合并成 `+++AT` → ERROR → 无限重试。透传模式按字节+保护期退出所以旧测试未暴露。
- **修复**：踢活保持期（1200ms）结束后先发 CRLF 终止挂起行（其 ERROR 由踢活窗口守卫消费），
  200ms 后再发 AT（`TESTER_WIFI_ESP_KICK_TERM_MS`）。两侧同一机制。

### 4. ATE0 关回显 + RX 会话冲刷（commit `9d9d7b8`）
- 8MHz 时 ESP 回显与发送窗口重叠导致边沿丢失 → 残行吞掉下一条响应（K2 需 2 次才过的原因）。
- 生产序列 AT→ATE0→…（`WIFI_ENGINE_SET_ECHO`）；K2 测试首步 ATE0。
- 会话开始与踢活结束时冲刷 `wifi_rx_line_len`/`wifi_rx_edge_tail`。
- 192MHz 后解码已干净，ATE0 作为纵深防御保留。

### 5. 上电 2 秒启动宽限（commit `9aa5daa`）
- **问题**：上电时 ESP-AT 需 ~1.5-2s 启动，立即发起会话会白费一轮 kick/超时/退避（~6s），
  打印侧整体 10s+ 才连上。
- **修复**：`TESTER_WIFI_ESP_BOOT_GRACE_MS` / `HOST_WIFI_ESP_BOOT_GRACE_MS = 2000`。
  测试机：`wifi_boot_grace_pending`（init 置位 → start 保持 → service STOPPED 分支到期开始）。
  打印侧：`host_first_start_pending`（init 置位 → service 宽限到期才调 `print_host_wifi_start`；
  `print_terminal_init` 不再立即 start）。
- **坑（已修）**：打印侧宽限分支必须先跑 `tester_wifi_print_service()` 再查宽限，否则时间基准
  不推进、宽限永不结束。
- 实测：打印侧烧录重启后 6.3s ONLINE（首次会话成功，reconnect=0）。

### 6. 通过态深绿色统一（commit `7722cab`）
- PASS 深绿 = RGB565 `992`（`FIRST_GEN_DISPLAY_COLOR_AUTO_PASS_GREEN` / `LCDM_DARK_GREEN`）。
- 测试机 11 处通过后绿色（PASS 闪烁/文字/填充、WIFI 图标、PRINT COMPLETE、WAITING FOR
  PRINTING、状态栏 PASS/PRINT、表格已通点）全部改用 `LCDM_DARK_GREEN`；
  学习矩阵调色板（1929/1936 两绿为不同分组色）与 IN 标签（2813）保留原色。
- 打印侧：`PRINT_LCDM_GREEN` 常量 2016→992（该侧所有绿色均为成功态）；`lcdm_motor_ui.c`
  PASS 态用 `LCDM_DARK_GREEN`。设置页 K2 本就用 992（`SETTINGS_WIFI_PASS_GREEN`）。

### 7. WIFI 标志移到左上角并垂直居中（commit `9aa5daa` / `626b17d`）
- 测试机：`LCDM_WIFI_X` 394→2（右上角留给 HALL IN 独占）。
- 打印侧：头部左上角新增 WIFI 标志（在线深绿/离线灰 `PRINT_LCDM_GRAY=33808`）。
- 垂直居中：WIFI（两侧）与测试机 HALL IN 盒子改 y=7/h=18（32px 头栏内
  (32-18)/2=7，16px Song 字体完整显示不裁切）。

## 三、烧录与验证方法（本机 macOS）

- **一律 500kHz**（1MHz 本机 DAP 不可靠，两次留下"向量写了、尾部擦空"半成品 → STKERR 硬故障、
  TJC 屏冻旧画面）。烧后**全镜像多点校验（含尾部）**，不能只验向量表。
- 烧录：`/tmp/pyocd-at32-venv/bin/pyocd flash -t at32f455vet7 --pack /tmp/ArteryTek.AT32F45x_DFP.2.0.1.pack -f 500000 --erase sector -O connect_mode=under-reset --uid <UID> <hex>`
- 向量表验证：commander `-c "halt" -c "wr sp 0x20024000" -c "wr pc <入口>" -c "go" -c "read32 0x08000000" -c "read32 0x08000004"`（多命令必须多个 `-c`；`read32` 须带 0x 前缀）。
- 跨构建读 RAM 前**必须重新 nm 当前 ELF**（静态变量移位，旧地址读出垃圾值）。
- 构建：`cmake --build <dir> -- -j 1`（本机进程表满，ninja 并行必挂）。

## 四、已知问题与待办

1. **打印侧 ESP 掉电后沉默（2026-08-06 晚，当前主阻塞）**：
   - 现象：用户断电上电后打印侧无法连接，LCDM 显示 NETWORK ERROR；reconnect=19 反复重试。
   - 证据：`wifi_rx_edge_head/tail` 恒 0（**ESP 的 TXD 一个字节都没发过，连启动日志/ready 都没有**）、
     rx_frame=0、rx_error=0；host 会话在正常监听（capture=1）、TX 侧正常。
   - 对照：测试机 ESP 应答正常（engine=ONLINE、reconnect=0）—— 连线（PC3→RXD/PB9←TXD/EN 10k
     上拉，见 `wifi_module_pcb_connection_plan.md`）与软件（同一引擎、192MHz、2s 宽限、两阶段踢活）
     两侧完全相同。
   - 结论：**非接线/软件差异，是打印侧 ESP 模块断电后没起来**（挂死或固件损坏）。无 EN 复位线，
     kick 对挂死的 ESP 无效，软件无法救。
   - 处理顺序：① 彻底断电 10s 再上电（排除偶发挂死）；② 仍无声 → LIU 法重烧 ESP-AT
     （`LIU_ESP_WIFI_AT_FLASH_METHOD/README.md`，需 CH340 + 临时 AT32 辅助程序）；③ 重烧后仍无声
     → 硬件排查（3V3_WIFI 电源、EN 上拉、模块本体、TXD 走线）。
   - 历史参考：2026-08-06 20:24/20:43 两次 SWD 烧录重启（ESP 未断电）实测 5.7-6.3s ONLINE ——
     同一固件在 ESP 正常时连接无问题。
2. **打印侧设置页 COMM 页 "WIFI LINK" 行**（`print_terminal_settings.c:515`）：用户曾报告标签消失，
   经查该行代码与绘制均正常（设置页每 tick 刷新、缓存机制正确）。用户已决定不再处理
   （"如果没有就不要理会"）。若后续要恢复"打开设置页即显示生产连接状态"：目前
   `settings_wifi_link_ok` 仅由 K2 测试置位（`tester_settings.c:1563`），生产引擎 ONLINE
   不更新该值（设置页打开未跑 K2 时显示 WAITING）；可让 `settings_main_link_text()` 回退到
   `tester_wifi_print_is_online()` 实时状态。
3. **打印侧真实 RS232 打印回报未实现**：`print_driver_is_busy()` 空桩、PB3 RX 无状态行解析、
   service 非模拟分支为占位 —— 等用户确认打印机型号（方向 Zebra/ZPL，`build-print-zebra` 已通）。

## 五、验收闭环（用户验收标准）

测试 PASS → HALL IN（PB8）→ 测试侧发打印请求 → 打印侧回"打印中"→ 模拟 5 秒 →
打印侧回"打印完成" → 测试侧显示完成 → 拆线后自动回自动测试模式 → 打印侧继续等待请求。
SSID/密码随时可改并存 Flash。两侧断网/ESP 卡死自动踢活重连，无需人工断电。
