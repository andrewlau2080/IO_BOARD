# DB78 64x4 IO board specification summary

This directory is for the newer DB78 connector variant. The detailed
specification already exists outside this summary; keep its schematic, PCB, and
BOM files here as the design matures.

Firmware remains in the parent `IO_BOARD` project.

## Display and host role

This variant is the preferred Raspberry Pi + MCU direction. The Raspberry Pi
drives LCDM/screen and higher-level UI. The AT32F455 remains the scan/IO device
and communicates scan results through the shared communication/display port.

Recommended profile:

```c
#define IO_COMM_MODE_RPI_HOST
```

Do not route LCDM as an AT32-driven display requirement for this variant unless
a fallback standalone build is explicitly needed.

## External connector structure

This variant separates input and output connectors:

| Connector | Direction | Pins used | Logical range | Mux usage |
|---|---|---:|---:|---|
| `JOUT1` | OUT | 64 of 78 | `OUT001`-`OUT064` | `IO_MUX_OUT_A[0..63]` |
| `JOUT2` | OUT | 64 of 78 | `OUT065`-`OUT128` | `IO_MUX_OUT_B[0..63]` |
| `JIN1` | IN | 64 of 78 | `IN001`-`IN064` | `IO_MUX_IN_A[0..63]` |
| `JIN2` | IN | 64 of 78 | `IN065`-`IN128` | `IO_MUX_IN_B[0..63]` |

Total useful test points:

- 128 output points.
- 128 input points.
- 256 external test contacts total.

Each DB78 connector uses 64 pins and leaves 14 pins reserved/NC unless the
final specification assigns them to shield, ID, spare, or fixture detection.

## Circuit separation

Compared with the legacy DB50 variant, this connector plan is cleaner for PCB
layout because OUT and IN are separated physically:

- OUT connector routing can stay near output mux banks.
- IN connector routing can stay near input mux banks.
- 64-pin grouping aligns directly with the CD4051 bank capacity.
- Firmware mapping is direct and requires no unused middle ranges.

## Deliverables in this directory

- `README.md`: DB78 hardware summary.
- `BOM.md`: DB78 BOM baseline.
- Future schematic/PCB files for this variant should stay here.
