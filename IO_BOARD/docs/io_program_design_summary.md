# IO Program Design Summary

This is the working summary table for the first firmware design stage:
complete IO scanning, position coding, and Raspberry Pi communication.

## Modules

| Module | Files | Responsibility |
|---|---|---|
| Board IO | `inc/io_board.h`, `src/io_board.c` | MCU pin init, low-level mux bank select, PB4 buzzer control |
| Scan core | `inc/io_scan.h`, `src/io_scan.c` | Logical OUT/IN position validation, pair selection, full matrix scan |
| Pi protocol | `inc/rpi_protocol.h`, `src/rpi_protocol.c` | Binary frame encode/decode, CRC16, command IDs |
| Pi RS485 transport | `inc/rpi_rs485.h`, `src/rpi_rs485.c` | USART1 PA9/PA10, 115200 8N1 polling RX and blocking TX |
| tester_v2 legacy dispatcher | `inc/rpi_rs485_legacy.h`, `src/rpi_rs485_legacy.c` | Compatible 7-byte request and 22-byte response protocol for the current Qt/Raspberry Pi application |
| Hardware variants | `hardware/old_db50/`, `hardware/db78_64x4/` | Connector/BOM-specific specifications |

## Product Control Split

| Product path | Host/control model | IO mux control | Display/terminal role | Firmware impact |
|---|---|---|---|---|
| Basic tester | Single MCU local workflow | Direct AT32 GPIO control of CD4051 address/enable lines | LEDM/TM1637 on PA9/PA10 shared role connector; PB4 buzzer for PASS/NG prompt | Uses `FIRST_GEN_4051_LOCAL`, learned Flash matrix, and local PASS/NG indication |
| LCDM tester | Single MCU local workflow | Direct AT32 GPIO control of CD4051 address/enable lines | TJC LCDM on PB3/PB5; PB4 buzzer for PASS/NG prompt | Same test core as basic tester, different display page set |
| Print host | Line edge controller | No local scan required except diagnostics | TJC LCDM on PB3/PB5; PA9/PA10 used as printer USART/adapter; PB4 buzzer may be used for alarms | Manages binding, print queue, local cache, WiFi/MAS sync |
| Second-gen Raspberry Pi board | Raspberry Pi sends commands over RS485 | Existing direct GPIO 4051 mux control remains the default | Raspberry Pi drives LCD/screen and high-level UI | Needs RS485 transport and command dispatcher; AT32 acts as scan/IO slave |

The second-gen product must not be treated as an autonomous scanner. The AT32
waits for Raspberry Pi RS485 frames, executes requested scan operations, and
returns results. Full self-test or automatic test sequencing belongs on the
Raspberry Pi side unless a fallback standalone mode is explicitly selected.

## Device Role Configuration

The unified firmware must not decide print host versus tester only by probing
LCDM. High-end testers and print hosts can both have a TJC LCDM on PB3/PB5.
The role is a stored device configuration, written by factory tooling,
DEBUG_TTL, LCDM maintenance page, WiFi maintenance command, or line pairing.

| Stored role | Meaning | Display | PA9/PA10 use |
|---|---|---|---|
| `TESTER_BASIC` | Standard tester station | LEDM/TM1637 | LEDM GPIO/UART only |
| `TESTER_LCDM` | High-end tester station | TJC LCDM | Not used for printing; may remain idle or reserved |
| `PRINT_HOST` | Line print master / edge controller | TJC LCDM | Printer USART/adapter |
| `UNCONFIGURED` | New or invalid device config | Maintenance only | Safe idle |

Required stored fields: `device_role`, `display_type`, `device_uid`,
`line_id`, `station_id`, config revision, and CRC. If the config is missing or
CRC fails, firmware enters unconfigured safe mode: no automatic scan and no
printer command output.

## Scan Data Model

| Data | Type | Meaning |
|---|---|---|
| `io_scan_profile_t` | profile table | Selects valid OUT/IN count for board variant |
| `io_scan_pair_result_t` | pair result | One `OUTxxx` to one `INxxx` result |
| `io_scan_result_t.matrix[128][4]` | bitmap matrix | Full scan result; one row per OUT, four 32-bit words for IN bitmap |
| `g_scan_active_out_pos` | watch/debug global | Current selected OUT position |
| `g_scan_active_in_pos` | watch/debug global | Current selected IN position |
| `g_scan_pair_counter` | watch/debug global | Number of pair measurements attempted |
| `g_scan_connected_counter` | watch/debug global | Number of connected pair measurements detected |
| `g_scan_frame_counter` | watch/debug global | Number of completed full scans |

