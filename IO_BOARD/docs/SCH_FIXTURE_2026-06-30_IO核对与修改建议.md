# SCH_FIXTURE_2026-06-30 IO核对与修改建议

依据：`IO_BOARD/docs/SCH_FIXTURE_2026-06-30.pdf` 第3页 MCU2。目标 MCU：AT32F455VET7 LQFP100。

## 总体结论

| 模块 | 图纸现状 | 问题 | 建议 |
|---|---|---|---|
| 4051扫描脚 | IN/OUT A/B 地址线和使能线 | 和当前修正后的 `io_board.c` 一致 | 保留 |
| OUT_BMUX_EN | `PD9..PD15, PC6` | 新版图纸明确不是 `PD8..PD15` | 保留当前修正版 |
| SWD烧录 | J4标 `SWCLK/SWDIO`，但 MCU 侧疑似 `PA14/PA15`；`PA13` 被 `DEBUG_TTL_TX` 占用 | 标准 SWD 应使用 `PA13=SWDIO`、`PA14=SWCLK` | 必须改/确认 |
| PA9/PA10 | 图纸为 `USART_TX/USART_RX` | 旧 TM1637 代码使用 `PA9/PA10` 做 `CLK/DIO` | 必须改 |
| LCDM接口 | `PB3/PB4/PB5/PB6/PB7/PB8` | 图纸已有 LCDM/LCM 接口，代码只初始化部分控制脚 | 建议按 LCDM 实现 |
| LEDM/TM1637 | 图纸未见独立 LEDM/TM1637 `CLK/DIO` | 若要两组 LED 模块，需新增接口或占用空余 GPIO | 必须决定 |
| PA6/PA7 | 图纸无 `IR_RX/IR_TX` 网名 | 旧 IR 打印代码使用 `PA6/PA7`，不可直接上板 | 必须确认 |

## 给画图人员的修改清单

| 序号 | 修改项 | 建议修改内容 | 优先级 |
|---:|---|---|---|
| 1 | SWD接口 | J4改为：3.3V/Vref、`SWDIO=PA13`、`SWCLK=PA14`、GND，建议加 `NRST` | 必须改 |
| 2 | `DEBUG_TTL_TX` | 不要占用 `PA13`；移到空余脚或删除 | 必须改 |
| 3 | `PA9/PA10` | 只作为 `USART_TX/RX`；删除 TM1637/LEDM 复用说明 | 必须改 |
| 4 | 显示接口 | 优先使用 LCDM：`PB3 SCK`、`PB5 MOSI`、`PB4 RESET`、`PB6 CMD`、`PB7 CS`、`PB8 BL` | 建议改 |
| 5 | LEDM/TM1637 | 若要两组 LED 模块，新增独立 `J_LEDM`，不占 USART；建议 `CLK/DIO_ERR/DIO_CUR` 三线 | 待决定 |
| 6 | IR打印 | 如果保留红外打印，新增 `J_IR`：`IR_RX`、`IR_TX`、3.3V、GND，并指定 MCU 脚 | 待决定 |
| 7 | 固件 | 按最终图纸更新 `io_board.c`、LCDM驱动，禁用或迁移 `tm1637_display.c` 和 `ir_remote.c` | 后续执行 |

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
| `LCDM_TX` | `PB3` | GPIO 软件 UART TX | 接 TJC LCDM `RX` | 打印端 LCDM 采用 |
| `LCDM_RX` | `PB5` | GPIO 软件 UART RX | 接 TJC LCDM `TX` | 打印端 LCDM 采用 |
| `LCDM_RESET` | `PB4` | GPIO 输出/可选 | 接 TJC LCDM `RESET` 或 DNP | 只剩 PB3/PB4/PB5 时保留 |
| `PRINTER_TX` | `PA9` | `USART1_TX` | 接打印机/RS485 转换器 `RX/DI` | 打印主机采用 |
| `PRINTER_RX` | `PA10` | `USART1_RX` | 接打印机/RS485 转换器 `TX/RO` | 打印主机采用 |
| `RS485_DE_RE` | `PA1` | GPIO，可选 | 接收发器 `DE`/`RE` | 半双工需要时启用 |

限制说明：TJC/陶晶驰 LCDM 是串口 HMI，通讯量很小，不要求固定硬件 USART；当前 PB3/PB5 采用软件 UART，默认 9600 8N1，稳定后可评估 38400。`PA2` 已用于 `ADC2_IN2` 扫描输入，不能再给 LCDM。`PH2` 不写入 AT32F455VET7 LQFP100 最终规格，除非重新用封装 pinout 证明它是可落板脚。`PA9/PA10` 在测试机角色中可作为 LEDM/TM1637；在打印主机角色中作为打印机通讯 USART1。两种角色按 BOM/连接器二选一，不能同时并接 LEDM 和打印机通讯。
