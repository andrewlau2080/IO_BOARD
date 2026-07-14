# WiFi 模块 PCB 连接规格

本文用于当前测试机和打印主机 PCB 设计。参考 `docs/esp32-c3-wroom-02_datasheet_cn.pdf` 中 ESP32-C3-WROOM-02 / ESP32-C3-WROOM-02U 的引脚、封装、推荐 PCB Land Pattern 和天线布板要求。

当前 PCB 引脚基准以 `SCH_FIXTURE_2026-07-07-N.pdf` 为准。07-07-N 图纸中的 WiFi 模块型号为 `ESP32-C3-WROOM-02U-N4`；若采购改为 H4 或其它 Flash 容量版本，必须同步更新 BOM、原理图型号和库存编码，但 AT32 侧 UART 连接不变。

## 选型结论

| 位置 | 推荐模块 | 原因 |
|---|---|---|
| 当前 07-07-N 原理图 | `ESP32-C3-WROOM-02U-N4` | 已在原理图中放置，外接天线版本 |
| 打印主机 | 优先 `ESP32-C3-WROOM-02U-*` | 外接天线，车间环境更稳；打印主机位置固定，方便安装天线 |
| 测试机 | `ESP32-C3-WROOM-02-*` 或 `ESP32-C3-WROOM-02U-*` | 空间和外壳允许时优先统一 `02U`；若成本和装配更重要可用板载天线 `02` |
| 暂不优先 | `ESP32-C3-MINI-*`、`ESP8684/ESP32-C2-MINI` | 焊盘更小或底部焊接检查不方便；后续可做降成本版本 |

第一版建议把 WiFi 模块当作 AT32 的网络协处理器。AT32 仍是业务主控，ESP32-C3 只负责 WiFi 连接、TCP/MQTT/HTTP 或 AT 指令通讯。

ESP32-C3-WROOM-02/02U 模块上的 4 MB Flash 不作为打印主机的生产记录缓存来规划。它主要服务 WiFi 模块固件、参数和可能的 OTA/文件系统；本线 10 台测试机在 MAS 断线时产生的测试/打印记录，应保存在 AT32 打印主机侧的独立非易失存储中。打印主机 PCB 建议预留 SPI NOR Flash，16 MB 起步，推荐 32 MB 或 64 MB；如果要求长时间断网保存详细记录，可再预留 microSD/eMMC 方案。

## 推荐连接方式

### AT32 到 ESP32-C3-WROOM-02/02U

第一版 AT32F455VET7 LQFP100 侧先固定 ESP32 模块侧连接方式；AT32 侧 IO 已按 `SCH_FIXTURE_2026-07-07-N.pdf` 和 `SCH_FIXTURE_2026-07-07_IO核对与PCB检查表.md` 复核。

### 新版 AT32 与 WiFi 连接规划表

当前推荐按“最小可靠连接”画 PCB：AT32 只占用 WiFi UART 的 TX/RX 两根线；ESP32 的 `EN`、`IO9/BOOT` 用上拉、按键/测试点处理，不强制占用 AT32 IO。`PB13/PB14/PB15/PA6` 已用于打印主机 SPI NOR Flash，不能再作为 WiFi 备用脚。2026-07-09 起 `PB4` 已改为 `BUZZER_2K`，不再作为 WiFi 或低速控制备用脚；`PA7/PA8/PC13/PA15` 只作为后续低速控制备用脚池记录，不直接改变第一版 WiFi 主连接。

