# SCH_FIXTURE IO核对与修改建议

最新依据：`IO_BOARD/docs/SCH_FIXTURE_2026-07-05.pdf` 第3页 MCU2。旧 `SCH_FIXTURE_2026-06-30.pdf` 内容只作历史对照。目标 MCU：AT32F455VET7 LQFP100。

## 总体结论

| 模块 | 图纸现状 | 问题 | 建议 |
|---|---|---|---|
| 4051扫描脚 | IN/OUT A/B 地址线和使能线 | 和当前修正后的 `io_board.c` 一致 | 保留 |
| OUT_BMUX_EN | `PD9..PD15, PC6` | 新版图纸明确不是 `PD8..PD15` | 保留当前修正版 |
| SWD烧录 | J4标 `SWCLK/SWDIO`，但 MCU 侧疑似 `PA14/PA15`；`PA13` 被 `DEBUG_TTL_TX` 占用 | 标准 SWD 应使用 `PA13=SWDIO`、`PA14=SWCLK` | 必须改/确认 |
| PA9/PA10 | 07-05 图纸为 `USART_TX/USART_RX`，同时规划 LEDM/TM1637 角色复用 | 使用 LEDM 时会与打印机/主通讯 USART1 冲突 | 按产品角色/BOM 二选一 |
| LCDM接口 | `PB3=LCDM_TX`、`PB5=LCDM_RX` | TJC 串口屏只需 TX/RX；PB4 不再写作 LCDM_RESET | 按 07-05 图纸保留 |
| IR打印 | `PB6=IR_TX`、`PB7=IR_RX` | 旧文档曾写反 PB6/PB7 | 固件与规格表必须按 07-05 图纸更新 |
| 感应触发 | `PB8=HALL_SW` | 旧图曾作 `LCM_BL_LED`，当前新版已改为霍尔/感应输入 | 按 07-05 图纸保留 |

## 给画图人员的修改清单

| 序号 | 修改项 | 建议修改内容 | 优先级 |
|---:|---|---|---|
| 1 | SWD接口 | J4改为：3.3V/Vref、`SWDIO=PA13`、`SWCLK=PA14`、GND，建议加 `NRST` | 必须改 |
| 2 | `DEBUG_TTL_TX` | 不要占用 `PA13`；移到空余脚或删除 | 必须改 |
| 3 | `PA9/PA10` | 只作为 `USART_TX/RX`；删除 TM1637/LEDM 复用说明 | 必须改 |
| 4 | 显示接口 | 按 2026-07-05 当前方案使用 TJC LCDM：`PB3=LCDM_TX`、`PB5=LCDM_RX`；不要再把 `PB4/PB6/PB7/PB8` 写成裸屏接口 | 必须改 |
| 5 | LEDM/TM1637 | 若要两组 LED 模块，新增独立 `J_LEDM`，不占 USART；建议 `CLK/DIO_ERR/DIO_CUR` 三线 | 待决定 |
| 6 | IR打印 | 如果保留红外打印，新增 `J_IR`：`PB6=IR_TX`、`PB7=IR_RX`、3.3V、GND | 待决定 |
| 7 | 固件 | 按最终图纸更新 `io_board.c`、LCDM驱动，禁用或迁移 `tm1637_display.c` 和 `ir_remote.c` | 后续执行 |
| 8 | WiFi/无线模块 | 当前模块未选定，不分配固定 MCU IO；只预留安装空间、电源余量和可选 0R/测试点，待模块确认后再定 UART/SPI/SDIO | 待决定 |

## 修订：感应打印触发输入

`PC7` 是按键 `KEY_RIGHT`，此前只作临时打印触发测试，正式硬件不要再用 PC7 接感应触发。

| 功能 | MCU脚 | 外设/方向 | 模块侧连接 | 结论 |
|---|---|---|---|---|
| `HALL_SW` | `PB8` | GPIO 输入，上拉，低电平有效 | 接霍尔/感应器 OUT / 开漏下拉输出 | 2026-07-05 正式打印感应触发采用 |

限制说明：PB8 在旧裸屏接口中曾是 `LCM_BL_LED`。2026-07-05 图纸已改为 `HALL_SW`，因此不要再把 PB8 当 LCD 背光输出。若后续回退旧裸屏方案，需要重新分配感应触发脚。

完整表格见同目录 DOCX：`SCH_FIXTURE_2026-06-30_IO核对与修改建议.docx`。

## 补充：LEDM两个TX/RX脚最终建议

明确建议如下：

| LEDM信号 | 推荐MCU脚 | 方向 | 模块侧连接 | 结论 |
|---|---|---|---|---|
| `LEDM_TX` | `PA6` | MCU -> LEDM | 接 LEDM 模块 `RX` | 推荐采用 |
| `LEDM_RX` | `PA7` | LEDM -> MCU | 接 LEDM 模块 `TX` | 推荐采用 |
| `3.3V` | 电源 | 供电 | 接 LEDM `VCC` | 优先选3.3V模块 |
| `GND` | 地 | 共地 | 接 LEDM `GND` | 必须 |

