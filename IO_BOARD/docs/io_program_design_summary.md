# IO Program Design Summary

This is the working summary table for the first firmware design stage:
complete IO scanning, position coding, and Raspberry Pi communication.

## Modules

| Module | Files | Responsibility |
|---|---|---|
| Board IO | `inc/io_board.h`, `src/io_board.c` | MCU pin init and low-level mux bank select |
| Scan core | `inc/io_scan.h`, `src/io_scan.c` | Logical OUT/IN position validation, pair selection, full matrix scan |
| Pi protocol | `inc/rpi_protocol.h`, `src/rpi_protocol.c` | Binary frame encode/decode, CRC16, command IDs |
| Hardware variants | `hardware/old_db50/`, `hardware/db78_64x4/` | Connector/BOM-specific specifications |

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
| Raspberry Pi command dispatcher | pending | Next step after UART/SPI physical port is finalized |
| LED seven-segment reuse of comm port | specified | Command `0x30` reserved |

