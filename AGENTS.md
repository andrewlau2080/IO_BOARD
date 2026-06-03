# AGENTS.md

## Resume Entry

To continue this exact development session next time, open a terminal and run:

```sh
cd /Users/andrewlau/qtprj/ARTERY
codex resume 019e39f9-4427-73a2-9fa5-8a894b670626
```
/Users/andrewlau/qtprj/ARTERY/AGENTS.md
Fallback if this is still the latest session:

```sh
cd /Users/andrewlau/qtprj/ARTERY
codex resume --last
```

Ubuntu project mirror:

```sh
ssh andrew@10.211.55.4
cd /home/andrew/ARTERY
```

## Project Context

This workspace is for ARTERY AT32F455 IO board firmware development.

Primary application project:

- `IO_BOARD/`

Current IO scan and Raspberry Pi protocol design references:

- `IO_BOARD/docs/io_scan_mapping.md`: logical OUT/IN position codes, mux bank
  mapping, DB50 connector mapping, and DB78 connector mapping.
- `IO_BOARD/docs/rpi_protocol.md`: Raspberry Pi binary frame format, command
  IDs, payload definitions, response definitions, and LED seven-segment display
  command reservation.
- `IO_BOARD/docs/io_program_design_summary.md`: module summary, scan data
  model, profile capacity table, and implementation status.
- `IO_BOARD/docs/led7seg_module_selection.md`: LED seven-segment display module
  selection; current default is two 3.3 V-capable TM1637 4-digit modules.
- `IO_BOARD/docs/production_line_terminal_model.md`: production-line terminal
  and print model; old first-gen communication protocol placeholders.
- `terminal/print_terminal_mockup.html`: browser mockup for terminal label
  editing, preview, and queue.

Hardware connector variants are separated under:

- `IO_BOARD/hardware/old_db50/`: legacy-compatible DB50 design. Four DB50
  connectors, each connector has upper/lower corresponding rows, 24 OUT/IN
  pairs per connector, row pin 25 reserved/NC. Total used points:
  96 OUT + 96 IN. Remaining mux points 97-128 are unused.
- `IO_BOARD/hardware/db78_64x4/`: DB78 design. Two DB78 connectors for OUT and
  two DB78 connectors for IN, 64 pins used per DB78. Total used points:
  128 OUT + 128 IN.

Keep circuit/BOM/layout files for each hardware variant in its own hardware
directory. Keep firmware shared in the `IO_BOARD` root (`src/`, `inc/`,
`CMakeLists.txt`) and select connector behavior by board profile/mapping table
instead of duplicating program directories.

Display/communication planning:

- The old single-MCU DB50 board keeps a local LED seven-segment display. It uses
  the same planned communication/display MCU resource as the Raspberry Pi port,
  populated as `J_LED7SEG` / LED display driver instead of a Pi connector.
- Current default display module choice for the old single-MCU DB50 board is two
  TM1637 4-digit LED modules powered from 3.3 V. Use one shared CLK plus
  separate DIO lines, or independent CLK/DIO pairs if the layout is simpler.
- Current TM1637 bench test firmware reuses AT-START-F455 J8 USART1 pins as
  GPIO, not as UART: `PA9/CTRL_USART1_TX` = TM1637 `CLK`,
  `PA10/CTRL_USART1_RX` = TM1637 `DIO`, plus 3.3 V and GND. It drives a
  six-digit module at TM1637 addresses `0xC0` ... `0xC5`.
- Current TM1637 application simulation maps the four module keys as:
  `0xF3` self-test, `0xF4` auto-test, `0xF5` restart, `0xF0` reset. Self-test
  and auto-test both display `A01b01` through `A92b92`; self-test ends with
  `StPASS`, auto-test ends with `AUPASS`.
- The Raspberry Pi build connects that same resource to `J_RPI_COMM`; LCDM or
  screen output is driven by Raspberry Pi, not directly by AT32.
- Do not require both LED seven-segment display and Raspberry Pi communication
  on the same MCU peripheral at the same time; select by BOM/board profile.

Production line / terminal planning:

- There can be up to 10 tester stations. Each station runs local board logic,
  then sends a print request when the rotating line reaches the terminal sensor
  position.
- The printer-side polling/trigger packet has been decoded from
  `TestV1.0/analysis_outputs/useful_printer_trigger_tx_envelope.csv`.
  `IO_BOARD/src/line_comm_bridge.c` contains this 71-segment packet as
  `LINE_COMM_CODE_PRINT_REQUEST`.
- Current firmware main loop continuously transmits this printer-side polling
  packet from `PA7` with about 41.7 kHz carrier and about 203.147 ms repeat
  period. Other old-link response/ack code slots are still placeholders.
- Printer terminal should be Raspberry Pi/Linux based. Use CUPS/IPP/vendor
  drivers to support broad market printers; direct MCU printer driving is only
  for fixed printer languages such as ESC/POS, TSPL, ZPL, or CPCL.