| 序号 | WiFi 功能 | AT32F455VET7 引脚 | LQFP100 脚号 | ESP32-C3-WROOM-02/02U 脚位 | 连接方式 | 当前结论 |
|---:|---|---|---:|---|---|---|
| 1 | `AT32_WIFI_TX` / `WIFI_TX` | `PC3` | 18 | Pin 11 `RXD` | AT32 -> ESP32，串 0R/22R | 07-07-N 已确认 |
| 2 | `AT32_WIFI_RX` / `WIFI_RX` | `PB9` | 96 | Pin 12 `TXD` | ESP32 -> AT32，串 0R/22R | 07-07-N 已确认 |
| 3 | `WIFI_EN` | 不接 AT32 | - | Pin 2 `EN` | `10k` 上拉到 `3V3_WIFI`，RESET 测试点/按键可拉 GND | 第一版推荐；不占 MCU IO |
| 4 | `WIFI_BOOT` | 不接 AT32 | - | Pin 8 `IO9` | `10k` 上拉到 `3V3_WIFI`，BOOT 测试点/按键可拉 GND | 第一版推荐；不占 MCU IO |
| 5 | `3V3_WIFI` | 3.3 V 电源 | - | Pin 1 `3V3` | 独立 3.3 V 分支，建议磁珠/0R 预留 | 峰值能力按 >= 500 mA 设计 |
| 6 | `GND` | 系统地 | - | Pin 9、Pin 19-25 `GND` | 多点接地，内部 GND 焊盘按规格书 land pattern | 必须接地，19-25 底部焊盘按回流焊设计 |

不能使用以下脚接 WiFi：`PA1`、`PA3`、`PA2`、`PA6`、`PD8`、`PA9/PA10`、`PB3/PB5`、`PB6/PB7`、`PB8`、`PB13/PB14/PB15`、`PA13/PA14`、`PH2/PH3`。`PA15` 在 07-07-N 图纸为 `backup04`，但涉及 JTAG 复用，谨慎作为备用脚记录，不作为第一优先。

### 可备用脚池

| AT32 引脚 | LQFP100 脚号 | 核查状态 | 备用建议 |
|---|---:|---|---|
| `PA7` | 32 | 07-07-N 为 `backup01` | 可备用；注意旧 IR 测试资料曾用 PA7 |
| `PA8` | 67 | 07-07-N 为 `backup02` | 可备用 |
| `PC13` | 7 | 07-07-N 为 `backup03` | 只建议低速控制，注意 RTC/TAMPER 区域 |
| `PA15` | 77 | 07-07-N 为 `backup04` | 谨慎备用；涉及 JTAG 复用 |
| `PB4` | 90 | 2026-07-09 改为 `BUZZER_2K` | 不再作为备用 |
| `PB9` | 96 | 07-07-N 已为 `WIFI_RX` | 当前用于 WiFi RX，不再当普通备用 |

### 当前核查状态

| WiFi 信号 | AT32 引脚 | ESP32-C3-WROOM-02/02U 脚位 | 方向 | 说明 |
|---|---|---|---|---|
| `AT32_WIFI_TX` / `WIFI_TX` | `PC3` | Pin 11 `RXD` | AT32 -> ESP32 | 07-07-N 已确认 |
| `AT32_WIFI_RX` / `WIFI_RX` | `PB9` | Pin 12 `TXD` | ESP32 -> AT32 | 07-07-N 已确认 |
| `WIFI_EN` | 第一版不接 AT32 | Pin 2 `EN` | 手动/硬件复位 | ESP32 侧 10 k 上拉到 `3V3_WIFI`，预留 RESET 测试点/按键 |
| `WIFI_BOOT` | 第一版不接 AT32 | Pin 8 `IO9` | 手动下载控制 | ESP32 侧 10 k 上拉到 `3V3_WIFI`，预留 BOOT 测试点/按键 |
| `3V3_WIFI` | 3.3 V 电源 | Pin 1 `3V3` | 电源 | WiFi 分支供电能力建议 >= 500 mA |
| `GND` | 系统地 | Pin 9、Pin 19-25 `GND` | 电源 | 19-25 为内部 GND 焊盘，按规格书 land pattern 接地 |

本轮不把 `PB13/PB14/PB15/PA6` 写成 WiFi 脚，因为它们已规划给 SPI NOR Flash。`PB4` 已规划给 2 kHz 蜂鸣器，不再作为备用。`PA7/PA15` 仍只作为谨慎备用脚池。`WIFI_EN/WIFI_BOOT` 直接上拉是可行的，第一版不强制占用 MCU。

