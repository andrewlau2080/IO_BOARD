# IO BOARD 硬件更改清单 2026-07-02

目的：给重新设计原理图和 PCB layout 使用。当前最终核对依据以 `SCH_FIXTURE_2026-07-07-N.pdf` 为准；`SCH_FIXTURE_2026-07-07.pdf`、`SCH_FIXTURE_2026-07-05.pdf` 和旧 `SCH_FIXTURE_2026-06-30.pdf` 只作历史对照。最终完整核对表见 `SCH_FIXTURE_2026-07-07_IO核对与PCB检查表.md`。

产品规划：所有功能集中在同一块通用 PCB 上，测试机/打印主机角色由固件配置和绑定资料判定。2026-07-12 起，`PA9/PA10` 定义为显示共用口：上电先按 LCDM 串口协议多次探测，若有 LCDM 回应则进入 LCDM 显示模式；若无回应则改为 LEDM/TM1637 模式。普通测试机和高档测试机同一时间只装 LEDM 或 LCDM 其中一种，不再要求二者共存。打印主机同样使用 LCDM 做标签输入/参数设置，打印机通讯改由 `PB5/PB3` 独立接口输出。

## 总体原则

| 序号 | 项目 | 结论 | 原因/说明 | 优先级 |
|---:|---|---|---|---|
| 1 | 通用 PCB | 可以同一 PCB 覆盖测试机与打印主机 | 固件可统一；PA9/PA10 作为 LEDM/LCDM 显示共用口，PB5/PB3 作为打印机通讯口 | 必须 |
| 2 | 电平 | 所有 MCU IO 外部信号必须限制在 0-3.3V | AT32F455 GPIO 不允许直接承受 5V 输入；4051 也建议 3.3V 供电 | 必须 |
| 3 | 测试点 | IR_TX、IR_RX、LEDM_CLK/TX、LEDM_DIO/RX、SWDIO、SWCLK、NRST 必须加测试点 | 本轮调试证明没有测试点会严重拖慢定位 | 必须 |
| 4 | 2 kHz 蜂鸣器 | `PB4=BUZZER_2K`，LQFP100 Pin 90 | 高电平有效，默认低电平关闭；建议用三极管/MOSFET 驱动有源蜂鸣器 | 必须 |

## 公共硬件必须修改

| 序号 | 模块 | 当前/风险 | 建议修改 | PCB Layout 注意事项 | 优先级 |
|---:|---|---|---|---|---|
| 1 | SWD 调试口 | 必须确认没有把 PA13 占作 DEBUG_TTL_TX 或其它功能 | J_SWD 固定为：VREF/3.3V、GND、PA13/SWDIO、PA14/SWCLK、NRST | SWD 走线短、旁边放 GND；NRST 上拉 10k，可加 100nF 到 GND | 必须 |
| 2 | NRST | 调试和量产下载需要稳定复位 | 新增或确认 NRST 到 SWD 口和复位按键 | NRST 远离强干扰，保留测试点 | 必须 |
| 3 | BOOT0 | 防止上电进错模式 | BOOT0 默认 10k 下拉到 GND，可加跳帽到 3.3V | 靠近 MCU，避免悬空 | 建议 |
| 4 | 3.3V 电源 | TM1637、4051、IR 接收头均依赖 3.3V | 每个模块接口附近加 100nF；IR 接收头旁加 100nF + 4.7uF | 电源线宽足够，IR LED 驱动电流不要从 MCU IO 直接供 | 必须 |

## 扫描 IO 与 4051

