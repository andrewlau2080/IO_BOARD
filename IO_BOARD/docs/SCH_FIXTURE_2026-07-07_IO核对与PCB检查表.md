# SCH_FIXTURE_2026-07-07-N IO 核对与 PCB 检查表

本文以 `SCH_FIXTURE_2026-07-07-N.pdf` 为原理图核查基准。2026-07-12 后续设计修订已重新定义显示和打印接口：`PA9/PA10` 作为 LEDM/LCDM 显示共用口，`PB5/PB3` 作为打印机通讯口。`SCH_FIXTURE_2026-07-07.pdf`、`SCH_FIXTURE_2026-07-05.pdf`、旧 ATWP、旧 MD 记录只作历史对照，不能再直接拿来分配新脚。

核对方法：使用“刘氏PDF”方式抽取 PDF 网络名，并只在 MCU2 引脚处或器件引脚处能对上的网络才写为“已确认”。接口处重复出现的网络名不单独作为 MCU 引脚依据。

## 总体结论

| 项目 | 结论 |
|---|---|
| 单板共用 | 可以做成一块通用 PCB，测试机/打印主机由固件通信或配置判断角色 |
| 硬件共用限制 | 角色判断不能解决硬连接冲突；同一 MCU 脚若同时焊接两个主动外设，必须用 DNP/0R/跳线/三态隔离处理 |
| 当前 PCB 基准 | 07-07-N 图纸已经包含 WiFi、SPI NOR Flash、LCDM、IR、HALL、CAN、USART、按键、4051 扫描；2026-07-09 规划把 `PB4/Pin90` 由 `backup05` 改为 `BUZZER_2K` |
| PA9/PA10 结论 | 只保留一个显示共用接插件；上电先按 LCDM 协议探测，有回应则为 LCDM，无回应则为 LEDM/TM1637 |
| WiFi | 已确认 `PC3=WIFI_TX`、`PB9=WIFI_RX`；`WIFI_EN/WIFI_BOOT` 当前按 ESP32 侧上拉/测试点处理，未确认接 AT32 GPIO |
| Flash | 已确认 `U7=W25Q128JVSIQ`，容量 128 Mbit / 16 MB；`PA6/PB13/PB14/PB15` 已分配给 SPI Flash；07-07-N 已修正 `FLASH_CS_N/FLASH_WP_N` 拼写 |

## 已确认 MCU2 关键引脚

