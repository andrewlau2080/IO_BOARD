# 打印终端与 LCDM 输入模块规划

本文记录打印终端模块的当前实现方向。三大模块保持独立：

| 模块 | 构建模式 | 主要文件 | 说明 |
|---|---|---|---|
| 一代本机测试 | `FIRST_GEN_4051_LOCAL` | `first_gen_4051_scan.c`, `tm1637_display.c` | IO 扫描、学习、测试、LEDM 或 LCDM 显示、打印触发 |
| 打印终端 / LCDM | `PRINT_TERMINAL` | `lcdm_tjc.c`, `print_terminal.c`, `print_driver.c`, `print_job_model.c` | 标签内容输入、预览、打印提交 |
| 树莓派 / RS485 | `RPI_RS485_LEGACY` | `rpi_rs485*.c`, `rpi_protocol.c` | 第二代或上位机通信 |

当前程序把三类模块放在同一个工程内一起编译，可以用 `IO_APP_MODE=UNIFIED` 输出一个统一固件文件。2026-07-09 修订后，统一固件上电不再靠“是否探测到 LCDM”决定打印主机，因为高档测试机也会使用 LCDM。统一固件应先读取本机 Flash 配置中的 `device_role` 和 `display_type`，再初始化对应业务模块。LCDM、LEDM 和 RS485 代码保持独立接口，不在业务逻辑里互相调用。

当前 `UNIFIED` 选择逻辑先由 CMake 参数 `UNIFIED_DEFAULT_PRODUCT_MODE` 决定，可选 `FIRST_GEN_4051_LOCAL`、`PRINT_TERMINAL`、`RPI_RS485_LEGACY`。后续把 `unified_select_product_mode()` 改成 Flash 配置优先：`TESTER_BASIC`、`TESTER_LCDM`、`PRINT_HOST`、`UNCONFIGURED`。烧录文件仍然只有一个。

高档测试机 LCDM 显示程序已按 LEDM 兼容模式开始实现，显示后端通过 `FIRST_GEN_DISPLAY_BACKEND=LEDM/LCDM` 选择。具体 TJC 控件名和按键映射见 `docs/high_end_tester_lcdm_display_plan.md`。

## 上电角色判定

| 步骤 | 动作 | 结果 |
|---:|---|---|
| 1 | 上电后 MCU 做基础时钟、GPIO 安全态初始化 | `PB4` 蜂鸣器默认低电平关闭，`PA9/PA10` 暂不发送打印命令 |
| 2 | 读取本机 Flash 配置 | 取得 `device_role`、`display_type`、CRC 和版本号 |
| 3 | `TESTER_BASIC` | 进入测试机端，显示使用 LEDM/TM1637，`PA9/PA10` 按 LEDM 使用 |
| 4 | `TESTER_LCDM` | 进入测试机端，显示使用 TJC LCDM，`PB3/PB5` 按 LCDM 使用 |
| 5 | `PRINT_HOST` | 进入打印主机端，初始化 LCDM、打印通讯、WiFi/MAS、本地缓存 |
| 6 | 未配置或 CRC 错误 | 进入未配置安全模式，不启动测试、不发送打印命令 |
| 7 | 角色锁定后 | 本次运行不再自动切换；异常只按当前角色报错 |

LEDM/LCDM 探测只用于确认显示外设是否在线，不作为角色唯一依据。测试机模式不能发送任何可能被打印机执行的 ZPL、TSPL、ESC/POS 或其它打印内容；只有 `PRINT_HOST` 才允许启用打印机通讯。

如果 LEDM 使用 TM1637，探测方式是 TM1637 ACK 或读键返回；如果 LEDM 使用串口 LED 模块，探测方式是读取 ID/版本/握手返回。LCDM 高档测试机和打印主机使用同一 `PB3/PB5` 电气接口，界面内容后续分开定义。

## LCDM 方向

MOTOR / Steering Engine 项目已选 TJC 陶晶驰串口 HMI 智能屏，不是 MCU 直接驱动的裸 LCD。打印端 LCDM 采用同一品牌/同一协议方向，当前 `PRINT_TERMINAL` 固定按 TJC/陶晶驰 UART HMI 设计：

- MCU 通过 UART 发送 ASCII 控件命令，结尾为 `FF FF FF`。
- LCDM 屏幕工程负责触摸、键盘、字体和页面控件。
- MCU 只保存打印字段和接收屏幕发回的字段更新。