| 序号 | 信号/模块 | MCU 引脚 | 建议修改 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | OUT_A 地址 | PC10/PC11/PC12 | 保留 | 当前固件 `io_board.c` 使用这 3 根控制 OUT_A 4051 地址 | 必须 |
| 2 | OUT_A EN | PD0-PD7 | 保留，低有效 | 4051 inhibit/enable 默认禁用，高电平禁用，选中时拉低 | 必须 |
| 3 | OUT_B 地址 | PB10/PB11/PB12 | 若 B 区暂时不用，可保留但允许 DNP | 当前程序可屏蔽 B 区，硬件建议预留方便后续扩展 | 建议 |
| 4 | OUT_B EN | `PD8..PD15 = OUT_BMUX_EN0..EN7` | 旧记录曾写 `PD9..PD15, PC6`，已按 2026-07-07-N 图纸修正；`PD8` 不得再分配给 WiFi | 避免 WiFi 占用 4051 OUT_B 使能线 | 必须 |
| 5 | IN_A 地址 | PC0/PC1/PC2 | 保留 | 当前固件使用 | 必须 |
| 6 | IN_A EN | PE0-PE7 | 保留，低有效 | 当前固件使用 | 必须 |
| 7 | IN_B 地址 | PB0/PB1/PB2 | 若 B 区暂时不用，可保留但允许 DNP | 后续扩展用 | 建议 |
| 8 | IN_B EN | PE8-PE15 | 若 B 区暂时不用，可保留但允许 DNP | 后续扩展用 | 建议 |
| 9 | 扫描输出激励 | PA4/PA5 | 保留测试点；确认通过前端进入 4051/线束 | 当前程序使用 PA4/PA5 作输出激励，前端需匹配 3.3V IO | 必须 |
| 10 | 扫描输入检测 | PA0/PA2 | 保留测试点；当前暂按普通数字输入验证 | ADC 方案此前不稳定，硬件需预留可选 RC/保护/分压，避免直接 5V | 必须 |
| 11 | 4051 供电 | 74HC4051 VCC | 统一 3.3V 供电 | MCU 3.3V 控制 HC4051 在 3.3V 供电下电平匹配 | 必须 |
| 12 | 外部输入保护 | IN/OUT 到 4051/MCU | 加限流/钳位/分压或明确只允许 3.3V | 任何外部 5V 都不能直接进入 3.3V 4051/MCU | 必须 |

## LEDM / TM1637 / LCDM 显示与按键

| 序号 | 信号 | MCU 引脚 | 建议连接 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | 显示共用口 TX / LEDM_CLK / LCDM_RX | PA9 | 只接到一个显示共用接插件；普通测试机接 LEDM，高档测试机或打印主机接 LCDM | 同一实物只装 LEDM 或 LCDM 其中一种，避免 PA9 分叉并接 | 必须 |
| 2 | 显示共用口 RX / LEDM_DIO / LCDM_TX | PA10 | 只接到同一个显示共用接插件；LEDM 模式为 TM1637 DIO，LCDM 模式为 USART1_RX | 上电先按 LCDM 协议探测，无回应再切 LEDM 模式 | 必须 |
| 3 | 显示共用接插件电源 | 3.3V/GND | 建议 4 pin：3.3V、GND、PA9/TX、PA10/RX；丝印写 DISPLAY，不只写 LEDM 或 LCDM | LCDM 若为 5V 供电但串口非 3.3V 兼容，必须加电平转换 | 必须 |
| 4 | 上拉 | PA9/PA10 | LEDM/TM1637 可由模块侧上拉；主板上不建议固定加过强上拉影响 USART1 | 兼容 LCDM 硬件 USART 和 TM1637 GPIO 两种模式 | 建议 |
| 5 | 按键 | TM1637 键盘 | K1-K4 使用 TM1637 键值，不建议另接 PC4-PC9 作为主按键 | 当前程序主要按 TM1637 键值定义：K1/K2/K3/K4 | 必须 |
| 6 | USART1 角色复用 | PA9/PA10 | LCDM 模式按 USART1 使用；LEDM 模式按 GPIO 模拟 TM1637 使用 | 上电检测只检测 LCDM 协议，失败后进入 LEDM 模式 | 必须 |
| 7 | 打印机通讯口 | PB5=PRINTER_TX、PB3=PRINTER_RX | 打印主机角色通过该接口连接打印机串口/RS232/RS485 转换器 | 标签内容由 LCDM 输入到 AT32，AT32 再从 PB5/PB3 发给打印机 | 必须 |

上电角色判定：

