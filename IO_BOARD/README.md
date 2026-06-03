# IO_BOARD AT32F455

VS Code/CMake project for the IO board using AT32F455VET7.

## 192-Point Cable Tester Plan

The 192-point cable tester work is now tracked in this ARTERY project instead
of the older ATMEGA8L reference folder. Use `192点线束测试规格.md` as the
current planning entry.

Hardware connector variants are separated under `hardware/`:

- `hardware/old_db50/`: legacy-compatible DB50 design. Four DB50 connectors,
  each connector has upper/lower corresponding rows, 24 pairs per connector,
  row pin 25 reserved, 96 OUT + 96 IN.
- `hardware/db78_64x4/`: DB78 design. Two OUT DB78 and two IN DB78, 64 pins
  used per connector, 128 OUT + 128 IN.

Firmware stays in this project root (`src/`, `inc/`, `CMakeLists.txt`) and
should select the physical mapping by board profile instead of duplicating
program directories.

Program design references:

- `docs/io_scan_mapping.md`: logical OUT/IN point codes, mux banks, and connector mapping.
- `docs/rpi_protocol.md`: Raspberry Pi binary communication frame and command definitions.
- `docs/io_program_design_summary.md`: scan/protocol module summary and implementation status.
- `docs/led7seg_module_selection.md`: LED seven-segment display module selection for the single-MCU DB50 build.
- `docs/production_line_terminal_model.md`: 10-station production-line print terminal model and old-link placeholders.
- `../terminal/print_terminal_mockup.html`: browser mockup for the print terminal label dialog and queue.

Direction:

- Low-cost standalone tester: AT32F455VET7 performs scan, logic comparison,
  LED seven-segment local display, keys, and buzzer.
- Raspberry Pi + MCU tester: AT32F455VET7 remains the scan/IO device, while the
  Raspberry Pi handles LCDM/screen driving, UI, product files, database, cloud
  upload, and printing.
- Both variants should reuse the same scan hardware, point mapping, and low
  level firmware modules wherever possible.

The old single-MCU board uses the same physical communication resource planned
for Raspberry Pi communication as a LED seven-segment display interface. In the
Raspberry Pi build, LCDM is driven by Raspberry Pi, not directly by AT32.

The first firmware target uses the pin plan from `AT32F455VET7_Fixture.ATWP`
and `at32f_fixture250724.xlsx`. On boot it keeps every CD4051 inhibit/enable
line disabled, configures the six keys as pull-up inputs, and toggles
`PH3 DEBUG_OUT` as a safe heartbeat.

The four LEDs marked `LD1-LD4` on the AT-Link/ICE section are AT-Link-EZ status
LEDs controlled by the debugger MCU, not by the AT32F455 application MCU. The
debug blink firmware therefore uses `PH3 DEBUG_OUT` and the watch variables
`g_led_pattern`, `g_led_phase`, `g_led_cycle`, and `g_blink_counter` for
target-side run verification.

Physical/logical LED pattern:

- `g_led_pattern = 0x01`: target `LED2` / `PD13` on for one second.
- `g_led_pattern = 0x02`: target `LED3` / `PD14` on for one second.
- `g_led_pattern = 0x04`: target `LED4` / `PD15` on for one second.
- `g_led_pattern = 0x08`: fourth logical step; visible on `PH3 DEBUG_OUT` and
  in the debugger, because AT-START-F455 BSP has no target-controlled `LED1`.
- This repeats three times.
- Then `g_led_pattern = 0x0F, 0x00` flashes target `LED2-LED4` together three
  times.
- `PH3 DEBUG_OUT` toggles on every pattern update as the physical target-side
  heartbeat.

## Tester Response IR Transmitter

The current firmware is the first closed-loop tester/printer IR model. It waits
for the printer-side polling packet on `PA6`, validates the polling prefix, then
transmits the tester response packet from `PA7`.

Current transmit behavior:

- Input: `PA6` / `IR_RX`, LQFP100 physical pin 31.
- Output: `PA7` / `IR_TX`, LQFP100 physical pin 32.
- Trigger: first 12 MARK/SPACE segments must match
  `LINE_COMM_CODE_PRINT_REQUEST`.
- Timing: response starts about 24.786 ms after the detected printer polling
  packet start, matching the old-machine analysis.
- Packet: 3321 MARK/SPACE segments from `LINE_COMM_CODE_TESTER_RESPONSE`.
- Source timing file:
  `TestV1.0/analysis_outputs/useful_tester_response_tx_envelope.csv`.
- Packet duration: about 2.151 s.
- Carrier half-period: 13 us, about 38.5 kHz.
- Loop behavior: after transmitting, hold IR TX off for 100 ms, then wait for
  the next printer polling packet.
- Scope check: `PA7` shows tester-response carrier bursts only during MARK
  segments; `PH3` stays high during the about 2.151 s response packet.
- Watch variables: `g_tester_response_tx_counter`,
  `g_tester_response_segment_count`, `g_tester_response_code_us`,
  `g_tester_response_ready`, `g_tester_response_sent`,
  `g_printer_poll_rx_counter`, `g_printer_poll_match_counter`,
  `g_printer_poll_reject_counter`, `g_printer_poll_rx_segment_count`,
  `g_tester_response_waiting_for_poll`, and `g_ir_carrier_half_us`.

Previous confirmed printer-side polling transmitter behavior:

- Packet: 71 MARK/SPACE segments from `LINE_COMM_CODE_PRINT_REQUEST`.
- Verification status: confirmed on 2026-06-03. The printer-side polling IR
  transmitter outputs the expected packet envelope, not a continuous carrier.

