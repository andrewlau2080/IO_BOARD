# IO BOARD 硬件更改清单 2026-07-02

目的：给重新设计原理图和 PCB layout 使用。当前最终核对依据以 `SCH_FIXTURE_2026-07-07-N.pdf` 为准；`SCH_FIXTURE_2026-07-07.pdf`、`SCH_FIXTURE_2026-07-05.pdf` 和旧 `SCH_FIXTURE_2026-06-30.pdf` 只作历史对照。最终完整核对表见 `SCH_FIXTURE_2026-07-07_IO核对与PCB检查表.md`。

产品规划：所有功能集中在同一块通用 PCB 上，测试机/打印主机角色由固件通信或配置判定。`PA9/PA10` 只保留一个共用通讯接插件：测试机角色外接 LEDM，打印主机角色外接打印机/通讯转换器，不再画两个并联外设接口。

## 总体原则

| 序号 | 项目 | 结论 | 原因/说明 | 优先级 |
|---:|---|---|---|---|
| 1 | 通用 PCB | 可以同一 PCB 覆盖测试机与打印主机 | 固件可统一；PA9/PA10 按一个共用通讯接插件处理，现场按角色插 LEDM 或打印机通讯线 | 必须 |
| 2 | 电平 | 所有 MCU IO 外部信号必须限制在 0-3.3V | AT32F455 GPIO 不允许直接承受 5V 输入；4051 也建议 3.3V 供电 | 必须 |
| 3 | 测试点 | IR_TX、IR_RX、LEDM_CLK/TX、LEDM_DIO/RX、SWDIO、SWCLK、NRST 必须加测试点 | 本轮调试证明没有测试点会严重拖慢定位 | 必须 |

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

## LEDM / TM1637 显示与按键

| 序号 | 信号 | MCU 引脚 | 建议连接 | 原因/说明 | 优先级 |
|---:|---|---|---|---|---|
| 1 | 共用通讯口 TX / LEDM_CLK 或 LEDM_TX | PA9 | 只接到一个共用接插件；测试机接 LEDM，打印主机接打印机/通讯转换器 | 同一实物不会同时插两种外设，避免 PA9 分叉并接 | 必须 |
| 2 | 共用通讯口 RX / LEDM_DIO 或 LEDM_RX | PA10 | 只接到同一个共用接插件；测试机接 LEDM，打印主机接打印机/通讯转换器 | 同一实物不会同时插两种外设，避免 PA10 分叉并接 | 必须 |
| 3 | 共用接插件电源 | 3.3V/GND | 建议 4 pin：3.3V、GND、PA9/TX、PA10/RX；丝印写角色通讯口，不只写 LEDM 或 PRINTER | 选 3.3V 可用 LEDM；打印通讯转换器需确认 3.3V TTL 兼容 | 必须 |
| 4 | 上拉 | PA9/PA10 | 若测试机外接 TM1637，可由模块自带或在模块侧上拉；主板上不建议固定加影响 USART 的上拉 | 避免打印主机 USART 模式被不必要外设电路影响 | 建议 |
| 5 | 按键 | TM1637 键盘 | K1-K4 使用 TM1637 键值，不建议另接 PC4-PC9 作为主按键 | 当前程序主要按 TM1637 键值定义：K1/K2/K3/K4 | 必须 |
| 6 | USART1 角色复用 | PA9/PA10 | 测试机固件可按 LEDM/TM1637 使用；打印主机固件按 USART1 打印通讯使用 | 共用同一接插件，不属于两个接口冲突 | 必须 |

上电角色判定：

| 步骤 | 判定 | 后续动作 |
|---:|---|---|
| 1 | 先在 `PB3/PB5` 探测 LCDM | LCDM 有响应则锁定打印主机角色，初始化 LCDM、打印机通讯、WiFi/MAS 和本地缓存 |
| 2 | LCDM 无响应时，再在 `PA9/PA10` 共用通讯口探测 LEDM | 只发送 LEDM/TM1637 安全探测，不发送打印命令 |
| 3 | LEDM 有有效响应 | 锁定测试机角色，初始化 4051 扫描、LEDM 显示和测试流程 |
| 4 | LCDM 和 LEDM 都无响应 | 进入未识别安全模式，不自动测试，不发送打印命令 |

