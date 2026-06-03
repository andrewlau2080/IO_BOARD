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
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_flash.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_gpio.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_misc.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_pwc.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_spi.c"
  "${AT32_BSP_DIR}/libraries/drivers/src/at32f45x_usart.c"
)

add_executable(${PROJECT_NAME}
  src/main.c
  src/io_board.c
  src/io_scan.c
  src/line_comm_bridge.c
  src/print_job_model.c
  src/rpi_protocol.c
  src/tm1637_display.c
  src/ir_remote.c
  src/ir_learned_code.c
  src/syscalls.c
  src/at32f45x_clock.c
  src/at32f45x_int.c
  "${LINE_COMM_GENERATED_C}"
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
