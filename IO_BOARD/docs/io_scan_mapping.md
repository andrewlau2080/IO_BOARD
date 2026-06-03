# IO Scan Mapping And Position Codes

This document defines the shared firmware point model for both IO board hardware
variants. Firmware remains common; connector differences are selected by scan
profile and board wiring.

## Logical Points

| Item | Range | Position code | Notes |
|---|---:|---|---|
| Output source points | `OUT001` ... `OUT128` | `0x0001` ... `0x0080` | Driven one point at a time during scan |
| Input sense points | `IN001` ... `IN128` | `0x0101` ... `0x0180` | Read one point at a time during scan |

Position code format:

| Bits | Meaning |
|---|---|
| `bit 8` | `0` = OUT, `1` = IN |
| `bits 7:0` | 1-based point number, `1` ... `128`; `0` is invalid |

Examples:

| Logical point | Position code |
|---|---:|
| `OUT001` | `0x0001` |
| `OUT064` | `0x0040` |
| `OUT065` | `0x0041` |
| `OUT096` | `0x0060` |
| `OUT128` | `0x0080` |
| `IN001` | `0x0101` |
| `IN064` | `0x0140` |
| `IN065` | `0x0141` |
| `IN096` | `0x0160` |
| `IN128` | `0x0180` |

## Firmware Scan Profiles

| Profile ID | Firmware name | Valid OUT points | Valid IN points | Hardware variant |
|---:|---|---:|---:|---|
| `0` | `old_db50_96x96` | `OUT001` ... `OUT096` | `IN001` ... `IN096` | Legacy-compatible DB50 |
| `1` | `db78_64x4_128x128` | `OUT001` ... `OUT128` | `IN001` ... `IN128` | DB78 separated IN/OUT |

For `old_db50_96x96`, points `OUT097` ... `OUT128` and `IN097` ... `IN128`
are reserved and must be treated as invalid by the firmware protocol.

## Mux Bank Mapping

| Logical range | Firmware mux bank | Mux index |
|---|---|---:|
| `OUT001` ... `OUT064` | `IO_MUX_OUT_A` | `0` ... `63` |
| `OUT065` ... `OUT128` | `IO_MUX_OUT_B` | `0` ... `63` |
| `IN001` ... `IN064` | `IO_MUX_IN_A` | `0` ... `63` |
| `IN065` ... `IN128` | `IO_MUX_IN_B` | `0` ... `63` |

During a pair read, firmware selects one OUT mux channel and one IN mux channel,
waits for the settle delay, then calls `io_scan_measure_selected_pair()`.
That function is currently a weak stub and must be replaced by the final
measurement circuit implementation.

## Legacy DB50 Connector Mapping

Each physical DB50 connector has two corresponding 25-pin rows:

| Connector | Upper row pins 1-24 | Lower row pins 1-24 | Row pin 25 |
|---|---|---|---|
| `JOLD1` | `OUT001` ... `OUT024` | `IN001` ... `IN024` | NC |
| `JOLD2` | `OUT025` ... `OUT048` | `IN025` ... `IN048` | NC |
| `JOLD3` | `OUT049` ... `OUT072` | `IN049` ... `IN072` | NC |
| `JOLD4` | `OUT073` ... `OUT096` | `IN073` ... `IN096` | NC |

Pin formula for DB50 row pins 1-24:

| Formula | Meaning |
|---|---|
| `point = (connector_index - 1) * 24 + row_pin` | `connector_index` is `1` ... `4`; `row_pin` is `1` ... `24` |
| upper row position | `OUT%03u`, code `0x0000 | point` |
| lower row position | `IN%03u`, code `0x0100 | point` |

## DB78 64x4 Connector Mapping

| Connector | Used pins | Logical points |
|---|---:|---|
| `JOUT1` | 1-64 | `OUT001` ... `OUT064` |
| `JOUT2` | 1-64 | `OUT065` ... `OUT128` |
| `JIN1` | 1-64 | `IN001` ... `IN064` |
| `JIN2` | 1-64 | `IN065` ... `IN128` |

Pin formula for DB78 used pins 1-64:

| Connector | Formula |
|---|---|
| `JOUT1` | `OUT point = pin` |
| `JOUT2` | `OUT point = 64 + pin` |
| `JIN1` | `IN point = pin` |
| `JIN2` | `IN point = 64 + pin` |

