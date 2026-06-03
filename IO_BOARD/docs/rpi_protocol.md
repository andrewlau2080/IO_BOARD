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

## tester_v2 Legacy RS485 Compatibility

The current Raspberry Pi / Qt application in
`/Users/andrewlau/qtprj/tester_v2` uses the verified old simple protocol, not
the newer `0x55 0xAA` frame above. The AT32 firmware now includes a compatibility
slave in `inc/rpi_rs485_legacy.h` and `src/rpi_rs485_legacy.c`.

Physical settings:

| Item | Value |
|---|---|
| UART | `USART1` |
| Pins | `PA9` = TX, `PA10` = RX |
| Baud | `115200` |
| Format | 8 data bits, no parity, 1 stop bit |
| RS485 direction | No GPIO direction pin by default; set `RPI_RS485_USE_DIR_PIN=1` and define the DE/RE pin when the final transceiver schematic requires it |

Request format from Raspberry Pi / Qt:

| Byte | Meaning |
|---:|---|
| 0 | `0x9E` |
| 1 | command |
| 2 | `para0` |
| 3 | `para1` |
| 4 | `para2` |
| 5 | `para3` |
| 6 | `0xCC` |

Response format from AT32:

| Byte | Meaning |
|---:|---|
| 0 | `0x9E` |
| 1 | `command | 0x80` |
| 2 | status, `0x00` = OK |
| 4 | active IO count |
| 5 | normal `READ`: next A, or `0xFF` when finished; `READ_FORCE`: bitmap byte 0 |
| 6 | normal `READ`: next B, or `0xFF` when finished; `READ_FORCE`: bitmap byte 1 |
| 7 | normal `READ`: pass count |
| 8 | normal `READ`: error count |
| 5 ... 20 | `READ_FORCE` bitmap, bit 0 = port 1 |
| 21 | `0xCC` |

Supported command IDs:

| Command | Name | Current AT32 behavior |
|---:|---|---|
| `0x10` | `SIMPLE_START` | Starts the compatibility test model and replies with a summary |
| `0x11` | `SIMPLE_STOP` | Stops the compatibility test model |
| `0x12` | `SIMPLE_READ` | Returns `IONumMax`, finished marker `0xFF/0xFF`, pass count, and error count |
| `0x13` | `SIMPLE_WRITE` | Acknowledged for compatibility |
| `0x14` | `SIMPLE_START_FORCE` | Starts force/detail mode and returns bitmap |
| `0x15` | `SIMPLE_READ_FORCE` | Returns 16-byte OK bitmap for the active IO count |
| `0x16` | `SIMPLE_KEY_READ` | Acknowledged with zero payload |
| `0x17` | `SIMPLE_KEYMAP_READ` | Acknowledged with zero payload |

The current compatibility build is intended to verify the Raspberry Pi/Qt
RS485 link and UI parsing before the final IO measurement circuit is fixed. It
uses the DB78 128-point profile by default and reports all active points as OK.
The hardware scan hook remains `io_scan_measure_selected_pair()`.
