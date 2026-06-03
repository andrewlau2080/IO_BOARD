# Legacy DB50 IO board specification

## Purpose

This variant follows the old cable tester connector style. It is intended for a
legacy-compatible IO board using DB50 connector positions while reusing the
current AT32F455/CD4051 scan architecture and firmware.

Firmware remains in the parent `IO_BOARD` project. This directory only tracks
the old DB50 hardware circuit, connector mapping, and BOM.

## External connector structure

The board has 4 legacy interface groups:

| Group | Physical concept | Effective pairs | Empty pins |
|---|---|---:|---|
| `JOLD1` | One DB50 connector, upper/lower rows vertically corresponding | 24 OUT/IN pairs | Upper row pin 25, lower row pin 25 |
| `JOLD2` | One DB50 connector, upper/lower rows vertically corresponding | 24 OUT/IN pairs | Upper row pin 25, lower row pin 25 |
| `JOLD3` | One DB50 connector, upper/lower rows vertically corresponding | 24 OUT/IN pairs | Upper row pin 25, lower row pin 25 |
| `JOLD4` | One DB50 connector, upper/lower rows vertically corresponding | 24 OUT/IN pairs | Upper row pin 25, lower row pin 25 |

Each group therefore has 50 physical DB50 contacts but only 48 are populated in
the logical test map. The 25th corresponding pair is reserved/NC.

Total useful test points:

- 96 output points: `OUT001` ... `OUT096`
- 96 input points: `IN001` ... `IN096`
- 192 external test contacts total

Remaining mux capacity is unused:

- `OUT097` ... `OUT128`: reserved/unused
- `IN097` ... `IN128`: reserved/unused

## Per-group mapping rule

For each legacy group, DB50 row pin `1` to `24` is one test pair:

```text
Upper DB50 row pin N -> OUT point
Lower DB50 row pin N -> IN point
N = 1..24
Pin 25 is NC/reserved on both DB50 upper and lower rows.
```

| Group | DB50 row pins used | OUT range | IN range | Mux usage |
|---|---:|---:|---:|---|
| `JOLD1` | 1-24 | `OUT001`-`OUT024` | `IN001`-`IN024` | `OUT_A[0..23]`, `IN_A[0..23]` |
| `JOLD2` | 1-24 | `OUT025`-`OUT048` | `IN025`-`IN048` | `OUT_A[24..47]`, `IN_A[24..47]` |
| `JOLD3` | 1-24 | `OUT049`-`OUT072` | `IN049`-`IN072` | `OUT_A[48..63]` + `OUT_B[0..7]`, `IN_A[48..63]` + `IN_B[0..7]` |
| `JOLD4` | 1-24 | `OUT073`-`OUT096` | `IN073`-`IN096` | `OUT_B[8..31]`, `IN_B[8..31]` |

## Detailed DB50 row pin map

| Group | Pin | DB50 upper row | DB50 lower row | Logical pair |
|---|---:|---|---|---|
| `JOLD1` | 1-24 | `OUT001`-`OUT024` | `IN001`-`IN024` | Pair 1-24 |
| `JOLD1` | 25 | NC | NC | Reserved |
| `JOLD2` | 1-24 | `OUT025`-`OUT048` | `IN025`-`IN048` | Pair 25-48 |
| `JOLD2` | 25 | NC | NC | Reserved |
| `JOLD3` | 1-24 | `OUT049`-`OUT072` | `IN049`-`IN072` | Pair 49-72 |
| `JOLD3` | 25 | NC | NC | Reserved |
| `JOLD4` | 1-24 | `OUT073`-`OUT096` | `IN073`-`IN096` | Pair 73-96 |
| `JOLD4` | 25 | NC | NC | Reserved |

For PCB silkscreen, mark the two rows inside each DB50 connector clearly:

```text
JOLDx upper row: OUT side
JOLDx lower row: IN side
```

Use one schematic connector symbol per physical DB50 connector when possible.
If the EDA library splits the upper and lower rows into two symbol units, keep
both units under the same physical designator `JOLDx`.

## Circuit blocks

Use the same scan hardware concept as the current IO board:

1. `AT32F455VET7` main MCU.
2. Four CD4051 mux banks:
   - `IO_MUX_OUT_A`
   - `IO_MUX_OUT_B`
   - `IO_MUX_IN_A`
   - `IO_MUX_IN_B`
3. Output stimulus path:
   - MCU/mux control selects one OUT point.
   - Output point is driven through current limiting/protection.
4. Input sense path:
   - IN mux scans all candidate input points.
   - Sense signal returns to MCU comparator/GPIO/ADC path according to the
     final measurement circuit.
5. Connector protection:
   - Series resistor per external test point.
   - ESD/TVS footprint per connector group or per signal cluster.
   - Optional RC/filter footprint if old tester cabling is noisy.

