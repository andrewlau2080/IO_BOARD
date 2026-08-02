# ESP-AT 网络诊断

`IO_APP_MODE=WIFI_NET_DIAG` 是独立的上板观察固件。它不启动 4051 扫描或生产打印流程，只通过 `PC3/PB9` 验证：

1. `AT` 和 `AT+GMR`；
2. `AT+CWMODE?`、`AT+CWJAP?`；
3. 若构建时给出 SSID，则切换 STA、加入测试 AP，并用 `AT+CIFSR` 读取 IP。

该模式只验证 ESP-AT 网络基础，不代表生产打印已经闭环。正式 `print_request` 由
`FIRST_GEN_4051_LOCAL + LCDM` 固件通过 `AT+CIPSTART` / `AT+CIPSEND` 送往已配置的
打印主机；本诊断模式不会启动该 TCP 状态机或 4051/Hall 流程。详见
[`tester_wifi_print_protocol.md`](tester_wifi_print_protocol.md)。

默认 SSID/密码为空，固件只查询当前状态，不会把凭据写入仓库。带凭据的本地构建示例：

```sh
cmake -S IO_BOARD -B IO_BOARD/build-wifi-net-diag \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=IO_BOARD/cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=WIFI_NET_DIAG \
  -DFIRST_GEN_DISPLAY_BACKEND=LCDM \
  -DFIRST_GEN_DISPLAY_AUTO_DETECT=OFF \
  -DWIFI_NET_DIAG_SSID='你的测试SSID' \
  -DWIFI_NET_DIAG_PASSWORD='你的测试密码'
cmake --build IO_BOARD/build-wifi-net-diag
```

`WIFI_NET_DIAG_PASSWORD` 只进入本地 build 目录的生成头文件；不要提交该 build 目录或把密码写进 `.c/.h/.md`。

上板前保持正常连接：

```text
AT32 PC3 (LQFP100 Pin 18) -> ESP RXD0 / Pin 11
AT32 PB9 (LQFP100 Pin 96) <- ESP TXD0 / Pin 12
ESP EN、IO9/BOOT、IO8 保持高电平；EN/IO9 各有 10 k 上拉
共地，3.3 V 电源具备 WiFi 峰值电流余量
```

不要把 `PC3` 接到 `EN`，也不要把 `PB9` 接到 `IO9/BOOT`。用 CH340 观察时，断开 CH340 的 TX，避免它与 AT32 同时驱动 ESP RXD；正常测试时建议暂时断开 CH340，只保留 AT32–ESP 两线。