| 步骤 | 判定 | 后续动作 |
|---:|---|---|
| 1 | PA9/PA10 按 LCDM USART1 初始化 | 连续 3-5 次发送 LCDM 握手命令 |
| 2 | LCDM 有正确回应 | 锁定 `DISPLAY_LCDM`；PA9/PA10 保持 USART1 LCDM |
| 3 | LCDM 多次无回应 | 锁定 `DISPLAY_LEDM`；PA9/PA10 改为 TM1637/LEDM GPIO 时序 |
| 4 | 读取本机 Flash 角色配置 | 区分 `TESTER_LEDM`、`TESTER_LCDM`、`PRINT_HOST`、`UNCONFIGURED` |
| 5 | 角色配置缺失或 CRC 错误 | LCDM 模式可进入维护选择页；无 LCDM 时进入安全模式或 LEDM 错误提示 |

显示类型锁定后本次运行不再自动切换。LCDM 有回应只代表显示类型是 LCDM，不能单独代表打印主机；打印主机和高档测试机都可能使用 LCDM。设备角色仍以 Flash 配置、SPI NOR Flash、PB5/PB3 打印机接口检测和 LCDM 维护页面选择为辅助依据。

蜂鸣器节奏：

| 事件 | PB4 输出 |
|---|---|
| PASS | 1.0 s 高电平响，1.0 s 低电平停 |
| NG | 0.5 s 高，0.5 s 低，0.5 s 高，1.0 s 低 |

## 感应触发输入

| 序号 | 信号/模块 | MCU 引脚/接口 | 建议修改 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | `HALL_SW` / 打印感应触发 | `PB8` | 按 2026-07-07-N 图纸接霍尔/感应器输出；接口建议 3.3V、GND、HALL_SW | `PC7` 是按键 `KEY_LEFT`，此前只作临时触发测试，正式版本不得再占用 | 必须 |
| 2 | 触发电平 | `PB8` 输入上拉，低电平有效 | 传感器输出用开漏/集电极开路下拉，或 3.3V TTL；必要时加 100R-1k 串阻和 ESD/RC 位置 | AT32 GPIO 不允许 5V 直接输入；低有效与当前固件一致 | 必须 |
| 3 | 与旧裸屏冲突 | `PB8` 旧图曾作 `LCM_BL_LED` | 2026-07-07-N 图纸已改为 `HALL_SW`；不要再把 PB8 当 LCD 背光输出 | 避免 PB8 同时作为输出背光和输入触发 | 必须 |

## 红外打印通讯

| 序号 | 信号/模块 | 当前临时脚 | 建议最终硬件 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | IR_TX | PB6 | 按 2026-07-07-N 图纸接 IR 发射驱动；必须用三极管/MOSFET 驱动 IR LED，不要 MCU 直接带 LED | 固件已按 `PB6=IR_TX` 更新；正式建议用硬件 PWM 载波 + 包络开关 | 必须 |
| 2 | IR_RX | PB7 | 接 38kHz 解调红外接收头 OUT，优先 3.3V 接收头 | 固件已按 `PB7=IR_RX` 更新；输入需上拉到 3.3V | 必须 |
| 3 | IR 接口 | J_IR | 建议 4-6 pin：3.3V、GND、IR_TX_LED+、IR_TX_DRV、IR_RX_OUT、可选 5V_LED | 方便独立调试和更换发射/接收组件 | 必须 |
| 4 | IR_TX 驱动极性 | PB6 高电平 = 发光 | 硬件按高电平打开驱动设计；若硬件反相，固件必须同步改极性 | 避免 MARK/SPACE 反相导致接收端不能解码 | 必须 |
| 5 | IR LED 电流 | 外接驱动 | 低边 N-MOS 或 NPN，串联限流电阻；按距离选择 20-100mA 脉冲电流 | MCU IO 不能直接输出大电流 | 必须 |
| 6 | IR 接收头供电 | 3.3V | 接收头旁 100nF + 4.7uF；OUT 到 PB7 可加 100R 串联 | 降低误码和供电噪声 | 建议 |
| 7 | 载波生成 | PB6 或其它确认可 PWM 的脚 | 最终 PCB 要确认 IR_TX 引脚支持定时器 PWM；若 PB6 不适合硬件 PWM，固件继续用软件载波或重新分配脚 | 正式代码应避免软件翻转载波的时序误差 | 必须 |
| 8 | 与旧裸屏 LCDM 冲突 | PB6/PB7 | 若使用图纸上的裸屏 `LCM_CMD/CS`，不能同时接 IR；打印端已改用 TJC 串口 HMI 后，PB6/PB7 可继续留给 IR 或不装 | 两种产品独立，BOM 或连接器要分开 | 必须 |

