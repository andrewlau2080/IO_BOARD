cmake_minimum_required(VERSION 3.20)

project(io_board_at32f455 C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(AT32_BSP_DIR "${CMAKE_CURRENT_LIST_DIR}/../.vendor/AT32F45x_Firmware_Library_V2.0.1" CACHE PATH "AT32F45x firmware library path")
set(LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/AT32F455xE_FLASH.ld")
set(LINE_COMM_TESTER_RESPONSE_CSV
  "${CMAKE_CURRENT_LIST_DIR}/../TestV1.0/analysis_outputs/useful_tester_response_tx_envelope.csv")
set(LINE_COMM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LINE_COMM_GENERATED_C "${LINE_COMM_GENERATED_DIR}/generated_line_comm_codes.c")
set(LINE_COMM_GENERATED_H "${LINE_COMM_GENERATED_DIR}/generated_line_comm_codes.h")
set(WIFI_NET_DIAG_CONFIG_IN "${CMAKE_CURRENT_LIST_DIR}/cmake/wifi_net_diag_config.h.in")
set(WIFI_NET_DIAG_CONFIG_H "${LINE_COMM_GENERATED_DIR}/wifi_net_diag_config.h")
set(IO_APP_MODE "RPI_RS485_LEGACY" CACHE STRING "Firmware app mode: RPI_RS485_LEGACY, IR_PRINT_BRIDGE, FIRST_GEN_4051_LOCAL, TM1637_DEMO, PA4_GPIO_DIAG, PA4_DAC_DIAG, MUX_A_DIAG, PRINT_TERMINAL, UNIFIED, LCDM_TJC_VERIFY, WIFI_LINK_DIAG, WIFI_NET_DIAG, or ESP_AT_FLASH_ASSIST")
set(UNIFIED_DEFAULT_PRODUCT_MODE "FIRST_GEN_4051_LOCAL" CACHE STRING "Default product when IO_APP_MODE=UNIFIED: FIRST_GEN_4051_LOCAL, PRINT_TERMINAL, or RPI_RS485_LEGACY")
set(PRINT_DRIVER_BACKEND "MOCK" CACHE STRING "Print backend: MOCK or ZPL_RS485")
set(FIRST_GEN_DISPLAY_BACKEND "LEDM" CACHE STRING "First-gen tester display backend: LEDM or LCDM")
set(FIRST_GEN_DISPLAY_AUTO_DETECT "ON" CACHE BOOL "Probe LCDM first, then fall back to LEDM/TM1637")
set(TESTER_WIFI_PRINT_UART_BAUDRATE "115200" CACHE STRING "LCDM tester ESP32-C3 software UART baudrate")
set(WIFI_NET_DIAG_SSID "" CACHE STRING "WIFI_NET_DIAG test AP SSID (leave empty for status-only mode)")
set(WIFI_NET_DIAG_PASSWORD "" CACHE STRING "WIFI_NET_DIAG test AP password (never commit this cache/build directory)")

# Keep credentials out of tracked source and out of the compiler command line.
# They are copied only into the selected local build's generated header.
file(MAKE_DIRECTORY "${LINE_COMM_GENERATED_DIR}")
set(WIFI_NET_DIAG_SSID_C "${WIFI_NET_DIAG_SSID}")
set(WIFI_NET_DIAG_PASSWORD_C "${WIFI_NET_DIAG_PASSWORD}")
string(REPLACE "\\" "\\\\" WIFI_NET_DIAG_SSID_C "${WIFI_NET_DIAG_SSID_C}")
string(REPLACE "\"" "\\\"" WIFI_NET_DIAG_SSID_C "${WIFI_NET_DIAG_SSID_C}")
string(REPLACE "\\" "\\\\" WIFI_NET_DIAG_PASSWORD_C "${WIFI_NET_DIAG_PASSWORD_C}")
string(REPLACE "\"" "\\\"" WIFI_NET_DIAG_PASSWORD_C "${WIFI_NET_DIAG_PASSWORD_C}")
configure_file("${WIFI_NET_DIAG_CONFIG_IN}" "${WIFI_NET_DIAG_CONFIG_H}" @ONLY)

if(NOT EXISTS "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/at32f45x.h")
  message(FATAL_ERROR "AT32 BSP not found at ${AT32_BSP_DIR}")
endif()

add_custom_command(
  OUTPUT "${LINE_COMM_GENERATED_C}" "${LINE_COMM_GENERATED_H}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LINE_COMM_GENERATED_DIR}"
  COMMAND ${CMAKE_COMMAND}
          -DINPUT_CSV=${LINE_COMM_TESTER_RESPONSE_CSV}
          -DOUTPUT_C=${LINE_COMM_GENERATED_C}
          -DOUTPUT_H=${LINE_COMM_GENERATED_H}
          -P "${CMAKE_CURRENT_LIST_DIR}/cmake/generate_ir_code.cmake"
  DEPENDS
    "${LINE_COMM_TESTER_RESPONSE_CSV}"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/generate_ir_code.cmake"
  VERBATIM
)

set(AT32_DRIVER_SOURCES
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_adc.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_can.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_crm.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_dac.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_exint.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_flash.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_gpio.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_misc.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_pwc.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_scfg.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_spi.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_tmr.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_usart.c"
)

if(IO_APP_MODE STREQUAL "RPI_RS485_LEGACY")
  set(IO_APP_MODE_VALUE 2)
elseif(IO_APP_MODE STREQUAL "IR_PRINT_BRIDGE")
  set(IO_APP_MODE_VALUE 1)
elseif(IO_APP_MODE STREQUAL "FIRST_GEN_4051_LOCAL")
  set(IO_APP_MODE_VALUE 3)
elseif(IO_APP_MODE STREQUAL "TM1637_DEMO")
  set(IO_APP_MODE_VALUE 4)
elseif(IO_APP_MODE STREQUAL "PA4_GPIO_DIAG")
  set(IO_APP_MODE_VALUE 5)
elseif(IO_APP_MODE STREQUAL "PA4_DAC_DIAG")
  set(IO_APP_MODE_VALUE 6)
elseif(IO_APP_MODE STREQUAL "MUX_A_DIAG")
  set(IO_APP_MODE_VALUE 7)
elseif(IO_APP_MODE STREQUAL "PRINT_TERMINAL")
  set(IO_APP_MODE_VALUE 8)
elseif(IO_APP_MODE STREQUAL "UNIFIED")
  set(IO_APP_MODE_VALUE 9)
elseif(IO_APP_MODE STREQUAL "LCDM_TJC_VERIFY")
  set(IO_APP_MODE_VALUE 10)
elseif(IO_APP_MODE STREQUAL "WIFI_LINK_DIAG")
  set(IO_APP_MODE_VALUE 11)
elseif(IO_APP_MODE STREQUAL "WIFI_NET_DIAG")
  set(IO_APP_MODE_VALUE 13)
elseif(IO_APP_MODE STREQUAL "ESP_AT_FLASH_ASSIST")
  set(IO_APP_MODE_VALUE 12)
else()
  message(FATAL_ERROR "Unsupported IO_APP_MODE=${IO_APP_MODE}")
endif()

if(UNIFIED_DEFAULT_PRODUCT_MODE STREQUAL "RPI_RS485_LEGACY")
  set(UNIFIED_DEFAULT_PRODUCT_MODE_VALUE 2)
elseif(UNIFIED_DEFAULT_PRODUCT_MODE STREQUAL "FIRST_GEN_4051_LOCAL")
  set(UNIFIED_DEFAULT_PRODUCT_MODE_VALUE 3)
elseif(UNIFIED_DEFAULT_PRODUCT_MODE STREQUAL "PRINT_TERMINAL")
  set(UNIFIED_DEFAULT_PRODUCT_MODE_VALUE 8)
else()
  message(FATAL_ERROR "Unsupported UNIFIED_DEFAULT_PRODUCT_MODE=${UNIFIED_DEFAULT_PRODUCT_MODE}")
endif()

if(PRINT_DRIVER_BACKEND STREQUAL "MOCK")
  set(PRINT_DRIVER_BACKEND_ZPL_RS485_VALUE 0)
elseif(PRINT_DRIVER_BACKEND STREQUAL "ZPL_RS485")
  set(PRINT_DRIVER_BACKEND_ZPL_RS485_VALUE 1)
else()
  message(FATAL_ERROR "Unsupported PRINT_DRIVER_BACKEND=${PRINT_DRIVER_BACKEND}")
endif()

if(FIRST_GEN_DISPLAY_BACKEND STREQUAL "LEDM")
  set(FIRST_GEN_DISPLAY_BACKEND_LCDM_VALUE 0)
elseif(FIRST_GEN_DISPLAY_BACKEND STREQUAL "LCDM")
  set(FIRST_GEN_DISPLAY_BACKEND_LCDM_VALUE 1)
else()
  message(FATAL_ERROR "Unsupported FIRST_GEN_DISPLAY_BACKEND=${FIRST_GEN_DISPLAY_BACKEND}")
endif()

if(NOT TESTER_WIFI_PRINT_UART_BAUDRATE MATCHES "^[0-9]+$" OR
   TESTER_WIFI_PRINT_UART_BAUDRATE EQUAL 0)
  message(FATAL_ERROR "TESTER_WIFI_PRINT_UART_BAUDRATE must be a positive integer")
endif()

# The formal tester path and both standalone diagnostics use the same measured
# ESP32-C3 UART setting.  The WiFi software UART is clocked from the proven
# internal-HICK PLL image (192 MHz), giving the edge receiver ample timing
# margin at 115,200 baud.
set(TESTER_WIFI_PRINT_UART_BAUDRATE_VALUE ${TESTER_WIFI_PRINT_UART_BAUDRATE})
if(IO_APP_MODE STREQUAL "FIRST_GEN_4051_LOCAL" OR
   IO_APP_MODE STREQUAL "UNIFIED" OR
   IO_APP_MODE STREQUAL "WIFI_LINK_DIAG" OR
   IO_APP_MODE STREQUAL "WIFI_NET_DIAG")
  set(TESTER_WIFI_PRINT_UART_BAUDRATE_VALUE 115200)
endif()

# The LCDM local tester has its own PC3/PB9 WiFi print path.  Do not even
# link the PB6/PB7 legacy IR transport into that image.  The old IR bridge
# and print-terminal modes retain their own independent source set.
set(LEGACY_IR_COMM_SOURCES)
if(IO_APP_MODE STREQUAL "IR_PRINT_BRIDGE" OR
   IO_APP_MODE STREQUAL "PRINT_TERMINAL" OR
   IO_APP_MODE STREQUAL "UNIFIED")
  list(APPEND LEGACY_IR_COMM_SOURCES
    src/line_comm_bridge.c
    src/line_comm_transport.c
    src/ir_remote.c
    src/ir_learned_code.c
    "${LINE_COMM_GENERATED_C}"
  )
endif()

add_executable(${PROJECT_NAME}
  src/main.c
  src/device_config.c
  src/first_gen_4051_scan.c
  src/first_gen_display.c
  src/tester_wifi_print.c
  src/tester_wifi_link_diag.c
  src/tester_wifi_net_diag.c
  src/tester_settings.c
  src/esp_at_flash_assist.c
  src/io_board.c
  src/io_scan.c
  src/lcdm_motor_ui.c
  src/lcdm_tjc.c
  src/lcdm_tjc_verify.c
  src/print_driver.c
  src/print_job_model.c
  src/print_rs485.c
  src/print_terminal.c
  src/rpi_protocol.c
  src/rpi_rs485.c
  src/rpi_rs485_legacy.c
  src/tm1637_display.c
  src/tm1637_demo.c
  src/syscalls.c
  src/at32f45x_clock.c
  src/at32f45x_int.c
  ${LEGACY_IR_COMM_SOURCES}
  "${AT32_BSP_DIR}/project/at32f45x_board/at32f45x_board.c"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/system_at32f45x.c"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f45x.s"
  ${AT32_DRIVER_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
  inc
  "${LINE_COMM_GENERATED_DIR}"
  "${AT32_BSP_DIR}/project/at32f45x_board"
  "${AT32_BSP_DIR}/libraries/drivers/inc"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/core_support"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  AT32F455VET7
  USE_STDPERIPH_DRIVER
  AT_START_F455_V1
  IO_APP_MODE=${IO_APP_MODE_VALUE}
  UNIFIED_DEFAULT_PRODUCT_MODE=${UNIFIED_DEFAULT_PRODUCT_MODE_VALUE}
  PRINT_DRIVER_BACKEND_ZPL_RS485=${PRINT_DRIVER_BACKEND_ZPL_RS485_VALUE}
  FIRST_GEN_DISPLAY_BACKEND_LCDM=${FIRST_GEN_DISPLAY_BACKEND_LCDM_VALUE}
  FIRST_GEN_DISPLAY_AUTO_DETECT=$<IF:$<BOOL:${FIRST_GEN_DISPLAY_AUTO_DETECT}>,1,0>
  TESTER_WIFI_PRINT_UART_BAUDRATE=${TESTER_WIFI_PRINT_UART_BAUDRATE_VALUE}UL
)

set(CPU_FLAGS
  -mcpu=cortex-m4
  -mthumb
)

target_compile_options(${PROJECT_NAME} PRIVATE
  ${CPU_FLAGS}
  -Wall
  -Wextra
  -ffunction-sections
  -fdata-sections
  $<$<COMPILE_LANGUAGE:C>:-O0>
)

# PC3/PB9 is a GPIO software UART.  Its DWT-cycle timing
# and must not inherit the deliberately debug-friendly global -O0 setting.
set_source_files_properties(src/tester_wifi_print.c PROPERTIES
  COMPILE_OPTIONS "-O3"
)

target_link_options(${PROJECT_NAME} PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=${PROJECT_NAME}.map
  --specs=nano.specs
  --specs=nosys.specs
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
  BYPRODUCTS ${PROJECT_NAME}.hex ${PROJECT_NAME}.bin
)
