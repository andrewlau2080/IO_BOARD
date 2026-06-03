# LED Seven-Segment Module Selection

This note selects practical LED seven-segment modules for the legacy single-MCU
IO board build. The goal is to preserve MCU IO pins while keeping the display
simple enough for the low-cost standalone product.

## Display Requirement

The standalone DB50 board needs to show at least:

```text
[error point] [current point]
```

| Required range | Minimum digits | Practical module choice |
|---|---:|---|
| Old-style `00` ... `99` only | 4 digits | One 4-digit module, split as `EE CC` |
| Full point number `1` ... `192` | 6 digits | Two 4-digit modules or one 8-digit module |
| Full point number plus status | 8 digits | Two 4-digit modules or one 8-digit module |

For the current product direction, prefer the full-point display option so the
operator can see `001` ... `192` directly without paging.

## Recommended Option

Use two TM1637 4-digit LED modules, powered from 3.3 V.

| Item | Value |
|---|---|
| Display format | `EEE CCC` plus two spare digits for status |
| Module count | 2 x 4-digit TM1637 |
| MCU pins | 3 pins if sharing `CLK` and using separate `DIO`; 4 pins if fully separate |
| Power | 3.3 V preferred |
| Firmware complexity | Low; bit-banged 2-wire protocol |
| Reason | Easy to buy, low cost, 3.3 V capable, no address conflict problem if each module has its own `DIO` |

Suggested 8-digit layout using two 4-digit modules:

```text
Module A: [E hundreds][E tens][E ones][status]
Module B: [C hundreds][C tens][C ones][status]
```

Suggested connector signals:

| Signal | Direction | Notes |
|---|---|---|
| `LED7SEG_VCC` | power | 3.3 V |
| `LED7SEG_GND` | power | common ground |
| `LED7SEG_CLK` | MCU output | shared by both TM1637 modules if routing is short |
| `LED7SEG_DIO_ERR` | bidirectional/open-drain style GPIO | error point module |
| `LED7SEG_DIO_CUR` | bidirectional/open-drain style GPIO | current point module |

## AT-START-F455 J8 Test Wiring

For the current six-digit TM1637 application board, use the AT-START-F455 J8
USART1 header pins as GPIO test pins:

| J8 / AT32 signal | Firmware use | TM1637 module pin |
|---|---|---|
| `CTRL_USART1_TX` / `PA9` | GPIO open-drain `TM1637_CLK` | `CLK` |
| `CTRL_USART1_RX` / `PA10` | GPIO open-drain `TM1637_DIO` | `DIO` |
| `3V3` | module supply | `VCC` |
| `GND` | common ground | `GND` |

TM1637 is not a UART protocol. The firmware reuses the USART1 pins only as
ordinary GPIO and generates the TM1637 two-wire timing in software.

The current test firmware drives addresses `0xC0` ... `0xC5` for a six-digit
module, matching the provided 8051 reference code.

The four TM1637 board keys are read with command `0x42`. The practical
application simulation maps the keys from left to right as:

| Raw key | Front-panel function | Simulation behavior |
|---:|---|---|
| `0xF3` | Self test | Run display-only self-test from `A01b01` to `A92b92`, then show `StPASS` |
| `0xF4` | Auto test | Run display-only auto-test from `A01b01` to `A92b92`, then show `AUPASS` |
| `0xF5` | Restart | Restart the last selected self/auto test from `A01b01` |
| `0xF0` | Reset | Stop and show `000000` |

The six digits are split as left three and right three digits. During point
simulation, left three show the A-side point and right three show the B-side
point:

```text
A01b01
A02b02
...
A92b92
```

Future real detection can reuse the same display driver to show compressed
fault examples such as `A1b1NG`, `A1b1SC`, or `A1b1OP` when the point number
fits the six-digit layout.

If layout or firmware simplicity is more important than saving one GPIO, use
separate `CLK` and `DIO` for each module.

## Acceptable Alternatives

| Option | Digits | Interface | Suitability | Notes |
|---|---:|---|---|---|
| One TM1637 4-digit module | 4 | 2 GPIO | Good for lowest cost | Shows `EE CC`; cannot directly show `1-192` for both fields |
| Two TM1637 4-digit modules | 8 | 3-4 GPIO | Recommended | Direct full point display and cheap |
| HT16K33 4-digit I2C backpack | 4 per module | I2C 2 wires | Good but higher cost | Addressable I2C makes multiple modules cleaner |
| TM1638 LED&KEY module | 8 | 3 GPIO | Good for bench/prototype | Includes 8 keys and LEDs; may not match final enclosure |
| MAX7219 8-digit module | 8 | SPI-like 3 wires | Use with caution | Common and cheap, but device is normally 5 V class; verify 3.3 V logic compatibility or add level shifting |

## Electrical Notes

- Keep the display module logic compatible with AT32 3.3 V GPIO.
- Prefer powering the display control IC from 3.3 V when brightness is acceptable.
- If using a 5 V display module, confirm its input high threshold supports 3.3 V
  GPIO or add level translation.
- LED display current should not share a thin return path with analog/mux sense
  circuitry. Route display ground return back to board ground with enough width.
- Put series resistors or small RC footprints on long display signal lines if
  the front-panel cable is long.

## BOM Decision

Default BOM for the old DB50 single-MCU board:

```text
U_LED7SEG_A / MOD_LED7SEG_A: TM1637 4-digit 7-segment module, 3.3 V capable
U_LED7SEG_B / MOD_LED7SEG_B: TM1637 4-digit 7-segment module, 3.3 V capable
J_LED7SEG: VCC, GND, CLK, DIO_ERR, DIO_CUR
```

Keep `J_RPI_COMM` as DNP for this single-MCU build. The Raspberry Pi build does
not populate the LED seven-segment modules.