The older IR learn/replay capture helpers remain in the project for protocol
learning. The main firmware now uses a fast prefix receiver for the printer
polling trigger instead of waiting for the full polling frame to finish.

## Historical IR Learn/Replay Demo

Test pins:

- `PA6`: `IR_RX`, LQFP100 physical pin 31, input with pull-up. Connect to the output pin of a
  demodulated IR receiver module such as VS1838B/IRM type. Common idle level is
  high and received carrier bursts pull the signal low.
- `PA7`: `IR_TX`, LQFP100 physical pin 32, push-pull output. It generates 38 kHz carrier bursts during
  marks. Drive the IR LED through a transistor or MOSFET, not directly from the
  MCU pin for high-current transmission.
- Receiver/transmitter module `GND` must share board `GND`; use the module's
  required supply voltage.

Watch these variables in VS Code while debugging:

- `g_ir_rx_level`: current PA6 level, normally `1`, pulses to `0` when IR is
  detected by a demodulated receiver.
- `g_ir_rx_edge_counter`: increments when PA6 changes level.
- `g_ir_frame_counter`: increments when a complete frame is captured.
- `g_ir_code_count`: number of captured timing entries.
- `g_ir_code_start_level`: first level of the captured waveform.
- `g_ir_code_table_us`: captured timing table in microseconds.

Set a breakpoint on `ir_debug_frame_ready()` to stop exactly after a frame is
captured. The recorded code table is then available as:

```c
static const uint8_t archived_ir_start_level = 0;
static const uint16_t archived_ir_signal_us[] = { ... };
```

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

On this Mac the Homebrew `arm-none-eabi-gcc` package does not include newlib.
The toolchain file therefore prefers the local xPack toolchain at
`../.tools/xpack-arm-none-eabi-gcc` when it exists.

Outputs:

- `build/io_board_at32f455.elf`
- `build/io_board_at32f455.hex`
- `build/io_board_at32f455.bin`

## Flash

```sh
ssh andrew@10.211.55.4 '
  cd /home/andrew/ARTERY/IO_BOARD &&
  . /home/andrew/ARTERY/.tools/pyocd-venv/bin/activate &&
  pyocd flash \
    --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
    -t at32f455vet7 \
    -f 1000000 \
    --erase sector \
    build/io_board_at32f455.elf &&
  pyocd reset \
    --pack /home/andrew/ARTERY/.tools/packs/ArteryTek.AT32F45x_DFP.2.0.1.pack \
    -t at32f455vet7 \
    -f 1000000
'
```

The `flash-ubuntu-pyocd` VS Code task runs the same command after building.

The Homebrew OpenOCD available on macOS does not include the `artery` flash
driver. xPack OpenOCD on Ubuntu can connect to SWD for debug checks, but its
upstream `artery` flash driver does not identify this AT32F455 device for
programming. Use pyOCD with the ARTERY AT32F45x CMSIS-Pack for flashing.

## Debug

Use one of the VS Code launch configurations:

- `Debug AT32F455 via Ubuntu OpenOCD`: run from macOS VS Code; it syncs this
  workspace to Ubuntu, builds there, starts OpenOCD on Ubuntu, and uses
  Ubuntu's `/usr/bin/gdb` over SSH.
- `Debug AT32F455 on Ubuntu Local OpenOCD`: run from VS Code inside Ubuntu.

Recommended workflow on this machine:

1. Open `/Users/andrewlau/qtprj/ARTERY/IO_BOARD` in macOS VS Code.
2. Select `Debug AT32F455 via Ubuntu OpenOCD` in Run and Debug.
3. Set a breakpoint in `led_pattern_apply()` or `led_pattern_run_once()`.
4. Start debugging. VS Code starts Ubuntu OpenOCD through SSH and uses Ubuntu
   GDB to show the current source line, call stack, registers, and variables.

The current blink debug program runs a logical 4-LED pattern in
`g_led_pattern`. The physical `LD1-LD4` LEDs on the ICE/AT-Link section are
AT-Link status LEDs controlled by the debugger MCU, so the AT32F455 application
cannot force them to display the custom pattern. Use `g_led_pattern` in the
watch window and `PH3 DEBUG_OUT` as the target-side physical heartbeat.

The current Ubuntu VM is ARM64. ARTERY official AT32 IDE / AT-Link desktop
tools are generally distributed for x86_64 Linux or Windows, and they do not
open this CMake project as the source of truth. Use them only as reference or
from Windows/x86_64 Ubuntu if an official IDE workflow is required.

The OpenOCD debug server intentionally disables flash programming and the GDB
memory map because OpenOCD's upstream `artery` flash driver does not identify
AT32F455 for programming. Breakpoints in flash must be hardware breakpoints;
the included launch configuration and `debug/at32f455.gdb` set this up.

Manual terminal debug:

```sh
ssh andrew@10.211.55.4
cd /home/andrew/ARTERY/IO_BOARD
/home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/bin/openocd \
  -s /home/andrew/ARTERY/IO_BOARD \
  -s /home/andrew/ARTERY/.tools/xpack-openocd-0.12.0-7/openocd/scripts \
  -c "bindto 0.0.0.0" \
  -f openocd/atlink-cmsis-dap.cfg \
  -c "gdb_memory_map disable" \
  -c "gdb_flash_program disable"
```

In another Ubuntu terminal:

```sh
cd /home/andrew/ARTERY/IO_BOARD
gdb -q build/io_board_at32f455.elf -x debug/at32f455.gdb
```