- `IO_BOARD/inc/print_job_model.h` and `IO_BOARD/src/print_job_model.c` define
  the first ASCII print label data model.

Mux/electrical planning:

- Use 74HC4051 analog muxes powered from 3.3 V for both hardware variants.
- AT32 3.3 V GPIO can directly drive 74HC4051 address/enable control lines in
  this 3.3 V powered arrangement; no TTL level translator is required for these
  mux control signals.
- Do not power 74HC4051 from 5 V and directly drive its control pins from AT32
  3.3 V GPIO for production design; 3.3 V is not a guaranteed high level for
  every 5 V HC CMOS condition.
- Any external 5 V test stimulus must be limited, divided, clamped, or handled
  by a separate tolerant front end before reaching a 3.3 V powered 74HC4051 or
  AT32 pin.

Target MCU and board:

- MCU: `AT32F455VET7`
- Package: LQFP100
- Hardware planning sources:
  - `IO_BOARD/AT32F455VET7_Fixture.ATWP`
  - `IO_BOARD/at32f_fixture250724.xlsx`
  - `IO_BOARD/SCH_FIXTURE_2025-07-18.pdf`

## Development Approach

Use the CMake project in `IO_BOARD` as the main source of truth. Keep it portable
between macOS and Ubuntu.

Build commands:

```sh
cd IO_BOARD
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

Expected build outputs:

- `IO_BOARD/build/io_board_at32f455.elf`
- `IO_BOARD/build/io_board_at32f455.hex`
- `IO_BOARD/build/io_board_at32f455.bin`

## Toolchain Notes

On macOS, the Homebrew `arm-none-eabi-gcc` package did not include newlib, so
the project prefers the local xPack toolchain at:

- `.tools/xpack-arm-none-eabi-gcc`

On Ubuntu ARM64, install:

```sh
sudo apt install -y cmake ninja-build gcc-arm-none-eabi openocd usbutils
```

Install Linux USB access rules for AT-Link/CMSIS-DAP probes:

```sh
sudo install -m 0644 tools/udev/60-atlink-cmsis-dap.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Ubuntu is running in Parallels at:

- User: `andrew`
- Host/IP: `10.211.55.4`
- Project copy: `/home/andrew/ARTERY`

## Flashing And Debug

AT-Link/ICE enumerates as:

- USB: `2e3c:f000 Artery Technology CMSIS-DAP`
- Serial: `913575030040A0401D149407`
- Ubuntu device examples: `/dev/hidraw4`, `/dev/ttyACM0`

Mac Homebrew OpenOCD does not include the `artery` flash driver. The upstream
xPack OpenOCD can connect to AT32F455 over SWD, but its `artery` flash driver
does not identify this AT32F455 device for programming. Use pyOCD with the
official ARTERY CMSIS-Pack for flashing.

Installed Ubuntu tools:

- `/home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/bin/openocd`
- `/home/andrew/ARTERY/.tools/pyocd-venv/bin/pyocd`
- `/home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack`

Before flashing from Ubuntu, verify the AT-Link/ICE is visible:

```sh
lsusb
```

If it is not visible, connect the USB device to the `Ubuntu Linux` VM from
Parallels Desktop's Devices > USB & Bluetooth menu.

Flash from Ubuntu with:

```sh
cd /home/andrew/ARTERY/IO_BOARD
. /home/andrew/ARTERY/.tools/pyocd-venv/bin/activate
pyocd flash \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  -t at32f455vet7 \
  -f 1000000 \
  --erase sector \
  build/io_board_at32f455.elf
pyocd reset \
  --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
  -t at32f455vet7 \
  -f 1000000
```

The current LED test firmware runs forever. It prints one status line at every
pattern change on USART1 at 115200 baud:

```sh
ssh andrew@10.211.55.4
stty -F /dev/ttyACM0 115200 raw -echo -echoe -echok -echoctl -echoke
cat /dev/ttyACM0
```

VS Code task:

- `monitor-ubuntu-uart`

OpenOCD SWD connection test:

```sh
cd /home/andrew/ARTERY/IO_BOARD
/home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/bin/openocd \
  -s /home/andrew/ARTERY/IO_BOARD \
  -s /home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/openocd/scripts \
  -f openocd/atlink-cmsis-dap.cfg \
  -c "init; targets; reset halt; shutdown"
```

OpenOCD GDB debug server:

```sh
cd /home/andrew/ARTERY/IO_BOARD
/home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/bin/openocd \
  -s /home/andrew/ARTERY/IO_BOARD \
  -s /home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/openocd/scripts \
  -c "bindto 0.0.0.0" \
  -f openocd/atlink-cmsis-dap.cfg \
  -c "gdb_memory_map disable" \
  -c "gdb_flash_program disable"
```

GDB client from Ubuntu:

```sh
cd /home/andrew/ARTERY/IO_BOARD
gdb -q build/io_board_at32f455.elf -x debug/at32f455.gdb
```

VS Code launch configurations:

- `Debug AT32F455 via Ubuntu OpenOCD`: for macOS VS Code using Ubuntu GDB over
  SSH.
