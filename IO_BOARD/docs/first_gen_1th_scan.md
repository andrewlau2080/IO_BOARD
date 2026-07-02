# First-Gen 1TH Local Scan Plan

## Summary

| Item | Decision |
|---|---|
| Source schematic | `IO_BOARD/1thsch.pdf`, title `1TH_NEW`, date `2026-06-07` |
| Product mode | First-generation local MCU scan board |
| Shift register | Not used in this schematic; no 74LS164 planning is applied |
| Mux control | AT32 directly drives CD4051 address and enable nets |
| Firmware mode | `FIRST_GEN_4051_LOCAL` |
| Scan profile | `first_gen_1th_96x96` |
| Standard storage | Self-learned matrix saved in the last Flash sector |
| Current status | Local scan and self-learn storage are implemented; GPIO mapping is corrected to `1thsch.pdf` MCU2; display/print external wiring still needs schematic migration |

## Circuit Mapping

| Half | Points | Excitation | Sense | Firmware bank |
|---|---:|---|---|---|
| A | 1-48 | `DAC_OUT1` = `PA4` | `ADC1_IN0` = `PA0` | OUT_A / IN_A |
| B | 49-96 | `DAC_OUT2` = `PA5` | `ADC2_IN2` = `PA2` | OUT_B / IN_B |

## MCU2 GPIO Mapping From `1thsch.pdf`

The 4051 mapping below is taken from page 4 of `1thsch.pdf`.

| Net group | MCU pins |
|---|---|
| `IN_AMUX_A0/B1/C2` | `PC0` / `PC1` / `PC2` |
| `IN_AMUX_EN0..EN7` | `PE0`, `PE1`, `PE2`, `PE3`, `PE4`, `PE5`, `PE6`, `PE7` |
| `IN_BMUX_A0/B1/C2` | `PB0` / `PB1` / `PB2` |
| `IN_BMUX_EN0..EN7` | `PE8`, `PE9`, `PE10`, `PE11`, `PE12`, `PE13`, `PE14`, `PE15` |
| `OUT_AMUX_A0/B1/C2` | `PC10` / `PC11` / `PC12` |
| `OUT_AMUX_EN0..EN7` | `PD0`, `PD1`, `PD2`, `PD3`, `PD4`, `PD5`, `PD6`, `PD7` |
| `OUT_BMUX_A0/B1/C2` | `PB10` / `PB11` / `PB12` |
| `OUT_BMUX_EN0..EN7` | `PD9`, `PD10`, `PD11`, `PD12`, `PD13`, `PD14`, `PD15`, `PC6` |
| Keys | `KEY_ESC=PC4`, `KEY_OK=PC5`, `KEY_RIGHT=PC7`, `KEY_LEFT=PC8`, `KEY_DOWN=PC9`, `KEY_UP=PA8` |

## Scan Method

| Step | Operation | Output |
|---:|---|---|
| 1 | Initialize CD4051 GPIO, DAC1/DAC2, ADC1/ADC2, and board keys | Board ready |
| 2 | Select one OUT point and one IN point through CD4051 banks | One candidate path connected |
| 3 | Wait mux settle time, then read ADC based on selected IN half | ADC code |
| 4 | Compare ADC code with `g_first_gen_adc_threshold` | Connected / open |
| 5 | Press board `ENTER` with a known-good harness connected | Learn and save the current matrix |
| 6 | Normal scans compare the actual row with the saved matrix | PASS or current problem point |

## Runtime Watch Variables

| Variable | Meaning |
|---|---|
| `g_first_gen_last_adc1` / `g_first_gen_last_adc2` | Latest ADC raw values |
| `g_first_gen_adc_threshold` | Debug-adjustable connected threshold |
| `g_first_gen_first_fail_out` / `g_first_gen_first_fail_in` | First failing matrix coordinate |
| `g_first_gen_missing_counter` | Expected 1:1 pairs not detected |
| `g_first_gen_unexpected_counter` | Unexpected cross-connections detected |
| `g_first_gen_current_out` / `g_first_gen_current_problem_in` | Current display pair; left current OUT, right problem IN |
| `g_first_gen_recipe_valid` | Learned standard matrix is valid |
| `g_first_gen_learn_status` | `1=saved`, `2=scan failed`, `3=Flash write failed`, `4=abnormal learned connection count; not saved` |
| `g_first_gen_learn_connected_pairs` | Number of connected matrix points detected during the latest learn scan |
| `g_first_gen_last_connected_pairs` | Latest scan connected-point counter for ADC/mux diagnosis |
| `g_first_gen_learn_out_count` / `g_first_gen_learn_in_count` | Number of OUT and IN points involved in the latest learn preview |
| `g_first_gen_learn_pending` | Learn preview is waiting for K4 confirmation before Flash save |
| `g_first_gen_last_pass` | Last full scan result |
| `g_first_gen_print_ready` | Full learned-matrix scan passed and print response is armed |
| `g_first_gen_print_waiting_for_poll` | Firmware is waiting for the printer polling prefix |
| `g_first_gen_print_response_ready` | Learned tester response waveform is present in firmware |
| `g_first_gen_print_poll_match_counter` | Printer polling prefixes accepted |
| `g_first_gen_print_poll_reject_counter` | Captured prefixes rejected by timing match |
| `g_first_gen_print_response_counter` | Tester response transmissions sent |
| `g_first_gen_print_blocked_counter` | Print readiness cleared by a scan failure |

