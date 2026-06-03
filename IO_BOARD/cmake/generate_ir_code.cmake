if(NOT DEFINED INPUT_CSV OR NOT DEFINED OUTPUT_C OR NOT DEFINED OUTPUT_H)
  message(FATAL_ERROR "INPUT_CSV, OUTPUT_C, and OUTPUT_H are required")
endif()

if(NOT EXISTS "${INPUT_CSV}")
  message(FATAL_ERROR "IR timing CSV not found: ${INPUT_CSV}")
endif()

file(STRINGS "${INPUT_CSV}" csv_lines)

set(values "")
set(count 0)
set(total_us 0)
set(line_items "")
set(array_lines "")

foreach(row IN LISTS csv_lines)
  if(row MATCHES "^index,")
    continue()
  endif()

  string(REPLACE "," ";" cols "${row}")
  list(LENGTH cols col_count)
  if(col_count LESS 4)
    continue()
  endif()

  list(GET cols 3 duration_ms)
  if(NOT duration_ms MATCHES "^([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "Bad duration in ${INPUT_CSV}: ${duration_ms}")
  endif()

  set(whole_ms "${CMAKE_MATCH_1}")
  set(frac_ms "${CMAKE_MATCH_2}")
  string(APPEND frac_ms "0000")
  string(SUBSTRING "${frac_ms}" 0 3 frac_us)
  string(SUBSTRING "${frac_ms}" 3 1 round_digit)
  string(REGEX REPLACE "^0+" "" frac_us_num "${frac_us}")
  if(frac_us_num STREQUAL "")
    set(frac_us_num 0)
  endif()

  math(EXPR duration_us "${whole_ms} * 1000 + ${frac_us_num}")
  if(round_digit GREATER_EQUAL 5)
    math(EXPR duration_us "${duration_us} + 1")
  endif()

  math(EXPR count "${count} + 1")
  math(EXPR total_us "${total_us} + ${duration_us}")

  set(item "${duration_us}U")
  if(line_items STREQUAL "")
    set(line_items "  ${item}")
  else()
    string(APPEND line_items ", ${item}")
  endif()

  string(LENGTH "${line_items}" line_len)
  if(line_len GREATER 96)
    string(APPEND array_lines "${line_items},\n")
    set(line_items "")
  endif()
endforeach()

if(NOT line_items STREQUAL "")
  string(APPEND array_lines "${line_items}\n")
endif()

file(WRITE "${OUTPUT_H}"
"#ifndef GENERATED_LINE_COMM_CODES_H\n"
"#define GENERATED_LINE_COMM_CODES_H\n\n"
"#include <stdint.h>\n\n"
"#define LINE_COMM_TESTER_RESPONSE_COUNT ${count}U\n"
"#define LINE_COMM_TESTER_RESPONSE_DURATION_US ${total_us}UL\n\n"
"extern const uint16_t g_line_comm_tester_response_code_us[LINE_COMM_TESTER_RESPONSE_COUNT];\n\n"
"#endif\n")

file(WRITE "${OUTPUT_C}"
"#include \"generated_line_comm_codes.h\"\n\n"
"const uint16_t g_line_comm_tester_response_code_us[LINE_COMM_TESTER_RESPONSE_COUNT] = {\n"
"${array_lines}"
"};\n")