| 功能 | MCU 引脚 | LQFP100 脚号 | 07-07-N 网络名 | 状态 |
|---|---|---:|---|---|
| VBAT | VBAT | 6 | `VBAT` | 已确认，不能悬空 |
| RTC/TAMPER 备用 | PC13 | 7 | `backup03` | 已确认 |
| 32.768 kHz | PC14 | 8 | `OSC32_IN` | 已确认 |
| 32.768 kHz | PC15 | 9 | `OSC32_OUT` | 已确认 |
| 主晶振 | OSC_IN | 12 | `OSC_IN` | 已确认 |
| 主晶振 | OSC_OUT | 13 | `OSC_OUT` | 已确认 |
| 复位 | NRST | 14 | `NRST` | 已确认 |
| IN A 地址 | PC0 | 15 | `IN_AMUX_A0` | 已确认 |
| IN A 地址 | PC1 | 16 | `IN_AMUX_B1` | 已确认 |
| IN A 地址 | PC2 | 17 | `IN_AMUX_C2` | 已确认 |
| WiFi UART | PC3 | 18 | `WIFI_TX` | 已确认，AT32 -> ESP32 RXD |
| 模拟地 | VSSA | 19 | `VSSA` | 已确认 |
| ADC 参考负 | VREF- | 20 | `VREF-` | 已确认 |
| ADC 参考正 | VREF+ | 21 | `VREF+` | 已确认 |
| 模拟电源 | VDDA | 22 | `VDDA` | 已确认 |
| ADC 输入 | PA0 | 23 | `ADC1_IN0` | 已确认 |
| 调试 TTL | PA1 | 24 | `DEBUG_TTL_RX` | 已确认，禁止复用 |
| ADC 输入 | PA2 | 25 | `ADC2_IN2` | 已确认，禁止复用 |
| 调试 TTL | PA3 | 26 | `DEBUG_TTL_TX` | 已确认，禁止复用 |
| DAC | PA4 | 29 | `DAC_OUT1` | 已确认 |
| DAC | PA5 | 30 | `DAC_OUT2` | 已确认 |
| Flash 片选 | PA6 | 31 | `FLASH_CS_N` | 已确认 |
| 备用 | PA7 | 32 | `backup01` | 已确认 |
| 按键 | PC4 | 33 | `KEY_ESC` | 已确认 |
| 按键 | PC5 | 34 | `KEY_OK` | 已确认 |
| IN B 地址 | PB0 | 35 | `IN_BMUX_A0` | 已确认 |
| IN B 地址 | PB1 | 36 | `IN_BMUX_B1` | 已确认 |
| IN B 地址 | PB2 | 37 | `IN_BMUX_C2` | 已确认 |
| IN A 使能 | PE0..PE7 | 97,98,1,2,3,4,5,38 | `IN_AMUX_EN0..EN7` | 已确认 |
| IN B 使能 | PE8..PE15 | 39..46 | `IN_BMUX_EN0..EN7` | 已确认 |
| OUT B 地址 | PB10 | 47 | `OUT_BMUX_A0` | 已确认 |
| OUT B 地址 | PB11 | 48 | `OUT_BMUX_B1` | 已确认 |
| OUT B 地址 | PB12 | 51 | `OUT_BMUX_C2` | 已确认 |
| Flash SPI | PB13 | 52 | `FLASH_SPI_SCK` | 已确认 |
| Flash SPI | PB14 | 53 | `FLASH_SPI_MISO` | 已确认 |
| Flash SPI | PB15 | 54 | `FLASH_SPI_MOSI` | 已确认 |
| OUT B 使能 | PD8..PD15 | 55..62 | `OUT_BMUX_EN0..EN7` | 已确认，PD8 已占用 |
| 按键 | PC6 | 63 | `KEY_RIGHT` | 已确认 |
| 按键 | PC7 | 64 | `KEY_LEFT` | 已确认 |
| 按键 | PC8 | 65 | `KEY_DOWN` | 已确认 |
| 按键 | PC9 | 66 | `KEY_UP` | 已确认 |
| 备用 | PA8 | 67 | `backup02` | 已确认 |
| 显示共用口 | PA9 | 68 | `USART_TX` | 新基准：LCDM_TX 或 LEDM_CLK/TX |
| 显示共用口 | PA10 | 69 | `USART_RX` | 新基准：LCDM_RX 或 LEDM_DIO/RX |
| CAN | PA11 | 70 | `CAN_RX` | 已确认 |
| CAN | PA12 | 71 | `CAN_TX` | 已确认 |
| SWD | PA13 | 72 | `SWDIO` | 已确认 |
| SWD | PA14 | 76 | `SWCLK` | 已确认 |
| 备用/JTAG 相关 | PA15 | 77 | `backup04` | 已确认，使用前需确认调试配置 |
| OUT A 地址 | PC10 | 78 | `OUT_AMUX_A0` | 已确认 |
| OUT A 地址 | PC11 | 79 | `OUT_AMUX_B1` | 已确认 |
| OUT A 地址 | PC12 | 80 | `OUT_AMUX_C2` | 已确认 |
| OUT A 使能 | PD0..PD7 | 81..88 | `OUT_AMUX_EN0..EN7` | 已确认 |
| 打印机通讯 | PB3 | 89 | `LCDM_TX` | 新基准：PRINTER_RX，接打印机/转换器 TX |
| 蜂鸣器 | PB4 | 90 | `BUZZER_2K` | 2026-07-09 新增，高电平有效 |
| 打印机通讯 | PB5 | 91 | `LCDM_RX` | 新基准：PRINTER_TX，接打印机/转换器 RX |
| IR | PB6 | 92 | `IR_TX` | 已确认 |
| IR | PB7 | 93 | `IR_RX` | 已确认 |
| 启动脚 | BOOT0 | 94 | `BOOT0` | 已确认 |
| 感应输入 | PB8 | 95 | `HALL_SW` | 已确认 |
| WiFi UART | PB9 | 96 | `WIFI_RX` | 已确认，ESP32 TXD -> AT32 |

