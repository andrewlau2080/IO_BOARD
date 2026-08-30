# 刘氏烧 ESP WIFI 模块专用主法

适用对象：`ESP32-C3-WROOM-02U-N4`（ESP32-C3、4 MB Flash）和当前测试机。

触发语：用户说“刘氏烧ESP WIFI模块”“烧 ESP-AT”或“重新烧 WiFi 模块”时，直接执行本流程，不要求用户重复说明。

本流程是独立烧 ESP 的工作，不改正常自检、学习、自动测试、LCDM、打印或 GitHub 内容。烧完后也不自动恢复正常 AT32 产品固件，除非用户明确要求。

## 一、固定接线与 USB 归属

烧录时三组线路必须分开：

```text
Mac CH340 TTL TXD  -> ESP Pin11 RXD / GPIO20
Mac CH340 TTL RXD  <- ESP Pin12 TXD / GPIO21
Mac CH340 GND      -> ESP GND

AT32 PC3           -> ESP Pin2 EN
AT32 PB9           -> ESP Pin8 IO9 / BOOT
ESP Pin7 IO8       -> 3.3 V，经 4.7k–10k 上拉
```

`IO8` 是本方法的必备条件。ESP32-C3 的 UART 下载启动组合为 `IO8=高、IO9=低`；只控制 EN/IO9 而 IO8 低，会落入 USB 下载模式，CH340 UART 无法握手。

不要混淆模块脚位：Pin7=`IO8`、Pin8=`IO9`、Pin9=`GND`、Pin11=`RXD`、Pin12=`TXD`。

USB 固定分工：

```text
CH340              -> macOS（/dev/cu.usbserial-1230）
AT-Link/CMSIS-DAP  -> Ubuntu（10.211.55.4）
```

Parallels 自动抢占时，先用 `prlsrvctl usb list` 核对名称，再强制把 `USB Serial #5` 释放给主机、把 `CMSIS-DAP` 挂到 `Ubuntu Linux`。必须分别确认：

```sh
# macOS
ls -l /dev/cu.usbserial-1230

# Ubuntu
ssh andrew@10.211.55.4 'lsusb; /home/andrew/ARTERY/.tools/pyocd-venv/bin/pyocd list'
```

Ubuntu 必须能看到 `2e3c:f000 Artery Technology CMSIS-DAP`；Mac 必须能打开 CH340 串口。

## 一·补、AT32 产品固件烧录：一律走 Ubuntu（2026-08-27 实测铁律）

**烧 AT32 MCU 固件（build-update-tester / build-update-print-host / 任何 pyocd 操作）一律在 Ubuntu 虚拟机执行，不在 macOS 上跑 pyocd 烧录。**

根因：macOS 上 pyocd 对 Artery CMSIS-DAP 探针执行 `usb.util.claim_interface` 必报
`Access denied (insufficient permissions)`（IOKit 系统驱动占用接口；pyusb_v2 后端无法
绕开，hidapi 后端也枚举不到——AT-Link 探针不是 HID 接口）。Mac 上 pyocd 最多只能
`sudo pyocd list` 看到探针，无法 open/flash。

正确流程（三步）：

```sh
# 1) 把探针挂给 Ubuntu（prlsrvctl usb list 核对 System name；407/807 都要这样挂）
prlsrvctl usb attach '121000|2e3c|f000|full|--|913575030040A0401D149407' Ubuntu Linux
prlsrvctl usb list   # 确认 Used-By-Vm: Ubuntu Linux
ssh andrew@10.211.55.4 'lsusb | grep -i 2e3c'   # Ubuntu 必须能看到探针

# 2) 传固件
scp IO_BOARD/build-update-tester/io_board_at32f455.hex andrew@10.211.55.4:/tmp/tester.hex

# 3) Ubuntu 烧录（--erase sector 保留配方区；-f 50000：407 实测 50kHz 稳定，
#    100kHz 报 DAP_TRANSFER response error；under-reset 复位 AT32）
ssh andrew@10.211.55.4 '/home/andrew/ARTERY/.tools/pyocd-venv/bin/pyocd flash \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  --target at32f455vet7 --uid <探针UID> --erase sector --connect under-reset -f 50000 \
  /tmp/tester.hex'

# 4) 复位并确认运行（pyocd reset 有时不触发 MCU，若 PC 不动让用户按硬复位）
ssh andrew@10.211.55.4 '/home/andrew/ARTERY/.tools/pyocd-venv/bin/pyocd commander \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  --target at32f455vet7 --uid <探针UID> -c "reset" -c "status"'
# 期望输出：Core 0 (Cortex-M4): Running
```

