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
| 5 | Full matrix scan checks 96 expected 1:1 pairs and all non-expected pairs | PASS or first fail point |

## Runtime Watch Variables

| Variable | Meaning |
|---|---|
| `g_first_gen_last_adc1` / `g_first_gen_last_adc2` | Latest ADC raw values |
| `g_first_gen_adc_threshold` | Debug-adjustable connected threshold |
| `g_first_gen_first_fail_out` / `g_first_gen_first_fail_in` | First failing matrix coordinate |
| `g_first_gen_missing_counter` | Expected 1:1 pairs not detected |
| `g_first_gen_unexpected_counter` | Unexpected cross-connections detected |
| `g_first_gen_last_pass` | Last full scan result |

## Build

```sh
cmake -S IO_BOARD -B IO_BOARD/build-first-gen -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=FIRST_GEN_4051_LOCAL
cmake --build IO_BOARD/build-first-gen
```
