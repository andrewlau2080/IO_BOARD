# DB78 64x4 variant BOM baseline

This BOM is a baseline for the DB78 connector variant. Use the existing DB78
specification as the authority for final connector model, footprint, and pin
assignment.

| Block | Designator pattern | Qty | Item | Notes |
|---|---:|---:|---|---|
| MCU | `U_MCU` | 1 | AT32F455VET7, LQFP100 | Shared with current firmware |
| Output mux | `U_OUT_Ax`, `U_OUT_Bx` | 16 | 74HC4051 analog mux, powered from 3.3 V | 8 muxes per 64-point OUT bank; AT32 GPIO drives address/enable directly |
| Input mux | `U_IN_Ax`, `U_IN_Bx` | 16 | 74HC4051 analog mux, powered from 3.3 V | 8 muxes per 64-point IN bank; AT32 GPIO drives address/enable directly |
| OUT connector | `JOUT1`, `JOUT2` | 2 | DB78 connector | 64 pins used per connector |
| IN connector | `JIN1`, `JIN2` | 2 | DB78 connector | 64 pins used per connector |
| Series protection | `R_OUT001` ... `R_OUT128` | 128 | Series resistor | Value TBD |
| Series protection | `R_IN001` ... `R_IN128` | 128 | Series resistor | Value TBD |
| ESD/protection | `D_JOUT*`, `D_JIN*` | TBD | TVS/ESD array or discrete device | Per connector or per signal cluster |
| Power | `U_3V3`, `L*`, `C*` | TBD | 3.3 V regulator and decoupling | Follow current IO board power plan |
| Debug | `J_SWD` | 1 | SWD header | Keep compatible with AT-Link/ICE |
| Host communication | `J_RPI_COMM` | 1 | Raspberry Pi communication connector | Uses the shared communication/display port |
| LCDM/screen | Raspberry Pi side | TBD | LCDM/display driven by Raspberry Pi | Not driven directly by AT32 in Pi variant |
| LED display option | `J_LED7SEG`, `U_LED7SEG` | DNP/TBD | LED seven-segment display option | Do not populate in normal Pi build |
| Key/buzzer | `SW*`, `BZ*` | TBD | Local keys and buzzer | Optional fallback/local controls |

## Reserved pins

Each DB78 connector has 14 unused/reserved pins in the 64-pin mapping. Do not
assign these pins casually; reserve them for one of these verified purposes:

- Shield/chassis strategy.
- Fixture ID.
- Future expansion.
- Keying/detection.
- NC for compatibility.