## 打印接收 / LCDM 主机硬件

| 序号 | 模块 | MCU 引脚/接口 | 建议修改 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | LCDM 品牌/类型 | TJC 陶晶驰串口 HMI | 与 MOTOR / Steering Engine 项目使用同一品牌/协议方向；不要按裸 SPI LCDM 当作首选 | 屏幕自己处理触摸、键盘、字体和页面，MCU 只收发变量/命令 | 必须 |
| 2 | LCDM 串口 | `PA9=USART1_TX`、`PA10=USART1_RX` | 接显示共用口：MCU TX 接 LCDM RX，MCU RX 接 LCDM TX；上电先探测 LCDM 协议 | 使用硬件 USART1，减少 LCDM 驱动对扫描流程的时序干涉 | 必须 |
| 3 | LCDM 电源/电平 | 按屏模块要求供电，串口 3.3V 兼容 | 若屏为 5V 供电但串口不是 3.3V 兼容，必须加电平转换 | AT32 GPIO 不能直接承受 5V 输入 | 必须 |
| 4 | 旧裸屏接口 | PB3/SCK、PB5/MOSI、PB4/RESET、PB6/CMD、PB7/CS、PB8/BL | 2026-07-12 新基准为：PA9/PA10=TJC LCDM 或 LEDM 显示共用口，PB5/PB3=打印机通讯，PB4=BUZZER_2K，PB6/PB7=IR，PB8=HALL_SW；旧裸屏接口不再作为当前方案 | 若最终改回裸屏，固件要另写 SPI LCD + 触摸 + 软键盘，并重分配蜂鸣器/IR/感应触发脚 | 建议 |
| 5 | 打印机 RS485/串口 | `PB5=PRINTER_TX`、`PB3=PRINTER_RX` | 打印主机角色连接打印机串口/RS232/RS485 转换器；如用 RS485，转换器侧处理 A/B、终端电阻、屏蔽地 | PA9/PA10 已作为 LEDM/LCDM 显示共用口，打印机必须独立到 PB5/PB3 | 必须 |
| 6 | RS485 方向控制 | 待重新分配 | `PA1` 已为 `DEBUG_TTL_RX`，不得再作为 DE/RE；如半双工 RS485 必须要方向控制，需从最终空闲脚重新指定 | 后续不同收发器/协议可能需要方向控制，但不能占用 DEBUG_TTL | 建议 |
| 7 | 打印机参数调试 | LCDM | LCDM 可设置打印波特率、协议、标签内容和打印参数 | 后续兼容不同机型 | 建议 |
| 8 | WiFi/无线模块 | `PC3=WIFI_TX`、`PB9=WIFI_RX`、`WIFI_EN/WIFI_BOOT` 不接 AT32 | 按 `docs/wifi_module_pcb_connection_plan.md` 接 07-07-N 图纸中的 `ESP32-C3-WROOM-02U-N4`；`PC3` 接 ESP32 `RXD`，`PB9` 接 ESP32 `TXD`；EN/BOOT 只做 10k 上拉和测试点/按键 | 避开 `PA1/PA3(DEBUG_TTL)`、`PA2`、`PH2`、`PD8/OUT_BMUX_EN0`、`PA9/PA10` 显示口、`PB5/PB3` 打印口、IR、HALL 和按键脚；第一版按 GPIO 软件 UART/网络协处理器设计 | 必须 |
| 9 | 打印主机本地缓存 | `PB13/PB14/PB15 + PA6` 接 SPI NOR Flash | WiFi 模块 4 MB Flash 不作为生产追溯缓存；打印主机建议预留 SPI NOR Flash，16 MB 起步，推荐 32 MB 或 64 MB；长时间断网可预留 microSD/eMMC | MAS 断线时每条线 10 台测试机记录要保存在打印主机内，不能依赖 WiFi 模块 Flash | 必须 |
| 10 | 禁止复用 | `PA2=ADC2_IN2` | `PA2` 保留给 B 区扫描输入检测，不得再接 LCDM/打印机/无线模块 | 避免破坏 48+48 扫描检测路径 | 必须 |
| 11 | 禁止复用 | `PH2` | `PH2` 不写入 AT32F455VET7 LQFP100 最终 IO 规格，除非重新用封装 pinout 证明它是可落板脚 | 避免 WorkBench 信号名和实际 QFP100 封装不一致导致画图错误 | 必须 |
| 12 | 禁止复用 | `PA1=DEBUG_TTL_RX`、`PA3=DEBUG_TTL_TX` | 07-07-N PDF 已把 PA1/PA3 用于 DEBUG_TTL，不得再给 RS485 方向、WiFi 或其它功能 | 避免调试串口和 WiFi/RS485 互相抢脚 | 必须 |