## Profile Capacity Summary

| Profile | OUT capacity | IN capacity | Total pair checks | Notes |
|---|---:|---:|---:|---|
| `old_db50_96x96` | 96 | 96 | 9,216 | Four DB50 connectors, row pin 25 NC |
| `db78_64x4_128x128` | 128 | 128 | 16,384 | Two OUT DB78 plus two IN DB78 |
| `first_gen_1th_96x96` | 96 | 96 | 9,216 | `1thsch.pdf`: A half 48 points and B half 48 points, direct CD4051 GPIO control |

## Position Code Summary

| Position family | Code range | Valid in old DB50 | Valid in DB78 64x4 |
|---|---:|---:|---:|
| `OUT001` ... `OUT096` | `0x0001` ... `0x0060` | yes | yes |
| `OUT097` ... `OUT128` | `0x0061` ... `0x0080` | no, reserved | yes |
| `IN001` ... `IN096` | `0x0101` ... `0x0160` | yes | yes |
| `IN097` ... `IN128` | `0x0161` ... `0x0180` | no, reserved | yes |

## Immediate Implementation Status

| Item | Status | Notes |
|---|---|---|
| Logical point coding | done | Shared by protocol and firmware |
| Mux bank selection | done | Uses existing four 64-channel mux banks |
| Full matrix scan loop | framework done | Calls measurement hook for each pair |
| Measurement circuit read | done for first-gen local mode | `src/first_gen_4051_scan.c` overrides `io_scan_measure_selected_pair()` in `FIRST_GEN_4051_LOCAL`; default non-measurement builds still keep the weak placeholder |
| First-gen self-learn standard | done for first-gen local mode | Known-good harness matrix is learned by board `ENTER`, then saved to the reserved last Flash sector |
| First-gen local display flow | code present, wiring pending | Current code uses the earlier PA9/PA10 TM1637 bench wiring; `1thsch.pdf` routes PA9/PA10 to the USART header and shows LCDM on `LCM_*` nets |
| Raspberry Pi frame codec | done | Encode/decode and CRC16 available |
| Raspberry Pi RS485 physical layer | done for legacy test | USART1 `PA9/PA10`, 115200 8N1; DE/RE GPIO is optional and disabled until final schematic confirms a direction pin |
| tester_v2 simple command dispatcher | done for link/UI test | Handles `0x10` ... `0x17`; currently reports active DB78 profile points as OK until the real measurement hook is implemented |
| New `55 AA` Raspberry Pi command dispatcher | pending | Keep for future richer scan/profile/row/pair API after the current Qt simple protocol test path is stable |
| LED seven-segment reuse of comm port | specified | Command `0x30` reserved |
| Learned IR print link after scan | code present, wiring pending | Current 2026-07-07 schematic uses `PB6=IR_TX` and `PB7=IR_RX`; older PA6/PA7 learned IR wiring is bench history only |

## Next Development Step

| Priority | Item | Expected result |
|---:|---|---|
| 1 | Decide first-gen display wiring | Either migrate firmware to `1thsch.pdf` LCDM nets or add confirmed TM1637 wiring |
| 2 | Decide first-gen print/IR wiring | Align firmware and connectors to the current `PB6=IR_TX` / `PB7=IR_RX` schematic nets |
| 3 | Bench-test first-gen scan GPIO | Confirm corrected `OUT_BMUX_EN0..EN7 = PD8..PD15` selection with meter/scope |
| 4 | Decode remaining legacy return codes | Fill PRINT_ACK/BUSY/DONE timing placeholders when captures are available |

## RS485 Implementation Estimate

| Block | Expected Flash | Expected RAM | Notes |
|---|---:|---:|---|
| USART/RS485 driver | 1-3 KB | 256-512 B | UART init, byte RX, TX, DE/RE direction timing |
| Frame assembler/parser buffer | 1-2 KB | 512-1024 B | Uses existing `rpi_protocol_decode()` and max 249-byte frame |
| Command dispatcher | 3-8 KB | 0.5-2 KB | Handles profile, read pair, scan status, row bitmap, errors |
| Optional async full scan state | 2-6 KB | 2-3 KB | Needed if Pi starts scan then polls rows/status |

Even with both product paths enabled, this remains small compared with the
AT32F455VET7 capacity. The main risk is not memory size; it is keeping the
first-gen local workflow and second-gen RS485 slave workflow cleanly separated.