这种方式支持英文大小写、阿拉伯数字和常用 ASCII 符号，MCU 不需要内置整套字体和软键盘。当前固件接受 `0x20` 到 `0x7E` 的可打印 ASCII 字符；为避免破坏 TJC 的 `obj.txt="..."` 指令，双引号会转成单引号，反斜杠会转成 `/`。

## 当前字段容量

| 字段 | 宏 | 容量 |
|---|---|---:|
| 标题 | `PRINT_FIELD_TITLE_LEN` | 23 字符 + `NUL` |
| 项目 | `PRINT_FIELD_ITEM_LEN` | 23 字符 + `NUL` |
| 内容 | `PRINT_FIELD_CONTENT_LEN` | 47 字符 + `NUL` |
| 条码/编号 | `PRINT_FIELD_CODE_LEN` | 31 字符 + `NUL` |
| 标签文本缓冲 | `PRINT_LABEL_TEXT_MAX` | 192 字节 |

AT32F455 链接脚本为约 510 KB Flash、144 KB RAM。当前这些字段和 LCDM 接收缓冲只有数百字节，容量没有压力。后续如果要保存多套模板或历史记录，应单独规划 Flash 存储区。

## LCDM 到 MCU 的建议协议

屏幕端输入完成后发送 ASCII 包，并以 `FF FF FF` 结束：

| 包内容 | 动作 |
|---|---|
| `title=HARNESS TEST` | 更新标题 |
| `item=MODEL-A` | 更新项目 |
| `content=LEFT DOOR 12P` | 更新内容 |
| `code=A123456789` | 更新条码/编号 |
| `qty=1` | 更新数量 |
| `copies=1` | 更新份数 |
| `pass=1` | 更新测试结果 |
| `preview` | 刷新预览 |
| `print` | 提交打印 |
| `refresh` | 重新下发当前字段 |

也兼容 TJC 常见触摸事件 `65 page component event FF FF FF`。当前临时定义：

| component id | 动作 |
|---:|---|
| 1 | 预览 |
| 2 | 打印 |
| 3 | 恢复默认字段 |

## LCDM 控件命名

当前 MCU 会写入这些控件：

| 控件 | 类型 | 内容 |
|---|---|---|
| `tTitle` | 文本 | 标题 |
| `tItem` | 文本 | 项目 |
| `tContent` | 文本 | 内容 |
| `tCode` | 文本 | 条码/编号 |
| `nQty` | 数字 | 数量 |
| `nCopies` | 数字 | 份数 |
| `tPreview` | 文本 | ASCII 标签预览 |
| `tStatus` | 文本 | `READY` / `PRINT OK` / `PRINT ERROR` |

## 接线与 IO 规格

打印端 LCDM 使用 TJC/陶晶驰 UART 协议，但不强制占用硬件 USART。按 `SCH_FIXTURE_2026-07-07-N.pdf`，当前 TJC LCDM 只使用 `PB3/PB5` 两线软件串口；`PA9/PA10` 只走到一个共用通讯接插件，测试机角色接 LEDM，打印主机角色接打印机串口/RS485 转换器。这样避免 LCDM 占用打印通讯口，也避免误用 `PA2` 或封装未确认的 `PH2`。

