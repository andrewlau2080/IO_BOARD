# IO_BOARD

FOR UPDATE THE ST1 OR ST2 TESTER.

Main firmware project:

- `IO_BOARD/`

Current confirmed bench status:

- Printer-side polling IR transmitter is working.
- `PA7` outputs the decoded 71-segment polling packet.
- `PH3 DEBUG_OUT` is the oscilloscope envelope reference: high during the about
  36.7 ms packet, low during the about 166.4 ms inter-packet space.

Build from the project directory:

```sh
cd IO_BOARD
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```