## 禁止复用清单

| 引脚 | 当前 07-07-N 用途 | 禁止事项 |
|---|---|---|
| PA0 | `ADC1_IN0` | 不给 WiFi/LCDM/IR/打印通讯 |
| PA1 | `DEBUG_TTL_RX` | 不给 WiFi、RS485 DE/RE 或其它控制 |
| PA2 | `ADC2_IN2` | 不给 WiFi/LCDM/打印通讯 |
| PA3 | `DEBUG_TTL_TX` | 不给 WiFi、RS485 DE/RE 或其它控制 |
| PA6 | `FLASH_CS_N` | 不再当备用 WiFi 或 IR 脚 |
| PA9/PA10 | `USART_TX/RX` | 固定到显示共用口；LEDM 与 LCDM 二选一，不接打印机 |
| PA13/PA14 | `SWDIO/SWCLK` | 不复用为业务 IO |
| PB3/PB5 | 新基准为打印机通讯口 | 不再给 WiFi、IR 或 LCDM |
| PB4 | `BUZZER_2K` | 不再作为 LCDM reset、WiFi 备用或普通备用 |
| PB6/PB7 | `IR_TX/RX` | 不再给 LCDM 或 WiFi |
| PB8 | `HALL_SW` | 不再给 LCD 背光或按键 |
| PB13/PB14/PB15 | SPI Flash | 不再给 WiFi 备用 |
| PC4..PC9 | 按键 | 不再拿 PC7 做临时感应输入 |
| PD8..PD15 | `OUT_BMUX_EN0..EN7` | `PD8` 禁止再分配给 WiFi |
| PH2/PH3 | 07-07-N LQFP100 图未使用 | 不写入最终 IO 规格，除非重新证明封装可落板 |

## WiFi 模块连接

07-07-N 图纸当前模块为 `ESP32-C3-WROOM-02U-N4`，外接天线版本。若采购改 H4，必须同步 BOM/原理图型号后再定稿。

| WiFi 功能 | AT32 引脚 | LQFP100 脚号 | ESP32-C3-WROOM-02U 侧 | 方向 | 结论 |
|---|---|---:|---|---|---|
| `WIFI_TX` | PC3 | 18 | RXD / Pin 11 | AT32 -> ESP32 | 已确认 |
| `WIFI_RX` | PB9 | 96 | TXD / Pin 12 | ESP32 -> AT32 | 已确认 |
| `WIFI_EN` | 不接 AT32 | - | EN / Pin 2 | 硬件上拉/测试点 | 当前按不占 MCU IO |
| `WIFI_BOOT` | 不接 AT32 | - | IO9 / Pin 8 | 硬件上拉/测试点 | 当前按不占 MCU IO |
| `3V3_WIFI` | 电源 | - | 3V3 / Pin 1 | 电源 | 需按 WiFi 峰值电流设计 |
| GND | 系统地 | - | Pin 9、Pin 19-25 | 地 | 底部 GND 焊盘按规格书处理 |

PCB 注意：

- WiFi 3.3 V 分支建议预留 >= 500 mA 峰值能力，模块旁放 `10 uF + 0.1 uF`，可加 `22 uF/47 uF` 储能。
- `PC3/PB9` 串 0R 或 22R 预留，便于调试和改线。
- `WIFI_EN/WIFI_BOOT` 不能悬空，按 10 k 上拉到 3.3 V，并留 RESET/BOOT 测试点或按键到 GND。
- `02U` 外接天线连接器和天线位置必须避开大电流、开关电源、金属外壳遮挡。

## SPI NOR Flash

07-07-N 图纸已放置 `U7=W25Q128JVSIQ`，容量 16 MB。它适合作为打印主机断 MAS 时的本地缓存，不应依赖 ESP32 模块内部 4 MB Flash 保存生产追溯记录。