## Electrical assumptions

- MCU logic is 3.3 V.
- Mux device selection is 74HC4051 powered from 3.3 V. AT32 address and enable
  GPIO can connect directly to 74HC4051 control pins, so no TTL level translator
  is required on those control lines.
- Do not power 74HC4051 from 5 V while driving its control pins directly from
  AT32 3.3 V GPIO; 3.3 V is not a guaranteed CMOS high level for every 5 V HC
  device and temperature/process corner.
- Signals passing through a 3.3 V powered 74HC4051 must remain within the mux
  supply rails. If any external test stimulus uses 5 V, it must be limited,
  divided, clamped, or moved to a separate 5 V tolerant measurement front end.
- External test contacts must not drive MCU pins directly.
- Every external OUT/IN point should have at least a series protection resistor
  or equivalent protection path.
- DB50 shell/chassis grounding must be decided with the enclosure strategy.
  Default: provide chassis/earth stitching footprints, do not hard-short to
  digital GND unless verified.
- Pin 25 on each DB50 is NC by default. It may be tied to shield or
  reserved only after mechanical compatibility is confirmed.

## Firmware profile

This variant uses a 96-pair board profile:

```c
#define IO_BOARD_PROFILE_OLD_DB50
#define IO_BOARD_OUT_COUNT 96
#define IO_BOARD_IN_COUNT 96
#define IO_DISPLAY_MODE_LED7SEG
```

Logical mapping is direct:

```text
Pair K uses OUT K and IN K, K = 1..96.
```

The current mux indexing remains zero-based internally:

```text
OUT001 -> IO_MUX_OUT_A index 0
OUT064 -> IO_MUX_OUT_A index 63
OUT065 -> IO_MUX_OUT_B index 0
OUT096 -> IO_MUX_OUT_B index 31
```

The same applies to `IN001` ... `IN096`.

## Display interface

The legacy DB50 single-MCU board must keep local display support. It uses the
same physical MCU communication resource that the Raspberry Pi variant uses for
host communication, but it is populated as an LED seven-segment display
interface instead.

Recommended signal name:

```text
COMM_DISPLAY_PORT
```

Assembly/function by product variant:

| Variant | `COMM_DISPLAY_PORT` target | Function |
|---|---|---|
| Legacy DB50 single-MCU board | LED seven-segment display module/driver | AT32 drives local error/current point display |
| Raspberry Pi board | Raspberry Pi host communication | AT32 returns scan data; Raspberry Pi drives LCDM/screen |

For this old DB50 variant:

- Do not fit the Raspberry Pi host connector unless the product is a Pi version.
- Fit the LED seven-segment display connector/driver path.
- Prefer a serial/UART LED display module or simple serial display driver so
  the same MCU UART resource can be reused by the Pi variant.
- If using SPI/shift-register or I2C LED drivers, keep the connector and BOM
  option clearly separated from the Pi communication option.

Expected local display content:

```text
[error point] [current point]
```

Use 2+2 digits for old-style compatibility, or 3+3 digits if the enclosure and
cost allow direct display of points above 99.

Current recommended module plan:

- Use two TM1637 4-digit LED modules, powered from 3.3 V.
- Display error point on module A and current point on module B.
- Use one shared `LED7SEG_CLK` plus two data lines, `LED7SEG_DIO_ERR` and
  `LED7SEG_DIO_CUR`, or use fully separate CLK/DIO pairs if routing is easier.
- This gives 8 total digits, enough for direct `001` ... `192` display plus
  simple status symbols.
- See `../../docs/led7seg_module_selection.md` from this directory for
  the selection table and alternatives.

## PCB layout notes

This old DB50 variant is more layout-constrained than the DB78 variant because
the connector rows are mechanically fixed and the signal order is interleaved
by old-machine compatibility.

Recommended placement order:

1. Lock all `JOLD1` ... `JOLD4` connector mechanical positions first.
2. Place mux banks physically close to their corresponding connector groups:
   - `JOLD1/JOLD2` near A-bank muxes.
   - `JOLD3/JOLD4` near B-bank muxes.
3. Keep address and enable lines short and grouped.
4. Route external OUT/IN traces with consistent spacing and avoid long parallel
   coupling where possible.
5. Reserve enough via channels between the DB50 upper and lower row fanouts.
6. Do not let connector routing split the digital GND return path around MCU,
   crystal, SWD, display, or power supply areas.

If two-layer routing becomes too tight, first consider swapping mux-to-connector
order inside the board profile while preserving external DB50 row pin order. If it
is still congested, move the product to a four-layer PCB.

## Deliverables in this directory

- `README.md`: old DB50 hardware specification.
- `BOM.md`: old DB50 variant BOM baseline.
- Future schematic/PCB files for this variant should stay here.