探针 UID：T 侧 407=`913575030040A0401D149407`，P 侧 807=`08703D0500C0643C1014A807`。

## 二、烧入临时 AT32 下载辅助程序

本仓库的 `ESP_AT_FLASH_ASSIST` 是专用临时模式。它仅配置开漏 GPIO：

```text
PC3 / EN:   BOOT 低时拉低 EN，随后释放高
PB9 / IO9:  在 EN 释放前拉低，之后释放高
```

它不调用 `io_board_init()`，不启动扫描、LCD、正常 WiFi UART 或产品业务。

在 Mac 构建单独目录，不能覆盖正常构建目录：

```sh
cd /Users/andrewlau/qtprj/IO_BOARD_GITHUB/IO_BOARD
cmake -S . -B build-esp-at-flash-assist -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=ESP_AT_FLASH_ASSIST
cmake --build build-esp-at-flash-assist
scp build-esp-at-flash-assist/io_board_at32f455.{hex,elf} \
  andrew@10.211.55.4:/tmp/esp-at-flash-assist/
```

在 Ubuntu 按刘氏烧录法烧 AT32：

```sh
/home/andrew/ARTERY/.tools/pyocd-venv/bin/pyocd flash \
  -t at32f455vet7 \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  -f 1000000 --erase sector -O connect_mode=under-reset \
  /tmp/esp-at-flash-assist/io_board_at32f455.hex
```

必须从**本次 ELF**读取向量表的 SP 和 PC，并用 `commander` 写入后 `go`，再读回 `0x08000000` 的 8 字节验证。不可沿用旧 ELF 的数值；本次已验证过的辅助程序示例为 `SP=0x20024000`、`PC=0x08000B91`，但每次仍以当前 ELF 为准。

## 三、确认 ESP UART 下载模式

在 ESP 启动辅助程序控制下，Mac 从 CH340 读取 ROM 日志。正确结果必须为：

```text
boot:0x4 (DOWNLOAD(USB/UART0/1))
waiting for download
```

异常诊断固定如下：

| ROM 日志 | 根因 | 处理 |
|---|---|---|
| `boot:0x0 (USB_BOOT)` | IO8 低或浮空 | 将 Pin7 IO8 经 4.7k–10k 上拉到 3.3 V |
| `boot:0xC (SPI_FAST_FLASH_BOOT)` | IO9 高；PB9 未实际接到 Pin8 IO9 | 核对 PB9 到 Pin8 的连续性，不能接 Pin9 GND 或其它脚 |
| ESP 输出会因 PC3 拉低而停止 | PC3 已正确控制 EN | 保持 PC3→Pin2 EN |

正确进入后，必须先做 ROM 握手：

```sh
/tmp/esp-flash-mac-venv/bin/esptool \
  --chip esp32c3 --port /dev/cu.usbserial-1230 --baud 115200 \
  --before no-reset --after no-reset chip-id
```

预期输出为 `Connected to ESP32-C3` 和芯片 MAC。未得到此结果时，不能开始写 Flash。

## 四、官方 ESP-AT 与 UART0 定制参数