| Flash 信号 | AT32 引脚 | LQFP100 脚号 | W25Q128 引脚 | 结论 |
|---|---|---:|---|---|
| `FLASH_CS_N` | PA6 | 31 | CS# / Pin 1 | 已确认 |
| `FLASH_SPI_MISO` | PB14 | 53 | DO / IO1 / Pin 2 | 已确认 |
| `FLASH_WP_N` | 不占 MCU | - | IO2 / WP# / Pin 3 | 已确认网络名，建议上拉 |
| GND | GND | - | Pin 4 | 已确认 |
| `FLASH_SPI_MOSI` | PB15 | 54 | DI / IO0 / Pin 5 | 已确认 |
| `FLASH_SPI_SCK` | PB13 | 52 | CLK / Pin 6 | 已确认 |
| IO3 / HOLD# | 待视觉复核 | - | Pin 7 | 建议 10 k 上拉到 3.3 V |
| VCC | 3.3 V | - | Pin 8 | 模块旁 0.1 uF + 1 uF/4.7 uF |

必须确认：

- 07-07-N 已把 `FALSH_CS_N/FALSH_WP_N` 修正为 `FLASH_CS_N/FLASH_WP_N`。
- 若不用 Quad SPI，`WP#` 与 `HOLD#/IO3` 均建议 10 k 上拉到 3.3 V，并留测试点或 0R/DNP 位置。

## LCDM / LEDM / USART 关系

| 项目 | 07-07-N 结论 |
|---|---|
| TJC LCDM | `PA9=USART1_TX`、`PA10=USART1_RX`，硬件 USART1，默认 115200 8N1；普通测试机可不装，高档测试机和打印主机都可装 |
| PA9/PA10 显示共用口 | 只放一个接插件，建议丝印为 `J_DISPLAY`：VCC、GND、`PA9/TX/CLK`、`PA10/RX/DIO` |
| 普通测试机显示 | 该接插件外接 LEDM；若 LEDM 是串口模块，`PA9` 接 LEDM RX、`PA10` 接 LEDM TX；若是 TM1637，固件把 `PA9/PA10` 当 CLK/DIO GPIO 使用 |
| 高档测试机显示 | 同一接插件外接 TJC LCDM；`PA9/PA10` 上电先按 LCDM 协议探测 |
| 打印主机角色 | 使用同一 LCDM 显示接口输入标签字段；打印机串口/RS232/RS485 转换器固定接 `PB5/PB3` |
| 冲突结论 | 普通测试机和高档测试机同一时间只装 LEDM 或 LCDM 其中一种；LCDM 是否存在不能作为角色唯一依据 |

建议：

- PA9/PA10 从 MCU 只走到一个显示共用接插件，不再画第二个 LEDM 或 LCDM 并接口。
- 接插件丝印不要只写 `LEDM` 或只写 `LCDM`，避免装机时误解；建议标为显示口，并在装配说明中规定普通测试机接 LEDM，高档测试机/打印主机接 LCDM。
- PB5/PB3 从 MCU 只走到打印机通讯接口，连接串口/RS232/RS485 转换器。
- 若以后要求同一块实物板同时插 LEDM 和 LCDM，必须重新分配 LEDM 脚，或增加模拟开关/三态隔离。
- 高档测试机与打印主机都可能带 LCDM，因此不建议为“是否测试机/打印主机”再增加单独检测脚；角色应由 Flash 出厂配置、本线绑定或维护菜单写入，显示类型由外设探测或配置决定。

## 上电角色判定逻辑

2026-07-12 修订：上电只先检测 LCDM 协议。LCDM 有回应则显示类型为 LCDM，无回应则显示类型为 LEDM/TM1637。LCDM/LEDM 只能用于判断显示外设类型，整机角色必须优先来自本机非易失配置和绑定资料。

```text
上电
  -> MCU 基础初始化，PB4 蜂鸣器默认低电平关闭，PB5/PB3 打印口不发送
  -> PA9/PA10 按 LCDM USART1 初始化，连续 3-5 次发送 LCDM 握手
     -> 有 LCDM 回应：display=LCDM，PA9/PA10 保持 LCDM USART1
     -> 无 LCDM 回应：display=LEDM，PA9/PA10 切换为 LEDM/TM1637 GPIO
  -> 读取 SPI NOR/内部 Flash 中的 device_role
     -> role=TESTER_BASIC：初始化 4051 扫描、LEDM、按键/测试流程
     -> role=TESTER_LCDM：初始化 4051 扫描、LCDM、按键/测试流程
     -> role=PRINT_HOST：初始化 LCDM、PB5/PB3 打印机通讯、WiFi/MAS、本地缓存
     -> role 未配置或 CRC 错误：进入未配置安全模式
  -> 未配置安全模式下只允许通过 DEBUG_TTL、LCDM 维护页、WiFi 维护命令或本线打印主机绑定流程写入角色
```

