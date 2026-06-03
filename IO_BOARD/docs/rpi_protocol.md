# Raspberry Pi Communication Protocol

This protocol is the shared interface between the Raspberry Pi controller and
the AT32 IO scan firmware. It also reserves one command for the old single-MCU
build where the same physical port drives a LED seven-segment display module.

## Frame Format

All multi-byte fields are little-endian.

| Offset | Size | Name | Description |
|---:|---:|---|---|
| 0 | 1 | `SOF0` | `0x55` |
| 1 | 1 | `SOF1` | `0xAA` |
| 2 | 1 | `VERSION` | `0x01` |
| 3 | 1 | `COMMAND` | Command or response command |
| 4 | 1 | `SEQ` | Host sequence number, echoed by response |
| 5 | 2 | `LEN` | Payload length, max 240 bytes |
| 7 | `LEN` | `PAYLOAD` | Command-specific data |
| 7 + `LEN` | 2 | `CRC16` | Modbus CRC16 over bytes from `VERSION` through payload |

Response command is normally `request_command | 0x80`. Error frames use command
`0x7F`.

## Status Codes

| Code | Name | Meaning |
|---:|---|---|
| `0x00` | `OK` | Command accepted or completed |
| `0x01` | `BAD_CRC` | CRC mismatch |
| `0x02` | `BAD_LENGTH` | Frame or payload length is invalid |
| `0x03` | `BAD_COMMAND` | Unsupported command or frame marker/version error |
| `0x04` | `BAD_POSITION` | Position code is invalid for the active profile |
| `0x05` | `BUSY` | Scan is already running or device cannot accept the request |
| `0x06` | `NOT_READY` | Requested data is not available |

## Commands

| Command | Name | Request payload | Response payload |
|---:|---|---|---|
| `0x01` | `PING` | none | `status:u8` |
| `0x02` | `GET_INFO` | none | `status:u8, profile:u8, out_count:u8, in_count:u8, scan_counter:u32` |
| `0x03` | `SET_PROFILE` | `profile:u8` | `status:u8, active_profile:u8` |
| `0x10` | `START_SCAN` | optional `profile:u8`; empty means use current profile | `status:u8` |
| `0x11` | `STOP_SCAN` | none | `status:u8` |
| `0x12` | `GET_SCAN_STATUS` | none | `status:u8, running:u8, progress_out_pos:u16, scanned_pairs:u32, connected_pairs:u32, scan_counter:u32` |
| `0x13` | `GET_SCAN_ROW` | `out_pos:u16` | `status:u8, out_pos:u16, word_count:u8, bitmap:u32[word_count]` |
| `0x20` | `READ_PAIR` | `out_pos:u16, in_pos:u16` | `status:u8, out_pos:u16, in_pos:u16, connected:u8` |
| `0x21` | `SELECT_OUT` | `out_pos:u16` | `status:u8, active_out_pos:u16` |
| `0x22` | `SELECT_IN` | `in_pos:u16` | `status:u8, active_in_pos:u16` |
| `0x30` | `LED7SEG_WRITE` | `mode:u8, length:u8, data:u8[length]` | `status:u8` |
| `0x40` | `PRINT_REQUEST` | `station:u8, product_id:u16, test_count:u16, pass:u8` | `status:u8, queue_id:u16` |
| `0x41` | `PRINT_STATUS` | `queue_id:u16` | `status:u8, queue_state:u8` |
| `0x42` | `PRINT_TEMPLATE_WRITE` | `field_id:u8, text_len:u8, ascii_text:u8[text_len]` | `status:u8` |
| `0x7F` | `ERROR` | not sent by host | `status:u8, failed_command:u8` |

## Scan Row Bitmap

`GET_SCAN_ROW` returns the detected input connections for one selected output
point. Bit `0` of bitmap word `0` is `IN001`; bit `31` of word `0` is `IN032`.

| Input range | Bitmap word | Bits |
|---|---:|---:|
| `IN001` ... `IN032` | `0` | `0` ... `31` |
| `IN033` ... `IN064` | `1` | `0` ... `31` |
| `IN065` ... `IN096` | `2` | `0` ... `31` |
| `IN097` ... `IN128` | `3` | `0` ... `31` |

For the old DB50 profile, word `3` bits for `IN097` ... `IN128` are reserved
and must be zero.

## LED Seven-Segment Display Command

`LED7SEG_WRITE` is reserved for the old single-MCU product build. The same
physical `COMM_DISPLAY_PORT` is connected either to a Raspberry Pi or to a LED
seven-segment module/driver by BOM/jumper selection.

| Field | Meaning |
|---|---|
| `mode = 0x00` | Raw segment bytes in `data[]` |
| `mode = 0x01` | ASCII digits/text in `data[]`; firmware/display driver handles conversion |
| `length` | Number of bytes following, limited by frame max payload |

The Raspberry Pi product build should not fit the LED seven-segment display
module. The Pi drives LCDM/screen and uses the AT32 only as the scan device.

## Print Terminal Commands

The print terminal commands are reserved for the production-line terminal model.
They are intended for the Raspberry Pi/Linux terminal side, not for direct MCU
printer driving.

| Command | Purpose |
|---|---|
| `PRINT_REQUEST` | A tester station asks the terminal to print a label after local test completion |
| `PRINT_STATUS` | Query queue state: `0=queued`, `1=printing`, `2=done`, `3=busy`, `4=error` |
| `PRINT_TEMPLATE_WRITE` | Update a terminal-side label field before printing |

The old first-gen learned communication link is modeled separately in
`line_comm_bridge`. When that protocol is decoded, it can be bridged into these
same terminal print commands.
