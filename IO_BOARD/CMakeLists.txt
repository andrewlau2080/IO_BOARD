cmake_minimum_required(VERSION 3.20)

project(io_board_at32f455 C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(AT32_BSP_DIR "${CMAKE_CURRENT_LIST_DIR}/../.vendor/AT32F45x_Firmware_Library_V2.0.1" CACHE PATH "AT32F45x firmware library path")
set(LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/AT32F455xE_FLASH.ld")

if(NOT EXISTS "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/at32f45x.h")
  message(FATAL_ERROR "AT32 BSP not found at ${AT32_BSP_DIR}")
endif()

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
  "${AT32_BSP_DIR}/project/at32f45x_board/at32f45x_board.c"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/system_at32f45x.c"
  "${AT32_BSP_DIR}/libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f45x.s"
  ${AT32_DRIVER_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
  inc
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