判定规则：

| 项目 | 规则 |
|---|---|
| 角色来源 | 优先读本机 Flash 配置：`TESTER_BASIC`、`TESTER_LCDM`、`PRINT_HOST`、`UNCONFIGURED` |
| 显示类型 | 上电先检测 LCDM；有回应为 LCDM，无回应为 LEDM/TM1637 |
| 配置写入 | 出厂治具、DEBUG_TTL、LCDM 维护页、WiFi 维护命令，或由本线打印主机在对码/换机时下发 |
| 未配置处理 | 不启动自动测试，不发送打印命令；蜂鸣器可按错误节奏提示，界面显示待配置 |
| 角色锁定 | 上电读取配置后本次运行不自动切换角色；需要维护命令或重启后才改变 |
| 打印安全 | 只有 `PRINT_HOST` 才允许初始化打印协议；测试机模式下 PB5/PB3 不发送任何 ZPL、TSPL、ESC/POS 或可被打印机执行的打印命令 |
| 误插处理 | 角色与外设不匹配时只报对应错误，不回退成其它角色 |

结论：当前不建议增加专用角色检测脚。原因是已经有 SPI NOR Flash 和 WiFi/DEBUG/LCDM 维护路径，角色属于设备配置，不属于必须用硬件脚判断的实时状态。若量产希望完全免配置，可从 `PA7/PA8/PC13/PA15` 另选 1-2 个 BOM ID 电阻脚，但这会增加 PCB 和装配复杂度，第一版不作为必需项。

## 蜂鸣器输出

| 功能 | MCU 引脚 | LQFP100 脚号 | 网络名 | 电平 | 说明 |
|---|---|---:|---|---|---|
| 2 kHz 蜂鸣器驱动 | `PB4` | 90 | `BUZZER_2K` | 高电平有效 | 由 `backup05` 改为蜂鸣器控制脚，默认低电平关闭 |

硬件要求：

- `PB4` 只作为控制信号，不建议 MCU 直接带蜂鸣器大电流；用 NPN 三极管或 NMOS 驱动 2 kHz 有源蜂鸣器。
- 若为有源蜂鸣器，`PB4=1` 即响；若为无源蜂鸣片，需要 PWM 产生 2 kHz，本规划当前按有源蜂鸣器处理。
- 蜂鸣器电源若为 5 V，驱动管与 AT32 GPIO 之间必须保证 3.3 V 可完全驱动，且不要让 5 V 回灌到 PB4。

提示节奏：

| 事件 | 蜂鸣器节奏 |
|---|---|
| PASS | 长 BEEP：响 1.0 s，停 1.0 s；按界面/流程需要可重复或只执行一轮 |
| NG | 短响：响 0.5 s，停 0.5 s，响 0.5 s，停 1.0 s；按界面/流程需要可重复或只执行一轮 |

## 感应、IR、按键

| 功能 | 07-07-N 引脚 | 结论 |
|---|---|---|
| 感应触发输入 | `PB8=HALL_SW` | 已确认，PC7 不再用于触发 |
| IR 发射 | `PB6=IR_TX` | 已确认，建议外接三极管/MOSFET 驱动 IR LED |
| IR 接收 | `PB7=IR_RX` | 已确认，接 3.3 V 解调红外接收头 |
| 按键 | `PC4=KEY_ESC`、`PC5=KEY_OK`、`PC6=KEY_RIGHT`、`PC7=KEY_LEFT`、`PC8=KEY_DOWN`、`PC9=KEY_UP` | 已确认 |

所有传感器/外部输入必须确认 3.3 V 兼容；若来自 5 V 开漏/TTL，必须加分压、限流、钳位或隔离。

## 电源、ADC、时钟、调试