使用 Espressif 官方 `ESP32-C3-MINI-1-AT-V4.1.1.0` 的 4 MB 镜像。该镜像适用于同为 ESP32-C3+4 MB 的 WROOM-02U-N4，但它的原始 `mfg_nvs.bin` 默认把 AT 命令口放在 UART1：`GPIO7/6`，不能直接用于本板的 Pin11/12。

必须基于官方 `mfg_nvs.csv` 重建定制 NVS，保留证书和 BLE 配置，仅改动下面参数：

```csv
uart_port,data,i8,0
uart_baudrate,data,i32,115200
uart_tx_pin,data,i32,21
uart_rx_pin,data,i32,20
uart_cts_pin,data,i32,-1
uart_rts_pin,data,i32,-1
```

`mfg_nvs` 分区地址和长度固定为：

```text
地址：0x1F000
长度：0x1F000（126,976 bytes）
```

使用 ESP-IDF 的 `nvs_partition_gen.py` / `esp-idf-nvs-partition-gen` 以 V2 multipage blob 生成定制二进制；不要直接修改 NVS 二进制字节。生成后检查文件大小必须为 `126976` bytes。

## 五、烧录顺序

在 ROM 下载模式中执行：

```sh
ESPTOOL=/tmp/esp-flash-mac-venv/bin/esptool
PORT=/dev/cu.usbserial-1230
IMAGE=/tmp/esp-at-esp32c3/ESP32-C3-MINI-1-AT-V4.1.1.0/ESP32-C3-MINI-1-AT-V4.1.1.0

$ESPTOOL --chip esp32c3 --port "$PORT" --baud 115200 \
  --before no-reset --after no-reset erase-flash

$ESPTOOL --chip esp32c3 --port "$PORT" --baud 115200 \
  --before no-reset --after no-reset write-flash \
  --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x0     "$IMAGE/bootloader/bootloader.bin" \
  0x60000 "$IMAGE/esp-at.bin" \
  0x8000  "$IMAGE/partition_table/partition-table.bin" \
  0xd000  "$IMAGE/ota_data_initial.bin" \
  0x1e000 "$IMAGE/at_customize.bin" \
  0x1f000 "$IMAGE/customized_partitions/mfg_nvs.bin"

# 用上节生成的定制 UART0 版本覆盖 mfg_nvs。
$ESPTOOL --chip esp32c3 --port "$PORT" --baud 115200 \
  --before no-reset --after no-reset write-flash \
  --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x1f000 /tmp/esp-at-uart0/mfg_nvs-uart0.bin
```

每个 `write-flash` 均须看到 `Hash of data verified`。烧录过程保持 `--before no-reset --after no-reset`，避免 CH340 的 DTR/RTS 改写手动 EN/BOOT 时序。

## 六、正常启动与验收

1. 释放 PB9/BOOT 为高。
2. PC3/EN 拉低至少 200 ms 后释放高，使 ESP 正常从 Flash 启动。
3. 在 Mac 115200 8N1 打开 CH340，日志须出现：

```text
at-uart: AT cmd port:uart0 tx:21 rx:20 cts:-1 rts:-1 baudrate:115200
```

4. 必须得到以下双重验收：

```text
AT       -> OK
AT+GMR   -> AT version:4.1.1.0 ... OK
```

本方法在 2026-07-30 已实测：ESP32‑C3 rev v0.4，MAC `f8:5b:1b:ef:1c:5c`，`AT` 和 `AT+GMR` 均成功。

## 七、结束状态和产品恢复

烧 ESP 期间，PC3/PB9 临时用于 EN/BOOT，USB-TTL 是 ESP 的 UART0。要恢复正常产品 WiFi 连线时，必须重新接为：

```text
AT32 PC3 -> ESP Pin11 RXD
AT32 PB9 <- ESP Pin12 TXD
ESP EN / IO9 -> 各自 10k 上拉和测试点
ESP IO8 -> 保持上拉，不得下拉
```

此时再按用户明确指示，重新烧回所需的正常 AT32 产品固件；不得把临时 `ESP_AT_FLASH_ASSIST` 当作产品交付固件。
