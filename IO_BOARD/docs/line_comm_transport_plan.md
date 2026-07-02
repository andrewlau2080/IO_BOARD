# Line Communication Transport Plan

This layer keeps the production-line trigger independent from the actual
physical link. Scan and print modules should not directly depend on IR, WiFi,
RS485, LoRa, or any other transport.

## Current Production Flow

The production line has up to 10 moving tester stations and one fixed print
terminal. A tester station completes assembly and test while rotating through
the line. When that station reaches the print position, a sensor or equivalent
condition allows the tester to trigger printing. The print terminal receives
the request and prints the preset LCDM label through the selected printer
backend.

```text
Tester station 1-10
  -> line_comm_transport
  -> print terminal receiver
  -> LCDM label state / preset template
  -> Zebra ZPL RS485 printer driver
```

## Current Firmware Status

| Role | Current implementation |
|---|---|
| Scanner IO module | After PASS, sends a print request through `line_comm_transport_send_print_request()` |
| Print module | Polls `line_comm_transport_poll_print_request()` and prints the current label when a request is received |
| First backend | IR trigger using the existing learned `LINE_COMM_CODE_PRINT_REQUEST` waveform |
| Future backend | Wireless UART / WiFi / Sub-GHz / LoRa / RS485 can be added under the same API |

The current IR trigger is a fixed waveform only. It means "print now" and does
not yet carry station/model/result data. The print terminal therefore uses the
current LCDM label state. A future packet transport can serialize
`line_comm_print_request_t` to carry station, product ID, test count, and result.

## API

| Function | Purpose |
|---|---|
| `line_comm_transport_init()` | Select and initialize the physical transport |
| `line_comm_transport_send_print_request()` | Tester sends a print trigger |
| `line_comm_transport_poll_print_request()` | Print terminal polls for a trigger |

## Station Pairing Direction

Tester stations have LEDM and keys only; the print terminal has LCDM and should
act as the line master. The agreed pairing operation is:

1. The tester enters pairing by long-pressing `K1 self-test + K4 confirm`.
2. The tester shows `PAIR` and a short device code on LEDM.
3. The print terminal LCDM opens the device pairing menu.
4. The operator selects the requesting device and binds/replaces `ST01`-`ST10`.
5. The master sends station assignment and the current product/profile data.
6. The tester acknowledges saving and shows `P-xx`.

Detailed pairing and replacement planning is tracked in
`docs/station_pairing_plan.md`.

## Wireless Recommendation

For this rotating 10-station line, a wireless backend is often more convenient
than IR because it avoids line-of-sight alignment and fixture-position tolerance
issues. Recommended order:

| Option | Recommendation |
|---|---|
| ESP32/ESP8266 WiFi | Best if future MAS/MES, dashboard, mobile/web app, or production records are planned |
| Sub-GHz transparent UART | Best for robust local trigger when no network/MES is needed |
| LoRa transparent UART | Good for long range or high interference, but slower and still needs an antenna form |
| IR | Good as a simple close-range fallback, but sensitive to alignment and blockage |
| Wired RS485 | Most deterministic if slip ring/cable routing is practical |

For future MAS/MES, prefer WiFi with a real packet protocol rather than only a
transparent trigger. First practical packet can be ASCII or JSON-like:

```text
IOBRD,PRINT,ST=01,PASS=1,COUNT=123,MODEL=MODEL-A,CODE=A1B1-000001
```

The scanner and print modules should continue using the transport API so the
physical link can change without rewriting test or print logic.
