# 打印终端与 LCDM 输入模块规划

本文记录打印终端模块的当前实现方向。三大模块保持独立：

| 模块 | 构建模式 | 主要文件 | 说明 |
|---|---|---|---|
| 一代本机测试 | `FIRST_GEN_4051_LOCAL` | `first_gen_4051_scan.c`, `tm1637_display.c` | IO 扫描、学习、测试、LEDM 显示、打印触发 |
| 打印终端 / LCDM | `PRINT_TERMINAL` | `lcdm_tjc.c`, `print_terminal.c`, `print_driver.c`, `print_job_model.c` | 标签内容输入、预览、打印提交 |
| 树莓派 / RS485 | `RPI_RS485_LEGACY` | `rpi_rs485*.c`, `rpi_protocol.c` | 第二代或上位机通信 |

当前程序把三类模块放在同一个工程内一起编译，可以用 `IO_APP_MODE=UNIFIED` 输出一个统一固件文件。统一固件上电后先选择产品类型，再只初始化对应模块；LCDM、LEDM 和 RS485 代码保持独立接口，不在业务逻辑里互相调用。

当前 `UNIFIED` 选择逻辑先由 CMake 参数 `UNIFIED_DEFAULT_PRODUCT_MODE` 决定，可选 `FIRST_GEN_4051_LOCAL`、`PRINT_TERMINAL`、`RPI_RS485_LEGACY`。后续把 `unified_select_product_mode()` 改成通信/配置判定即可，烧录文件仍然只有一个。

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

打印端 LCDM 使用 TJC/陶晶驰 UART 协议，但不强制占用硬件 USART。按 `SCH_FIXTURE_2026-07-05.pdf`，当前 TJC LCDM 只使用 `PB3/PB5` 两线软件串口；打印机通讯仍使用 `PA9/PA10` 的硬件 USART1。这样避免 LCDM 占用 LEDM/打印机共用的 `PA9/PA10`，也避免误用 `PA2` 或封装未确认的 `PH2`。

| 功能 | 模块侧信号 | AT32 引脚 | 外设 | 参数 | 说明 |
|---|---|---|---|---|---|
| LCDM RX | 接 MCU TX | `PB3` | GPIO 软件 UART TX | 默认 9600 8N1 | TJC/陶晶驰串口 HMI 接收 MCU 控件命令；原裸屏 `LCM_SPI_SCK` 改作串口 TX |
| LCDM TX | 接 MCU RX | `PB5` | GPIO 软件 UART RX | 默认 9600 8N1 | TJC/陶晶驰串口 HMI 回传触摸和字段包；原裸屏 `LCM_SPI_MOSI` 改作串口 RX |
| LCDM VCC | 屏幕电源 | 5V 或模块要求电源 | 电源 | - | 串口电平必须兼容 AT32 3.3V，必要时加电平转换 |
| LCDM GND | GND | GND | 电源 | - | 必须共地 |
| 打印机通讯 TX | MCU -> 打印机/RS485 | `PA9` | `USART1_TX` / AF7 | 默认 9600 8N1，可由 LCDM 设置 | `PRINT_TERMINAL` 模式下作为打印机通讯口 |
| 打印机通讯 RX | 打印机/RS485 -> MCU | `PA10` | `USART1_RX` / AF7 | 默认 9600 8N1，可由 LCDM 设置 | 可读打印机/转换器返回状态 |
| RS485 DE/RE | 方向控制 | 默认预留 `PA1` | GPIO | 可选 | 需要半双工收发器时启用 `PRINT_RS485_USE_DIR_PIN=1` |
| WiFi/无线模块 | 未定 | 未分配 | 待定 | 待模块选型后确认 | 当前没有找到合适 WiFi 模块，只预留机械空间、电源余量、0R/测试点，不写死 UART/SPI/SDIO 引脚 |

`PB4` 不写入 2026-07-05 TJC LCDM 当前规格；`PB6/PB7` 已用于 `IR_TX/IR_RX`，`PB8` 已用于 `HALL_SW`。`PA2` 已固定为扫描输入 `ADC2_IN2`，不得再分配给 LCDM、打印机通讯或 WiFi/无线模块。`PH2` 不写入 AT32F455VET7 LQFP100 最终规格，除非重新拿封装 pinout 证明它是可落板脚。`PA9/PA10` 在打印主机角色中给打印机通讯使用，在测试机角色中可给 LEDM/TM1637 使用，两种角色不能同时并接；WiFi 模块也不得默认占用这两个脚，除非后续重新定义产品角色和 BOM。

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
| 方向控制 | 默认不驱动 DE/RE；如硬件需要，编译加 `PRINT_RS485_USE_DIR_PIN=1`，默认方向脚 `PA1` |
| 打印提交 | LCDM 发送 `print` 后，MCU 生成 ZPL 并发到 RS485 |

注意：Zebra 打印机本体通常是 USB/以太网/RS232，若使用 RS485，需要打印机侧或中间转换器支持透明串口传输 ZPL。MCU 侧当前只负责把 ZPL 文本送到 RS485，总线应保证只有 MCU 对打印机发送，避免多主冲突。

`PA9/PA10` 是角色复用脚：扫描/LEDM 测试机角色可把它们接给 LEDM/TM1637；打印主机角色不装 LEDM，把它们恢复为打印机通讯 USART1。两种角色不能在同一硬件/BOM 上同时并接工作。
