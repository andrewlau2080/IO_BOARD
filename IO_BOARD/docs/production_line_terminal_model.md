# Production Line Terminal And Print Model

This document models the next two development blocks before the old first-gen
tester communication protocol is fully decoded.

## 1. Production Line Communication Placeholder

The production line has up to 10 tester stations. Each tester normally runs its
own local IO scan. The line is a rotating loop. After an operator finishes a
board, the fixture moves to a terminal/print position. When the current tester
is detected at that position, it sends a print request to the terminal unit.

Current implementation rule:

- The printer-side polling/trigger packet has been decoded from
  `TestV1.0/analysis_outputs/useful_printer_trigger_tx_envelope.csv`.
- `src/line_comm_bridge.c` now contains this 71-segment packet as
  `LINE_COMM_CODE_PRINT_REQUEST`.
- The current firmware main loop continuously transmits this printer-side
  polling packet from `PA7` as a test signal for the tester side.
- Verified on 2026-06-03: oscilloscope testing confirmed the printer-side
  polling transmitter outputs the intended packet envelope. The previous
  continuous-carrier symptom was fixed by using DWT-based microsecond timing
  for MARK/SPACE generation and by forcing the inter-packet SPACE low.
- Other old-link response/ack code slots remain empty until they are learned.
- Keep local tester logic independent from the terminal protocol so scanning
  and UI work can continue before the old protocol is decoded.

Firmware placeholder files:

| File | Role |
|---|---|
| `inc/line_comm_bridge.h` | Production-line communication data types and API |
| `src/line_comm_bridge.c` | Empty learned-code slots for print request/ack/busy/done |

Current reserved old-link code slots:

| Code ID | Purpose | Current status |
|---|---|---|
| `LINE_COMM_CODE_PRINT_REQUEST` | Printer polling/trigger packet for tester side | Filled, 71 MARK/SPACE segments |
| `LINE_COMM_CODE_PRINT_ACK` | Terminal accepts request | Empty timing table |
| `LINE_COMM_CODE_PRINT_BUSY` | Terminal is busy | Empty timing table |
| `LINE_COMM_CODE_PRINT_DONE` | Print job completed | Empty timing table |

Current printer polling transmitter settings:

| Item | Value |
|---|---:|
| Source analysis file | `TestV1.0/analysis_outputs/useful_printer_trigger_tx_envelope.csv` |
| Segment count | 71 |
| Packet duration | about 36.695 ms |
| Repeat period | about 203.147 ms |
| Inter-packet space | about 166.452 ms |
| Carrier half period | 12 us |
| Estimated carrier | about 41.7 kHz |
| Firmware output pin | `PA7` / `IR_TX` |
| Envelope debug pin | `PH3` / `DEBUG_OUT`, high during packet and low during inter-packet space |
| Bench verification | Confirmed normal on 2026-06-03 |

When the learned IR frame is available, fill the placeholder like this:

```c
static const uint16_t print_request_code_us[] = {
  9000, 4500, 560, 560, /* ... learned timings ... */
};

const line_comm_ir_code_t g_line_comm_ir_codes[LINE_COMM_CODE_COUNT] = {
  {0U, sizeof(print_request_code_us) / sizeof(print_request_code_us[0]), print_request_code_us},
  /* ... */
};
```

## 2. Terminal Printer Architecture

The terminal should not try to drive every printer directly from AT32 firmware.
Use a Raspberry Pi or Linux terminal as the print server:

```text
Tester AT32 boards
  -> old learned line protocol / future UART-RS485-CAN
  -> Terminal controller
  -> CUPS / IPP / printer driver layer
  -> USB / Ethernet / Wi-Fi printer
```

Recommended approach:

| Layer | Responsibility |
|---|---|
| Tester AT32 | Scan result, station ID, product ID, print request trigger |
| Terminal app | Dialog/input UI, label template, job queue, print history |
| CUPS/Linux print system | Printer discovery, driverless IPP, vendor driver support |
| Printer | Actual output device |

This is the only practical way to support a wide printer range. Direct MCU
printer support should be limited to known label-printer languages such as
ESC/POS, TSPL, ZPL, or CPCL after the exact printer family is selected.

## 3. Print Job Data Model

Firmware placeholder files:

| File | Role |
|---|---|
| `inc/print_job_model.h` | Common label fields and sizes |
| `src/print_job_model.c` | ASCII label formatting helper |

Initial fields:

| Field | Example | Notes |
|---|---|---|
| `title` | `HARNESS TEST` | Label header |
| `item` | `MODEL-A` | Product/model name |
| `content` | `LINE 01 STATION 03` | Body/content |
| `code` | `A1B1-000001` | Human-readable code or barcode text |
| `quantity` | `1` | Quantity/count |
| `pass` | `PASS` | Test result |

For the MCU-side model, keep the first stage ASCII-only because it is simple,
stable, and compact. The terminal UI can use UTF-8 and real fonts when printing
PDF or raster output through CUPS.

## 4. UI Model

The first visual model is:

| File | Role |
|---|---|
| `terminal/print_terminal_mockup.html` | Browser-based terminal dialog and label preview |

The mockup has:

- Station selection for 10 tester stations.
- Title, item/model, content, code, quantity, result, and copies fields.
- Label preview similar to a product label.
- Print queue preview.

This is not the final terminal program. It fixes the field layout and workflow
before choosing the final UI technology.

## 5. Future Runtime Flow

```text
1. Tester completes local board logic.
2. Fixture reaches print terminal sensor position.
3. Tester sends PRINT_REQUEST.
4. Terminal receives request and loads the current label template.
5. Operator can edit title/item/content/code through terminal key input.
6. Terminal prints through CUPS.
7. Terminal sends PRINT_DONE or PRINT_BUSY/ERROR to tester.
```

## 6. Open Decisions

| Decision | Current placeholder |
|---|---|
| Old first-gen protocol waveform | Empty arrays in `line_comm_bridge.c` |
| Print terminal hardware | Raspberry Pi/Linux terminal recommended |
| Exact printer class | CUPS first; direct ESC/POS/TSPL/ZPL only after printer is fixed |
| Label size | Mockup default; final size to be updated after printer/label media selection |
| Key input method | Independent module later |