## 打印主机本地 SPI NOR Flash

核查依据：`SCH_FIXTURE_2026-07-07-N.pdf` 已确认 `U7=W25Q128JVSIQ`，`PB13=FLASH_SPI_SCK`、`PB14=FLASH_SPI_MISO`、`PB15=FLASH_SPI_MOSI`、`PA6=FLASH_CS_N`。`PB12` 已是 `OUT_BMUX_C2`，不能作为 SPI 片选脚使用。

注意：07-07-N 原理图已把旧版 `FALSH_CS_N` / `FALSH_WP_N` 修正为 `FLASH_CS_N` / `FLASH_WP_N`。

| 序号 | NOR Flash 信号 | AT32F455VET7 引脚 | LQFP100 脚号 | 连接到 25Q Flash | PCB/固件说明 | 结论 |
|---:|---|---|---:|---|---|---|
| 1 | `FLASH_SPI_SCK` | `PB13` | 52 | `CLK` / Pin 6 | 按 SPI2 SCK 使用，串 22R/0R 预留 | 推荐采用 |
| 2 | `FLASH_SPI_MISO` | `PB14` | 53 | `DO` / `IO1` / Pin 2 | Flash -> AT32，串 22R/0R 预留 | 推荐采用 |
| 3 | `FLASH_SPI_MOSI` | `PB15` | 54 | `DI` / `IO0` / Pin 5 | AT32 -> Flash，串 22R/0R 预留 | 推荐采用 |
| 4 | `FLASH_CS_N` | `PA6` | 31 | `CS#` / Pin 1 | 普通 GPIO 片选，默认 10k 上拉到 3.3V | 推荐采用 |
| 5 | `FLASH_WP_N` | 不占 MCU | - | `WP#` / `IO2` / Pin 3 | 10k 上拉到 3.3V，预留测试点或 0R/DNP 到备用 GPIO | 第一版上拉 |
| 6 | `FLASH_HOLD_N` | 不占 MCU | - | `HOLD#` / `RESET#` / `IO3` / Pin 7 | 10k 上拉到 3.3V，预留测试点或 0R/DNP 到备用 GPIO | 第一版上拉 |
| 7 | `VCC` | 3.3V | - | Pin 8 | Flash 旁放 0.1uF，另加 1uF/4.7uF 可选 | 必须 |
| 8 | `GND` | GND | - | Pin 4 | 就近接地平面 | 必须 |

说明：`PB13/PB14/PB15` 一旦用于 SPI NOR Flash，就不再作为 WiFi 备用脚。WiFi 主连接继续使用 `PC3/PB9`，`WIFI_EN/WIFI_BOOT` 第一版只上拉和留测试点。`PA7` 暂不占用，保留给后续 `FLASH_WP_N`、`FLASH_HOLD_N`、RS485 方向或其它低速控制脚复核使用。