## Self-Learn And Display Mode

The first-gen local firmware can learn from a known-good harness. Connect the
standard harness and press `ENTER` on the board key. The firmware scans the
full 96 x 96 matrix and saves that matrix as the standard in internal Flash.

This supports one-to-two and one-to-many harnesses because the saved standard
is the whole connection matrix, not a fixed one-to-one rule.

Normal testing scans by OUT point. The left three display digits show the
current OUT point. The right three digits show the first problem IN point. If a
row has no issue, the display advances quickly. If a row has an open, wrong, or
extra connection, the display stays on that OUT/IN pair and keeps rechecking
the same row. After the problem is cleared, the firmware continues scanning the
next OUT point.

If no valid standard is stored, the display shows `LEArn` and normal PASS/NG
testing is blocked until learning succeeds.

## TM1637 Front-Panel Keys

The current new-tester front panel uses the TM1637 module key scan result, not
the fixture schematic `KEY_OK/KEY_ESC/...` GPIO keys.

| TM1637 key | Raw key | Firmware function |
|---|---:|---|
| K1 short press | `0xF3` | Self test: display-only sequence `A01b01` to `A92b92`, then `StPASS` |
| K1 long press 3 s | `0xF3` held | Learn and save the current known-good harness matrix to Flash |
| K2 | `0xF4` | Auto test: start normal learned-matrix scan |
| K3 | `0xF5` | Reset/restart; also cancels a pending learn preview |
| K4 | `0xF0` | Confirm key; saves a pending learn preview to Flash |

On power-up the firmware initializes the display and waits for a TM1637 key. If
a learned standard exists, K2 starts the normal scan. If no learned standard is
stored, K2 shows `LEArn` and does not run PASS/NG comparison.

The current display code still uses the earlier AT-START bench-test TM1637 pins
`PA9/PA10`. In `1thsch.pdf`, `PA9/PA10` are routed to the `USART` header and the
local display connector is `H10 LCDM` on `LCM_*` nets. The display firmware must
therefore be migrated to the LCDM interface, or the schematic must add the
TM1637 wiring, before this display flow is valid on the 1TH PCB.

## Completed Behavior Record

| Item | Current behavior |
|---|---|
| Standard creation | Connect a known-good harness, hold TM1637 K1 for 3 seconds to scan and preview counts; display left 3 digits = learned OUT count and right 3 digits = learned IN count; press K4 to save to Flash; saved state shows `SAUEd` |
| Multi-branch support | One-to-two and one-to-many branches are accepted when they exist in the learned standard matrix |
| Normal scan display | Left three digits show current OUT point; right three digits show the first problem IN point |
| Normal point behavior | Display advances quickly to the next OUT point |
| Problem point behavior | Firmware stays on the current OUT/IN pair and keeps rechecking the same row |
| Continue condition | After the current problem is cleared, the same row passes and firmware continues scanning the next OUT point |
| PASS hold | After a full PASS, firmware waits up to 5 seconds for the print trigger, then stays on PASS/print status |
| Automatic next test | After PASS, the firmware checks learned connection points; when all wires are open for 10 seconds, it treats the harness as removed and starts the next scan cycle |
| No learned standard | Display shows `LEArn`; normal PASS/NG comparison is blocked |
| Display code | TM1637 display code exists but is not matched to `1thsch.pdf` wiring |
| Print code | PASS-gated IR print response code exists but is not matched to `1thsch.pdf` wiring |
| Print block | Unlearned state, unresolved NG row, or missing tester response waveform blocks the carried-over print response code |

## Learned IR Print Link

| Step | Purpose | Firmware behavior |
|---:|---|---|
| 1 | Use local scan result as the print trigger | After a full learned-matrix scan passes, `g_first_gen_print_ready` is set |
| 2 | Reuse learned IR receive code | Firmware listens for the printer/terminal polling prefix on `PA6` |
| 3 | Reuse learned IR transmit code | Firmware sends `LINE_COMM_CODE_TESTER_RESPONSE` on `PA7` after the expected delay |
| 4 | Carry result data | Current waveform is the learned legacy tester response; richer station/test data waits for old protocol decoding |
| 5 | Failure handling | No response is sent while the scan is stopped on an unresolved problem point |

The current IR print link code still uses the earlier learned-capture pins
`PA6=IR_RX` and `PA7=IR_TX`. In `1thsch.pdf`, those MCU pins are not shown as IR
receive/transmit nets. Treat the PASS-gated IR response as carried-over logic
that needs final connector/pin assignment before bench testing on the 1TH PCB.

## Build

```sh
cmake -S IO_BOARD -B IO_BOARD/build-first-gen -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=FIRST_GEN_4051_LOCAL
cmake --build IO_BOARD/build-first-gen
```