- `Debug AT32F455 on Ubuntu Local OpenOCD`: for VS Code running inside Ubuntu.

Keep breakpoints in flash as hardware breakpoints; the configurations map flash
as read-only so GDB auto-selects hardware breakpoints.

Use macOS VS Code as the main interactive debug UI for this project:

```sh
code /Users/andrewlau/qtprj/ARTERY/IO_BOARD
```

Set breakpoints in `led_pattern_apply()` or `led_pattern_run_once()` to see the
running source location and watch `g_led_pattern`, `g_led_phase`, `g_led_cycle`,
and `g_blink_counter`.

The ICE/AT-Link `LD1-LD4` LEDs are AT-Link-EZ status LEDs controlled by the
debugger MCU, not by the AT32F455 target application. The target firmware maps
the requested sequence as:

- `0x01`: target `LED2` / `PD13`
- `0x02`: target `LED3` / `PD14`
- `0x04`: target `LED4` / `PD15`
- `0x08`: fourth logical step visible through `PH3 DEBUG_OUT` and debugger
  variables because the AT-START-F455 BSP has no target-controlled `LED1`

It repeats those one-second steps three times, then flashes `LED2-LED4`
together three times with `g_led_pattern = 0x0F / 0x00`.

The current Ubuntu VM is ARM64. ARTERY official AT32 IDE / AT-Link desktop
tools are not the primary workflow here because official Linux GUI/tooling is
typically x86_64 and does not use this CMake project directly. Use VS Code +
CMake + pyOCD/OpenOCD as the source-of-truth workflow; use official ARTERY
software from Windows or an x86_64 Ubuntu VM if required.

Current safe firmware behavior:

- Initializes IOBOARD pins from the WorkBench/Excel pin plan.
- Keeps all CD4051 inhibit/enable lines disabled by default.
- Configures PC4-PC9 buttons as pull-up inputs.
- Provides shared IO scan/protocol framework:
  - `IO_BOARD/inc/io_scan.h` and `IO_BOARD/src/io_scan.c` define `OUT001` ...
    `OUT128`, `IN001` ... `IN128`, profile validation, mux bank selection, and
    full matrix scan storage.
  - `IO_BOARD/inc/rpi_protocol.h` and `IO_BOARD/src/rpi_protocol.c` define the
    Raspberry Pi binary frame codec and CRC16.
  - `io_scan_measure_selected_pair()` is a weak measurement hook; replace it
    with the final hardware sense implementation when the detection circuit is
    fixed.
- Current test application is closed-loop tester-side IR receive/transmit:
  - `PA6` = `IR_RX`, LQFP100 physical pin 31, input pull-up for demodulated IR
    receiver output.
  - `PA7` = `IR_TX`, LQFP100 physical pin 32, carrier burst output for an IR LED
    driver.
  - It waits for the first 12 MARK/SPACE segments of
    `LINE_COMM_CODE_PRINT_REQUEST` from the printer side.
  - After a valid printer polling prefix, it starts the tester response at about
    24.786 ms from the detected polling packet start, matching the old-machine
    timing model.
  - It transmits `LINE_COMM_CODE_TESTER_RESPONSE` once, then guards IR TX low
    for 100 ms and waits for the next polling packet.
  - The tester response timing table is generated during CMake build from
    `TestV1.0/analysis_outputs/useful_tester_response_tx_envelope.csv`.
  - `LINE_COMM_CODE_TESTER_RESPONSE` has 3321 MARK/SPACE segments and lasts
    about 2.151 s.
  - The standalone test uses 13 us carrier half-period, about 38.5 kHz.
  - `PH3 DEBUG_OUT` is a packet envelope debug pin: high during the response
    packet and low after completion.
  - Watch `g_tester_response_tx_counter`, `g_tester_response_segment_count`,
    `g_tester_response_code_us`, `g_tester_response_ready`,
    `g_tester_response_sent`, `g_printer_poll_rx_counter`,
    `g_printer_poll_match_counter`, `g_printer_poll_reject_counter`,
    `g_printer_poll_rx_segment_count`, `g_tester_response_waiting_for_poll`,
    and `g_ir_carrier_half_us`.
  - Previous printer-side polling test:
    `LINE_COMM_CODE_PRINT_REQUEST` continuously transmitted the printer
    polling/trigger packet from `TestV1.0`.
  - Verified on 2026-06-03 with oscilloscope: printer-side polling IR transmit
    is normal. `PA7` outputs packeted MARK carrier bursts and SPACE gaps, not a
    continuous carrier.

Do not use AT-START onboard LED assumptions for this IO board. Pins used as
AT-START LEDs are assigned to IO board mux enable signals.

## Editing Rules

- Keep board-specific pin definitions in `IO_BOARD/inc/io_board.h` and
  `IO_BOARD/src/io_board.c`.
- Keep generated/vendor BSP code under `.vendor/` unchanged unless there is a
  clear reason.
- Preserve the CMake build path and output names because VS Code tasks rely on
  them.