## PCB Layout 注意事项

| 序号 | 项目 | 要求 | 优先级 |
|---:|---|---|---|
| 1 | IR_TX/PB6 | 从 MCU 到驱动管栅极/基极走线短；驱动管靠近 IR LED 接口；电流回路不要穿过 MCU 模拟输入区域 | 必须 |
| 2 | IR_RX/PB7 | 接收头远离 IR 发射大电流回路；OUT 线短，旁边走 GND；供电去耦贴近接收头 | 必须 |
| 3 | PA0/PA2 输入 | 远离 IR LED、RS485、LCD 背光大电流；保留滤波/保护位置 | 必须 |
| 4 | 4051 模拟路径 | IN/OUT 线束路径保持清晰，避免和高速/大电流线平行长距离耦合 | 必须 |
| 5 | SWD | 接口靠边，线短；PA13/PA14 不要串太大电阻，建议 22R-100R 以内或 0R | 必须 |
| 6 | 接口丝印 | 所有外接连接器标清方向和电平：3.3V、GND、TX/RX 或 CLK/DIO | 必须 |

## 当前固件对应关系

| 功能 | 当前固件引脚 | 备注 |
|---|---|---|
| 显示共用口 / LEDM 或 LCDM | PA9=USART1_TX/LEDM_CLK、PA10=USART1_RX/LEDM_DIO | 上电先按 LCDM 协议探测；有回应保持 LCDM USART1，无回应切换 LEDM/TM1637 GPIO |
| 感应打印触发 | PB8=HALL_SW | 输入上拉，低电平有效；PC7 保留按键 |
| 打印端 LCDM | PA9=LCDM_TX、PA10=LCDM_RX | TJC/陶晶驰串口 HMI，硬件 USART1，默认 115200 8N1；标签字段由 LCDM 输入到 AT32 |
| 高档测试机 LCDM | PA9=LCDM_TX、PA10=LCDM_RX | 与打印端同一显示接口；通过 Flash 角色配置区分 `TESTER_LCDM` 和 `PRINT_HOST` |
| 2 kHz 蜂鸣器 | PB4=BUZZER_2K | 高电平有效；PASS 长响，NG 短响两下 |
| 打印机通讯 | PB5=PRINTER_TX、PB3=PRINTER_RX | 打印主机角色使用独立打印口；AT32 生成 ZPL/TSPL/ESC/POS 后发送给打印机/转换器 |
| IR 打印测试 | PB6=IR_TX、PB7=IR_RX | 按 2026-07-07-N 图纸更新；正式建议硬件 PWM 载波 |
| WiFi/无线模块 | PC3=WIFI_TX、PB9=WIFI_RX；WIFI_EN/WIFI_BOOT 不接 AT32 | 07-07-N 图纸为 ESP32-C3-WROOM-02U-N4，连接规格见 `docs/wifi_module_pcb_connection_plan.md`；`PA1/PA3/PD8` 禁止使用 |
| 打印主机 SPI NOR Flash | PB13=FLASH_SPI_SCK、PB14=FLASH_SPI_MISO、PB15=FLASH_SPI_MOSI、PA6=FLASH_CS_N | 25Q 系列 SPI NOR；`WP#`/`HOLD#` 第一版上拉，不占 MCU；`PB12` 已为 `OUT_BMUX_C2`，不得作为 Flash 片选 |
| DEBUG_TTL | PA1=DEBUG_TTL_RX、PA3=DEBUG_TTL_TX | 按 2026-07-07-N 原理图保留，不再挪给 WiFi 或 RS485 DE/RE |
| 扫描输出 | PA4/PA5 | 数字输出激励 |
| 扫描输入 | PA0/PA2 | 当前按数字输入验证，ADC 暂不作为唯一依据 |
| DEBUG_OUT | 07-07-N 未分配 PH3 | 不再把 PH2/PH3 写入最终 IO 规格，除非重新证明 LQFP100 可落板 |
| SWD | PA13/PA14/NRST | 必须保留，不得挪用 |