不使用这些脚连接 WiFi：`PA1=DEBUG_TTL_RX`、`PA3=DEBUG_TTL_TX`，`PA2` 已用于 `ADC2_IN2`，`PA6/PB13/PB14/PB15` 已用于 SPI NOR Flash，`PB4=BUZZER_2K`，`PD8` 保留给 `OUT_BMUX_EN0`，`PH2/PH3` 不写入 LQFP100 最终规格，`PA9/PA10` 留给 LEDM/LCDM 显示共用口，`PB3/PB5` 留给打印主机打印机通讯口，`PB6/PB7` 留给 IR，`PB8` 留给 `HALL_SW`，`PC4..PC9` 留给按键。

### 本轮禁止/待核定脚位

| AT32 引脚 | 当前状态 | 结论 |
|---|---|---|
| `PA1` | 07-07-N 为 `DEBUG_TTL_RX` | 禁止作为 WiFi 或 RS485 方向脚 |
| `PA3` | 07-07-N 为 `DEBUG_TTL_TX` | 禁止作为 WiFi |
| `PD8` | 07-07-N 为 `OUT_BMUX_EN0` | 禁止作为 WiFi |
| `PB9` | 07-07-N 为 `WIFI_RX` | 当前固定为 WiFi RX |
| `PB13` | 07-07-N 为 `FLASH_SPI_SCK` | 已规划 SPI NOR，不再给 WiFi |
| `PB14` | 07-07-N 为 `FLASH_SPI_MISO` | 已规划 SPI NOR，不再给 WiFi |
| `PB15` | 07-07-N 为 `FLASH_SPI_MOSI` | 已规划 SPI NOR，不再给 WiFi |
| `PA6` | 07-07-N 为 `FLASH_CS_N` | 已规划 SPI NOR CS，不再给 WiFi |
| `PA7` | 07-07-N 为 `backup01` | 备用 GPIO，注意旧 IR 测试资料曾用 |
| `PA15` | 07-07-N 为 `backup04` | 不能先写成 `WIFI_BOOT`，如需使用先确认 JTAG/SWD 配置 |
| `PC3` | 07-07-N 为 `WIFI_TX` | 当前固定为 WiFi TX |

### ESP32 侧连接表

| 信号 | 方向 | ESP32-C3-WROOM-02/02U 脚位 | AT32 侧 | PCB 设计要求 |
|---|---|---|---|---|
| `3V3_WIFI` | 电源 | Pin 1 `3V3` | 3.3 V 电源 | 单独从 3.3 V 电源分支供电，建议供电能力 >= 500 mA |
| `GND` | 电源 | Pin 9、Pin 19-25 `GND` | 系统地 | 多点接地，内部 GND 焊盘按规格书 land pattern 做 |
| `AT32_WIFI_TX` | AT32 -> ESP | Pin 11 `RXD` | `PC3` | 串口交叉连接，可串 0R/22R 预留 |
| `AT32_WIFI_RX` | ESP -> AT32 | Pin 12 `TXD` | `PB9` | 串口交叉连接，可串 0R/22R 预留 |
| `WIFI_EN` | 手动/硬件复位 | Pin 2 `EN` | 不接 AT32 | 10 k 上拉到 `3V3_WIFI`，预留 RESET 测试点/按键，不要悬空 |
| `WIFI_BOOT` | 手动下载控制 | Pin 8 `IO9` | 不接 AT32 | 10 k 上拉到 `3V3_WIFI`，预留 BOOT 测试点/按键 |
| `WIFI_STATUS` | ESP -> AT32 | 可选 GPIO | 不分配固定 AT32 脚 | 第一版不接 MCU，可留 ESP 测试点或状态 LED |
| `WIFI_WAKE` | AT32 -> ESP | 可选 GPIO | 不分配固定 AT32 脚 | 第一版不接 MCU，可留 0R/DNP |
| `USB_D-` | 下载/调试可选 | Pin 13 `IO18` | USB D- 测试点/接口 | 建议预留测试点，不一定接主板 USB |
| `USB_D+` | 下载/调试可选 | Pin 14 `IO19` | USB D+ 测试点/接口 | 建议预留测试点，不一定接主板 USB |

