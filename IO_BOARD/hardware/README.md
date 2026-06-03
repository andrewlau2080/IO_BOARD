# IO_BOARD hardware variants

This directory separates the two IO-board hardware connector variants. Firmware
and shared low-level scan code stay in the current `IO_BOARD` project root:

- `inc/`
- `src/`
- `CMakeLists.txt`

Do not fork firmware per connector variant unless the physical mapping can no
longer be represented by a board profile table.

## Variants

| Directory | Variant | External points | Connector concept | Status |
|---|---|---:|---|---|
| `old_db50/` | Legacy-compatible DB50 layout | 96 OUT + 96 IN | 4 DB50 connectors, each DB50 has upper/lower corresponding rows, row pins 1-24 used, row pin 25 NC | New specification |
| `db78_64x4/` | DB78 separated IN/OUT layout | 128 OUT + 128 IN | 2 DB78 for OUT and 2 DB78 for IN, each DB78 uses 64 pins | Existing scheme summary |

## Shared scan resources

The current firmware and pin plan expose four logical mux banks:

| Logical bank | Capacity | Intended use |
|---|---:|---|
| `IO_MUX_OUT_A` | 64 | Output test source points 1-64 |
| `IO_MUX_OUT_B` | 64 | Output test source points 65-128 |
| `IO_MUX_IN_A` | 64 | Input sense points 1-64 |
| `IO_MUX_IN_B` | 64 | Input sense points 65-128 |

The legacy DB50 hardware uses points 1-96 on each side and leaves points
97-128 unused. The DB78 hardware uses points 1-128 on each side.

## Shared communication/display port

Reserve one MCU communication resource as `COMM_DISPLAY_PORT`.

| Product build | Connected device | Role |
|---|---|---|
| Legacy DB50 single-MCU build | LED seven-segment display module/driver | AT32 drives old-style local display |
| Raspberry Pi build | Raspberry Pi | AT32 scan device communication; Raspberry Pi drives LCDM/screen |

These two options should be selected by BOM/jumpers/0R links and firmware board
profile. Do not require AT32 to drive LCDM directly in the Raspberry Pi build.

## Naming convention

- Logical output points: `OUT001` ... `OUT128`
- Logical input points: `IN001` ... `IN128`
- Legacy DB50 interface groups: `JOLD1` ... `JOLD4`
- DB78 output connectors: `JOUT1`, `JOUT2`
- DB78 input connectors: `JIN1`, `JIN2`

Keep schematic sheet names, net labels, connector silkscreen, BOM names, and
firmware board profile names aligned with these identifiers.