| 功能 | 模块侧信号 | AT32 引脚 | 外设 | 参数 | 说明 |
|---|---|---|---|---|---|
| LCDM RX | 接 MCU TX | `PB3` | GPIO 软件 UART TX | 默认 9600 8N1 | TJC/陶晶驰串口 HMI 接收 MCU 控件命令；原裸屏 `LCM_SPI_SCK` 改作串口 TX |
| LCDM TX | 接 MCU RX | `PB5` | GPIO 软件 UART RX | 默认 9600 8N1 | TJC/陶晶驰串口 HMI 回传触摸和字段包；原裸屏 `LCM_SPI_MOSI` 改作串口 RX |
| LCDM VCC | 屏幕电源 | 5V 或模块要求电源 | 电源 | - | 串口电平必须兼容 AT32 3.3V，必要时加电平转换 |
| LCDM GND | GND | GND | 电源 | - | 必须共地 |
| 共用通讯口 TX | MCU -> LEDM/打印机转换器 | `PA9` | GPIO 或 `USART1_TX` / AF7 | 按角色决定 | 普通测试机端可作 LEDM CLK/TX；打印主机端作打印机通讯 TX |
| 共用通讯口 RX | LEDM/打印机转换器 -> MCU | `PA10` | GPIO 或 `USART1_RX` / AF7 | 按角色决定 | 普通测试机端可作 LEDM DIO/RX；打印主机端作打印机通讯 RX |
| 2 kHz 蜂鸣器 | 蜂鸣器驱动 | `PB4` | GPIO 输出 | 高电平有效 | PASS 1s 响 1s 停；NG 0.5s 响两下，中间 0.5s，末尾停 1s |
| RS485 DE/RE | 方向控制 | 待核定 | GPIO | 可选 | `PA1` 已为 `DEBUG_TTL_RX`，不能作为方向脚；需要半双工时重新分配 |
| WiFi/无线模块 TX | MCU 发给 ESP32 | `PC3` | GPIO 软件 UART TX | 第一版建议 115200 8N1 | 07-07-N 已确认为 `WIFI_TX`，接 ESP32-C3-WROOM-02U `RXD` / Pin 11 |
| WiFi/无线模块 RX | ESP32 发给 MCU | `PB9` | GPIO 软件 UART RX | 第一版建议 115200 8N1 | 07-07-N 已确认为 `WIFI_RX`，接 ESP32-C3-WROOM-02U `TXD` / Pin 12 |
| WiFi EN | ESP32 复位/使能 | 第一版不接 AT32 | 硬件上拉/手动控制 | ESP32 侧 10k 上拉到 3.3V | 接 ESP32 `EN` / Pin 2；留 RESET 测试点/按键拉 GND |
| WiFi BOOT | ESP32 下载模式 | 第一版不接 AT32 | 硬件上拉/手动控制 | ESP32 侧 10k 上拉到 3.3V | 接 ESP32 `IO9` / Pin 8；留 BOOT 测试点/按键拉 GND |

`PB4` 在 2026-07-09 规划改为 `BUZZER_2K`，不再作为 TJC LCDM reset、WiFi 备用或普通备用；`PB6/PB7` 已用于 `IR_TX/IR_RX`，`PB8` 已用于 `HALL_SW`。`PA1=DEBUG_TTL_RX`、`PA3=DEBUG_TTL_TX`，不得再分配给 RS485 方向、WiFi 或其它功能。`PA2` 已固定为扫描输入 `ADC2_IN2`，`PD8` 保留给 `OUT_BMUX_EN0`，二者都不得再分配给 LCDM、打印机通讯或 WiFi/无线模块。`PH2/PH3` 不写入 AT32F455VET7 LQFP100 最终规格，除非重新拿封装 pinout 证明它是可落板脚。`PA9/PA10` 在 07-07-N 图纸中为 `USART_TX/RX`，但 PCB 只保留一个共用通讯接插件；角色由 Flash 配置区分，高档测试机和打印主机都可使用 LCDM。WiFi 模块第一版固定 `PC3=AT32_WIFI_TX`、`PB9=AT32_WIFI_RX`；`WIFI_EN/WIFI_BOOT` 不占 AT32，只做上拉和测试点/按键。

## 斑马打印机 RS485 后端

打印驱动已预留 Zebra/ZPL 直连后端。启用方式：

```sh
cmake -S . -B build-print-zebra -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=PRINT_TERMINAL \
  -DPRINT_DRIVER_BACKEND=ZPL_RS485
```

当前后端行为：

| 项目 | 当前实现 |
|---|---|
| 打印语言 | ZPL |
| 串口 | `USART1` |
| 默认脚位 | `PA9=TX`, `PA10=RX`, AF7 |
| 默认波特率 | `9600`, 8N1 |
| 方向控制 | 默认不驱动 DE/RE；如硬件需要，编译加 `PRINT_RS485_USE_DIR_PIN=1` 并重新指定方向脚；`PA1` 已为 `DEBUG_TTL_RX`，不得使用 |
| 打印提交 | LCDM 发送 `print` 后，MCU 生成 ZPL 并发到 RS485 |

注意：Zebra 打印机本体通常是 USB/以太网/RS232，若使用 RS485，需要打印机侧或中间转换器支持透明串口传输 ZPL。MCU 侧当前只负责把 ZPL 文本送到 RS485，总线应保证只有 MCU 对打印机发送，避免多主冲突。

`PA9/PA10` 是角色复用脚，但 PCB 只放一个共用通讯接插件。普通测试机角色插 LEDM/TM1637；高档测试机使用 `PB3/PB5` LCDM；打印主机角色插打印机串口/RS485 转换器。上电先读本机 Flash 角色配置，不再用 LCDM 是否存在判定打印主机。