常用脚位速查：

| Pin | 名称 | 本项目建议 |
|---:|---|---|
| 1 | `3V3` | 接 `3V3_WIFI` |
| 2 | `EN` | 10 k 上拉到 `3V3_WIFI`，预留 RESET 测试点/按键到 GND；第一版不接 AT32 |
| 8 | `IO9` | 10 k 上拉到 `3V3_WIFI`，预留 BOOT 测试点/按键到 GND；第一版不接 AT32 |
| 9 | `GND` | 接系统地 |
| 11 | `RXD` | 接 AT32 `PC3 / AT32_WIFI_TX` |
| 12 | `TXD` | 接 AT32 `PB9 / AT32_WIFI_RX` |
| 13 | `IO18` | 可选 USB D- 测试点 |
| 14 | `IO19` | 可选 USB D+ 测试点 |
| 19-25 | `GND` | 底部/内部 GND 焊盘，必须按 land pattern 接地 |

最小可工作连接：

```text
AT32 PC3 / AT32_WIFI_TX -> ESP32 RXD0 / Pin 11
AT32 PB9 / AT32_WIFI_RX <- ESP32 TXD0 / Pin 12
WIFI_EN   --10k上拉到3V3_WIFI，RESET_TP/按键可拉GND
WIFI_BOOT --10k上拉到3V3_WIFI，BOOT_TP/按键可拉GND
3V3_WIFI      -> ESP32 3V3
GND           -> ESP32 GND
```

## EN / BOOT / 下载逻辑

ESP32-C3 正常从 Flash 启动时，`IO9/BOOT` 保持高电平；进入下载模式时，先把 `IO9/BOOT` 拉低，再复位 `EN`。

ESP32-C3 有启动配置脚，PCB 上不要给 `IO2`、`IO8`、`IO9` 外加强下拉或会改变上电状态的电路。第一版只把 `IO9` 作为手动 BOOT 控制脚，使用 10 k 上拉和测试点/按键拉低；其它启动相关脚若不用，按规格书默认状态处理。

推荐电路：

```text
3V3_WIFI -- 10k -- WIFI_EN  -- ESP32 EN
RESET_TP/按键 -------- WIFI_EN  -- 手动拉低复位

3V3_WIFI -- 10k -- WIFI_BOOT -- ESP32 IO9
BOOT_TP/按键 ---------- WIFI_BOOT -- 手动拉低进入下载模式
```

建议同时预留手动调试焊盘或按键：

```text
WIFI_EN   -> 测试点 / RESET 按键到 GND
WIFI_BOOT -> 测试点 / BOOT 按键到 GND
TXD0/RXD0 -> 测试点，方便 USB-UART 直接烧录
IO18/IO19 -> USB Serial/JTAG 测试点，可选
```

第一版如果 ESP32 使用 AT 固件或自定义固件，量产时通过 `BOOT + EN` 测试点/按键或治具进入下载模式；当前不占 AT32 GPIO 控制下载。

## 电源设计

WiFi 发射瞬间电流较大，不能把 ESP32-C3 当普通小电流逻辑芯片处理。

| 项目 | 建议 |
|---|---|
| 3.3 V 能力 | WiFi 分支建议预留 >= 500 mA 峰值能力 |
| 去耦 | 模块 3V3 附近放 `10 uF + 0.1 uF`，可再加 `22 uF` 或 `47 uF` 储能 |
| 走线 | 3V3_WIFI 尽量短而宽，避免和 ADC/模拟采样前端共用细长电源线 |
| 地 | 模块 GND 直接回主地平面，内部 GND 焊盘通过过孔连接地平面 |
| 干扰 | 模块远离继电器、电机驱动、大电流开关、打印机电源输入和 ADC 高阻节点 |

如果测试机和打印主机共用一套 3.3 V 电源，建议在 WiFi 分支前加磁珠或 0R 预留位，方便后续按 EMC 实测调整。

## 19-25 内部 GND 焊盘处理

WROOM-02/02U 的内部 GND 焊盘在模块底部，手工烙铁无法直接焊到。PCB 设计按下面处理：