画图建议：新增 `J_LEDM_4P`，针脚顺序建议：`1=3.3V`，`2=GND`，`3=LEDM_TX(PA6, 接模块RX)`，`4=LEDM_RX(PA7, 接模块TX)`。

限制说明：`PA6/PA7` 作为 LEDM 串口建议按 GPIO/软件UART 使用，不是当前硬件 `USART1`。不要再用 `PA9/PA10` 做 LEDM，因为新版图纸中 `PA9/PA10` 已作为主 `USART_TX/RX`。

## 修订：根据PCB约束，LEDM改用PA9/PA10

由于当前PCB没有把 `PA6/PA7` 引出来，LEDM最终建议改用 `PA9/PA10`。

| LEDM信号 | MCU脚 | 模块侧连接 | 结论 |
|---|---|---|---|
| `LEDM_TX` | `PA9` | 接 LEDM 模块 `RX` | 最终采用 |
| `LEDM_RX` | `PA10` | 接 LEDM 模块 `TX` | 最终采用 |

`PA9/PA10` 的冲突对象是：新版图纸中它们原本是 `USART_TX/USART_RX`，当前固件 `rpi_rs485.c` 也把它们作为 USART1 通信口。因此如果 `PA9/PA10` 给 LEDM 使用，就不能同时再作为树莓派/PC/RS485 主通信口使用。

如果 LEDM 是串口LED/串口屏：使用 `PA9=USART1_TX`、`PA10=USART1_RX`，模块侧交叉接线。

如果 LEDM 是 TM1637：不要叫 TX/RX，应改名为 `LEDM_CLK=PA9`、`LEDM_DIO=PA10`，固件按GPIO时序驱动，不启用USART1。

## 修订：打印端 LCDM 与打印机通讯 IO

打印端 LCDM 使用与 MOTOR / Steering Engine 项目相同的 TJC 陶晶驰串口 HMI 方向，不走图纸旧 `LCM_SPI_SCK/MOSI/RESET/CMD/CS/BL` 裸屏接口作为首选。

| 功能 | MCU脚 | 外设/方向 | 模块侧连接 | 结论 |
|---|---|---|---|---|
| `LCDM_TX` | `PB3` | GPIO 软件 UART TX | 接 TJC LCDM `RX` | 2026-07-05 图纸采用 |
| `LCDM_RX` | `PB5` | GPIO 软件 UART RX | 接 TJC LCDM `TX` | 2026-07-05 图纸采用 |
| `PRINTER_TX` | `PA9` | `USART1_TX` | 接打印机/RS485 转换器 `RX/DI` | 打印主机采用 |
| `PRINTER_RX` | `PA10` | `USART1_RX` | 接打印机/RS485 转换器 `TX/RO` | 打印主机采用 |
| `RS485_DE_RE` | `PA1` | GPIO，可选 | 接收发器 `DE`/`RE` | 半双工需要时启用 |
| `WIFI_MODULE` | 未定 | 未分配 | 待 WiFi 模块选型后确认 | 当前不写死 UART/SPI/SDIO 引脚 |

限制说明：TJC/陶晶驰 LCDM 是串口 HMI，通讯量很小，不要求固定硬件 USART；当前 PB3/PB5 采用软件 UART，默认 9600 8N1，稳定后可评估 38400。`PB4` 不写入 2026-07-05 TJC LCDM 当前规格。`PB6/PB7` 已用于 IR，`PB8` 已用于 `HALL_SW`。`PA2` 已用于 `ADC2_IN2` 扫描输入，不能再给 LCDM、打印机或 WiFi/无线模块。`PH2` 不写入 AT32F455VET7 LQFP100 最终规格，除非重新用封装 pinout 证明它是可落板脚。`PA9/PA10` 在测试机角色中可作为 LEDM/TM1637；在打印主机角色中作为打印机通讯 USART1。两种角色按 BOM/连接器二选一，不能同时并接 LEDM 和打印机通讯。WiFi/无线模块尚未选型，不分配固定 MCU IO。

## 修订：红外打印通讯 IO

| 功能 | MCU脚 | 外设/方向 | 模块侧连接 | 结论 |
|---|---|---|---|---|
| `IR_TX` | `PB6` | GPIO 输出 / 载波包络 | 接 IR LED 驱动管输入 | 2026-07-05 图纸采用 |
| `IR_RX` | `PB7` | GPIO 输入，上拉 | 接 38kHz 解调接收头 OUT | 2026-07-05 图纸采用 |

限制说明：旧记录曾把 PB6/PB7 写反；以 2026-07-05 图纸为准，固件 `ir_remote.c` 已同步为 `PB6=IR_TX`、`PB7=IR_RX`。
