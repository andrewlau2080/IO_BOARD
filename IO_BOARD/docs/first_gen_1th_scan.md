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
| Current status | Implemented and build-verified; GitHub commit `8a5d187` |

## Circuit Mapping

| Half | Points | Excitation | Sense | Firmware bank |
|---|---:|---|---|---|
| A | 1-48 | `DAC_OUT1` = `PA4` | `ADC1_IN0` = `PA0` | OUT_A / IN_A |
| B | 49-96 | `DAC_OUT2` = `PA5` | `ADC2_IN2` = `PA2` | OUT_B / IN_B |

## Scan Method

| Step | Operation | Output |
|---:|---|---|
| 1 | Initialize CD4051 GPIO, DAC1/DAC2, ADC1/ADC2, and TM1637 LED module | Board ready |
| 2 | Select one OUT point and one IN point through CD4051 banks | One candidate path connected |
| 3 | Wait mux settle time, then read ADC based on selected IN half | ADC code |
| 4 | Compare ADC code with `g_first_gen_adc_threshold` | Connected / open |
| 5 | Press `ENTER` or TM1637 `SET` with a known-good harness connected | Learn and save the current matrix |
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
| `g_first_gen_learn_status` | `1=saved`, `2=scan failed`, `3=Flash write failed` |
| `g_first_gen_last_pass` | Last full scan result |

## Self-Learn And Display Mode

The first-gen local firmware can learn from a known-good harness. Connect the
standard harness and press `ENTER` on the board key or `SET` on the TM1637
module. The firmware scans the full 96 x 96 matrix and saves that matrix as the
standard in internal Flash.

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

## Completed Behavior Record

| Item | Current behavior |
|---|---|
| Standard creation | Connect a known-good harness, press `ENTER` or TM1637 `SET`, then save the scanned 96 x 96 matrix to Flash |
| Multi-branch support | One-to-two and one-to-many branches are accepted when they exist in the learned standard matrix |
| Normal scan display | Left three digits show current OUT point; right three digits show the first problem IN point |
| Normal point behavior | Display advances quickly to the next OUT point |
| Problem point behavior | Firmware stays on the current OUT/IN pair and keeps rechecking the same row |
| Continue condition | After the current problem is cleared, the same row passes and firmware continues scanning the next OUT point |
| No learned standard | Display shows `LEArn`; normal PASS/NG comparison is blocked |

## Next Step: Learned IR Print Link

| Step | Purpose | Planned firmware work |
|---:|---|---|
| 1 | Use local scan result as the print trigger | After a full learned-matrix scan passes, set a local print-ready state |
| 2 | Reuse learned IR receive code | Listen for the printer/terminal polling prefix on the IR receive pin |
| 3 | Reuse learned IR transmit code | Send the learned tester response code after the polling packet is detected |
| 4 | Carry result data | Start with PASS/NG and current station/test result; extend later if old protocol supports more fields |
| 5 | Failure handling | Do not send print response while the scan is stopped on an unresolved problem point |

## Build

```sh
cmake -S IO_BOARD -B IO_BOARD/build-first-gen -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=FIRST_GEN_4051_LOCAL
cmake --build IO_BOARD/build-first-gen
```