建议每类探测重试 3 次，每次 50-100 ms。角色锁定后本次运行不再自动切换；测试机中途 LEDM 掉线只报 LEDM 错误，打印主机中途 LCDM/打印机掉线只报对应错误。

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
| 2 | LCDM 串口 | `PB3=LCDM_TX`、`PB5=LCDM_RX` | 按 2026-07-07-N 图纸接 `J_LCDM_TJC`：VCC、GND、LCDM_RX、LCDM_TX；MCU TX 接屏 RX，MCU RX 接屏 TX | TJC 不要求固定硬件 USART；使用软件 UART，避免占用 `PA9/PA10`，且不占用 `PA2/ADC2_IN2` 或封装未确认的 `PH2` | 必须 |
| 3 | LCDM 电源/电平 | 按屏模块要求供电，串口 3.3V 兼容 | 若屏为 5V 供电但串口不是 3.3V 兼容，必须加电平转换 | AT32 GPIO 不能直接承受 5V 输入 | 必须 |
| 4 | 旧裸屏接口 | PB3/SCK、PB5/MOSI、PB4/RESET、PB6/CMD、PB7/CS、PB8/BL | 2026-07-07-N 图纸为：PB3/PB5=TJC LCDM，PB4=backup05，PB6/PB7=IR，PB8=HALL_SW；旧裸屏接口不再作为当前方案 | 若最终改回裸屏，固件要另写 SPI LCD + 触摸 + 软键盘，并重分配 IR/感应触发脚 | 建议 |
| 5 | 打印机 RS485/串口 | `PA9=USART1_TX`、`PA10=USART1_RX` 共用通讯接插件 | 打印主机角色通过同一接插件连接打印机串口/RS485 转换器；如用 RS485，转换器侧处理 A/B、终端电阻、屏蔽地 | 测试机角色同一接插件接 LEDM；PCB 不再增加第二个 PA9/PA10 接口 | 必须 |
| 6 | RS485 方向控制 | 待重新分配 | `PA1` 已为 `DEBUG_TTL_RX`，不得再作为 DE/RE；如半双工 RS485 必须要方向控制，需从最终空闲脚重新指定 | 后续不同收发器/协议可能需要方向控制，但不能占用 DEBUG_TTL | 建议 |
| 7 | 打印机参数调试 | LCDM | LCDM 可设置打印波特率、协议、标签内容和打印参数 | 后续兼容不同机型 | 建议 |
| 8 | WiFi/无线模块 | `PC3=WIFI_TX`、`PB9=WIFI_RX`、`WIFI_EN/WIFI_BOOT` 不接 AT32 | 按 `docs/wifi_module_pcb_connection_plan.md` 接 07-07-N 图纸中的 `ESP32-C3-WROOM-02U-N4`；`PC3` 接 ESP32 `RXD`，`PB9` 接 ESP32 `TXD`；EN/BOOT 只做 10k 上拉和测试点/按键 | 避开 `PA1/PA3(DEBUG_TTL)`、`PA2`、`PH2`、`PD8/OUT_BMUX_EN0`、`PA9/PA10`、LCDM、IR、HALL 和按键脚；第一版按 GPIO 软件 UART/网络协处理器设计 | 必须 |
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
| 共用角色通讯口 / LEDM/TM1637 | PA9=TX/CLK、PA10=RX/DIO | PCB 只留一个接插件；测试机接 LEDM，固件可按 GPIO 时序和 TM1637 键值使用 |
| 感应打印触发 | PB8=HALL_SW | 输入上拉，低电平有效；PC7 保留按键 |
| 打印端 LCDM | PB3=LCDM_TX、PB5=LCDM_RX | TJC/陶晶驰串口 HMI，软件 UART，默认 9600 8N1 |
| 打印机通讯 | PA9=USART1_TX、PA10=USART1_RX | 打印主机角色使用同一共用接插件；不另画第二个 PA9/PA10 接口 |
| IR 打印测试 | PB6=IR_TX、PB7=IR_RX | 按 2026-07-07-N 图纸更新；正式建议硬件 PWM 载波 |
| WiFi/无线模块 | PC3=WIFI_TX、PB9=WIFI_RX；WIFI_EN/WIFI_BOOT 不接 AT32 | 07-07-N 图纸为 ESP32-C3-WROOM-02U-N4，连接规格见 `docs/wifi_module_pcb_connection_plan.md`；`PA1/PA3/PD8` 禁止使用 |
| 打印主机 SPI NOR Flash | PB13=FLASH_SPI_SCK、PB14=FLASH_SPI_MISO、PB15=FLASH_SPI_MOSI、PA6=FLASH_CS_N | 25Q 系列 SPI NOR；`WP#`/`HOLD#` 第一版上拉，不占 MCU；`PB12` 已为 `OUT_BMUX_C2`，不得作为 Flash 片选 |
| DEBUG_TTL | PA1=DEBUG_TTL_RX、PA3=DEBUG_TTL_TX | 按 2026-07-07-N 原理图保留，不再挪给 WiFi 或 RS485 DE/RE |
| 扫描输出 | PA4/PA5 | 数字输出激励 |
| 扫描输入 | PA0/PA2 | 当前按数字输入验证，ADC 暂不作为唯一依据 |
| DEBUG_OUT | 07-07-N 未分配 PH3 | 不再把 PH2/PH3 写入最终 IO 规格，除非重新证明 LQFP100 可落板 |
| SWD | PA13/PA14/NRST | 必须保留，不得挪用 |
