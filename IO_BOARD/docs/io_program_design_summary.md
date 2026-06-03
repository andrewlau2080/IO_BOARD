# IO Program Design Summary

This is the working summary table for the first firmware design stage:
complete IO scanning, position coding, and Raspberry Pi communication.

## Modules

| Module | Files | Responsibility |
|---|---|---|
| Board IO | `inc/io_board.h`, `src/io_board.c` | MCU pin init and low-level mux bank select |
| Scan core | `inc/io_scan.h`, `src/io_scan.c` | Logical OUT/IN position validation, pair selection, full matrix scan |
| Pi protocol | `inc/rpi_protocol.h`, `src/rpi_protocol.c` | Binary frame encode/decode, CRC16, command IDs |
| Pi RS485 transport | `inc/rpi_rs485.h`, `src/rpi_rs485.c` | USART1 PA9/PA10, 115200 8N1 polling RX and blocking TX |
| tester_v2 legacy dispatcher | `inc/rpi_rs485_legacy.h`, `src/rpi_rs485_legacy.c` | Compatible 7-byte request and 22-byte response protocol for the current Qt/Raspberry Pi application |
| Hardware variants | `hardware/old_db50/`, `hardware/db78_64x4/` | Connector/BOM-specific specifications |

## Product Control Split

| Product path | Host/control model | IO mux control | Display/terminal role | Firmware impact |
|---|---|---|---|---|
| First-gen replacement | Single MCU local workflow | SN74LS164 shifts 4051 select/enable state | Local LED seven-segment plus learned IR print link | Needs a separate low-level mux driver and local test state machine |
| Second-gen Raspberry Pi board | Raspberry Pi sends commands over RS485 | Existing direct GPIO 4051 mux control remains the default | Raspberry Pi drives LCD/screen and high-level UI | Needs RS485 transport and command dispatcher; AT32 acts as scan/IO slave |

The second-gen product must not be treated as an autonomous scanner. The AT32
waits for Raspberry Pi RS485 frames, executes requested scan operations, and
returns results. Full self-test or automatic test sequencing belongs on the
Raspberry Pi side unless a fallback standalone mode is explicitly selected.

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
| Measurement circuit read | pending hardware decision | Implement by overriding `io_scan_measure_selected_pair()` |
| Raspberry Pi frame codec | done | Encode/decode and CRC16 available |
| Raspberry Pi RS485 physical layer | done for legacy test | USART1 `PA9/PA10`, 115200 8N1; DE/RE GPIO is optional and disabled until final schematic confirms a direction pin |
| tester_v2 simple command dispatcher | done for link/UI test | Handles `0x10` ... `0x17`; currently reports active DB78 profile points as OK until the real measurement hook is implemented |
| New `55 AA` Raspberry Pi command dispatcher | pending | Keep for future richer scan/profile/row/pair API after the current Qt simple protocol test path is stable |
| LED seven-segment reuse of comm port | specified | Command `0x30` reserved |

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