| 项目 | 要求 |
|---|---|
| VBAT | 不使用电池 RTC 时也不能悬空；建议接 3.3 V/VDD，可用 0R 选项并就近去耦 |
| VDDA/VSSA | `VDDA` 接模拟 3.3 V，`VSSA` 接模拟地，旁路电容靠近 MCU |
| VREF+/VREF- | `VREF+` 接稳定模拟 3.3 V/VDDA，`VREF-` 接 VSSA/GND；不能悬空 |
| ADC | `PA0/PA2` 已用于 ADC 输入；ADC 乱跳时优先检查 VREF、VDDA、输入阻抗、RC 滤波和外部保护 |
| 主晶振 | 07-07-N 有 `X1=8MHz` |
| RTC 晶振 | 07-07-N 有 `X2=32.768kHz`，若用 RTC 时间应保留 |
| SWD | `PA13/PA14/NRST` 必须保留到 SWD 接口 |
| BOOT0 | 默认下拉到 GND，留测试/跳帽方式进入下载 |

## PCB 前必须修正/确认

| 序号 | 项目 | 状态 | 处理建议 |
|---:|---|---|---|
| 1 | Flash 网络拼写 | 已修正 | 07-07-N 已为 `FLASH_CS_N/FLASH_WP_N` |
| 2 | `PA9/PA10` 显示共用接插件 | 必须确认 | PCB 只保留一个显示共用接插件；普通测试机接 LEDM，高档测试机/打印主机接 LCDM |
| 3 | `PB5/PB3` 打印机通讯接口 | 必须确认 | 打印主机用该接口接打印机串口/RS232/RS485 转换器 |
| 4 | `WIFI_EN/WIFI_BOOT` | 待确认 | 当前不占 AT32；若需要 AT32 控制复位/下载，必须从已确认备用脚另行分配 |
| 5 | Flash `HOLD#/IO3` | 待视觉复核 | 若未接 MCU，必须上拉到 3.3 V |
| 6 | VBAT | 必须确认 | 无电池时接 3.3 V，不能悬空 |
| 7 | VREF/VDDA/VSSA | 必须确认 | 按模拟电源/参考电压接法去耦，不能悬空 |
| 8 | WiFi 电源 | 必须确认 | 3.3 V 峰值能力和去耦足够，避免影响 ADC |
| 9 | 外部输入电平 | 必须确认 | 所有传感器、CAN/USART 转接、HALL、IR_RX 均不能给 MCU 5 V |
| 10 | CAN 接口 | 待确认 | `PA11/PA12` 已到 CAN 网络；确认收发器、电源、终端电阻、ESD |
| 10 | USART 接口 | 待确认 | `J7=USART` 当前看起来是直接 4 pin 串口；若需要 RS485，必须补收发器和 DE/RE 方案 |
| 11 | 旧文档 | 必须更新 | 所有写 `SCH_FIXTURE_2026-07-05.pdf` 或旧 `SCH_FIXTURE_2026-07-07.pdf` 为最终依据的文档改为 07-07-N 或标为历史 |

## 当前可作为备用的脚

这些脚在 07-07-N 已有 `backup` 网络，但使用前仍要结合调试、下载、启动、电气位置再定：

| 备用网络 | MCU 引脚 | 脚号 | 备注 |
|---|---|---:|---|
| `backup01` | PA7 | 32 | 普通备用；旧 IR 测试曾用 PA7，当前 07-07 已不用 |
| `backup02` | PA8 | 67 | 普通备用；如使用 MCO/定时器功能需复核 |
| `backup03` | PC13 | 7 | 低速/RTC 区域脚，不适合高速或大电流 |
| `backup04` | PA15 | 77 | JTAG 相关脚，使用前确认 SWD/JTAG 配置 |
| `backup05` | PB4 | 90 | 已改为 `BUZZER_2K`，不再作为普通备用 |

## Layout 提醒

- 4051 模拟路径与 WiFi、IR LED、USART/CAN、开关电源、大电流线保持距离。
- `PA0/PA2/VREF+/VDDA` 区域优先按模拟前端处理，走线短、加 RC/保护预留。
- WiFi 模块和天线远离 DB 接插件金属、大电流线束和模拟输入。
- SPI Flash 靠近 MCU，SCK/MOSI/MISO 可串 22R/0R，CS 上拉。
- SWD、DEBUG_TTL、WiFi UART、USART、IR、HALL 都保留测试点，方便 layout 后首板调试。
