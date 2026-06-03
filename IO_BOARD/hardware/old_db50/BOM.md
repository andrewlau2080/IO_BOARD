# Legacy DB50 variant BOM baseline

This BOM is a design baseline for the old DB50 connector variant. Values and
manufacturer part numbers must be confirmed during schematic capture and PCB
footprint selection.

| Block | Designator pattern | Qty | Item | Notes |
|---|---:|---:|---|---|
| MCU | `U_MCU` | 1 | AT32F455VET7, LQFP100 | Shared with current firmware |
| Output mux | `U_OUT_Ax`, `U_OUT_Bx` | 16 | 74HC4051 analog mux, powered from 3.3 V | 8 muxes per 64-point OUT bank; AT32 GPIO drives address/enable directly |
| Input mux | `U_IN_Ax`, `U_IN_Bx` | 16 | 74HC4051 analog mux, powered from 3.3 V | 8 muxes per 64-point IN bank; AT32 GPIO drives address/enable directly |
| Legacy connector group | `JOLD1` ... `JOLD4` | 4 | DB50 connector, 2x25 contacts | Upper row pins 1-24 are OUT, lower row pins 1-24 are IN, row pin 25 NC |
| Series protection | `R_OUT001` ... `R_OUT096` | 96 | Series resistor | Value TBD, start from 1 kOhm for protection review |
| Series protection | `R_IN001` ... `R_IN096` | 96 | Series resistor | Value TBD, start from 1 kOhm for protection review |
| ESD/protection | `D_JOLDx_*` | TBD | TVS/ESD array or discrete device | Per connector group or per signal cluster |
| Power | `U_3V3`, `L*`, `C*` | TBD | 3.3 V regulator and decoupling | Follow current IO board power plan |
| Debug | `J_SWD` | 1 | SWD header | Keep compatible with AT-Link/ICE |
| LED display interface | `J_LED7SEG` | 1 | LED seven-segment display module connector | Suggested pins: 3.3 V, GND, CLK, DIO_ERR, DIO_CUR |
| LED display module | `MOD_LED7SEG_A`, `MOD_LED7SEG_B` | 2 | TM1637 4-digit seven-segment module, 3.3 V capable | Recommended default; gives 8 digits total for error/current point display |
| LED display driver option | `U_LED7SEG` | DNP/TBD | HT16K33/TM1638/MAX7219 alternative driver | Only fit if replacing the default TM1637 module approach |
| Host communication option | `J_RPI_COMM`, `R_SEL*` | DNP/TBD | Raspberry Pi communication option | Do not populate for old single-MCU DB50 build |
| Key/buzzer | `SW*`, `BZ*` | TBD | Local keys and buzzer | Keep for standalone variant |
| Mounting/shield | `H*`, `SH*` | TBD | Mounting holes / shield contacts | Decide chassis grounding strategy |

## Connector quantity note

One DB50 connector is one physical connector with two corresponding rows:

```text
4 interface groups x 1 DB50 connector = 4 physical DB50 connectors.
```

Use one physical DB50 connector per interface group. If the schematic library
uses multi-unit symbols, keep the symbol units under one designator, for
example `JOLD1A/JOLD1B` only if the BOM still resolves to one physical `JOLD1`.

## Do not populate

| Designator/pin | Qty | Reason |
|---|---:|---|
| `JOLD1` upper/lower row pin 25 | 2 contacts | Reserved/NC |
| `JOLD2` upper/lower row pin 25 | 2 contacts | Reserved/NC |
| `JOLD3` upper/lower row pin 25 | 2 contacts | Reserved/NC |
| `JOLD4` upper/lower row pin 25 | 2 contacts | Reserved/NC |
| `OUT097` ... `OUT128` | 32 logical points | Unused in old DB50 profile |
| `IN097` ... `IN128` | 32 logical points | Unused in old DB50 profile |