| 项目 | 要求 |
|---|---|
| Land Pattern | 按规格书推荐 PCB Land Pattern，不要自行缩小内部 GND 焊盘 |
| 钢网 | 内部 GND 建议分窗开膏，避免锡量过多把模块顶高 |
| 过孔 | 内部 GND 焊盘打小过孔到地平面；优先塞孔/盖油，避免吸锡 |
| 可维修性 | 可把 GND 铜皮向外侧适当延伸形成可见焊锡边，但不要破坏官方焊盘间距 |
| 打样 | 若手工焊接困难，优先做 WiFi 子板，由 SMT 厂贴好模块 |

如果主板要方便维修，推荐 WiFi 子板方案：

```text
主板 J_WIFI:
3V3_WIFI
GND
PC3 / AT32_WIFI_TX -> ESP_RXD
PB9 / AT32_WIFI_RX <- ESP_TXD
WIFI_EN 上拉/RESET_TP，不占 AT32
WIFI_BOOT 上拉/BOOT_TP，不占 AT32
WIFI_STATUS optional
WIFI_WAKE optional
```

WiFi 子板上贴 ESP32-C3-WROOM-02U，主板用排针、FPC 或板对板连接器。这样后续更换 WiFi 模块、返修和天线位置调整都更简单。

## 天线和布局

### 使用 ESP32-C3-WROOM-02U

`02U` 使用外接天线连接器。建议用于打印主机，或金属外壳/干扰较强的测试机。

| 项目 | 要求 |
|---|---|
| 天线 | 2.4 GHz WiFi 天线，连接器型号按模块实际 U.FL/IPEX 规格 |
| 安装 | 天线尽量引到外壳外侧或塑胶窗口位置 |
| 走线 | 不要在天线连接器附近走高速、大电流、开关电源线 |
| 外壳 | 金属外壳内不要直接用板载天线；优先外接天线 |

### 使用 ESP32-C3-WROOM-02

`02` 为板载 PCB 天线版本。必须严格保留天线禁布区：

| 项目 | 要求 |
|---|---|
| 位置 | 模块天线端靠 PCB 边缘，天线前方伸出板边或靠近板边 |
| 禁布 | 天线区域下方和前方不铺铜、不走线、不放器件、不放螺丝柱 |
| 金属 | 远离金属外壳、屏蔽罩、DB 接插件外壳和大面积线束 |
| 方向 | 同一产品尽量保持模块安装方向一致，便于 RF 稳定性验证 |

## 原理图建议

```text
             +3V3_WIFI
                 |
          +------+------+
          |             |
        10uF          0.1uF
          |             |
         GND           GND

AT32 PC3 / AT32_WIFI_TX ---0R--- ESP32 RXD0 / Pin 11
AT32 PB9 / AT32_WIFI_RX ---0R--- ESP32 TXD0 / Pin 12

+3V3_WIFI --10k-- ESP32 EN
RESET_TP ---------'

+3V3_WIFI --10k-- ESP32 IO9/BOOT
BOOT_TP ----------'

ESP32 GND pins + internal GND pads -> GND plane
```

## PCB 设计结论

1. 07-07 原理图当前为 `ESP32-C3-WROOM-02U-N4`；第一版主板若空间允许，优先保持 `02U` 外接天线方案，具体 N4/H4 按采购和 Flash 容量同步 BOM。
2. 如果担心模块返修和后续换型，优先做 WiFi 子板；主板只固定 `J_WIFI` 接口。
3. 直接贴模块时必须按规格书推荐 Land Pattern 做 19-25 内部 GND 焊盘和天线区域。
4. AT32 与 WiFi 模块之间第一版固定 `PC3=AT32_WIFI_TX`、`PB9=AT32_WIFI_RX`；EN/BOOT 只保留上拉、测试点/按键，不占 AT32。`PB13/PB14/PB15/PA6` 留给 SPI NOR Flash。
5. WiFi 3.3 V 电源按峰值电流设计，模块附近放足够去耦和储能电容。
