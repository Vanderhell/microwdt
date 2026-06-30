cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE)
  message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED PATTERN)
  message(FATAL_ERROR "PATTERN is required")
endif()

set(include_dir "${CMAKE_CURRENT_LIST_DIR}/../include")
set(source_path "${CMAKE_CURRENT_LIST_DIR}/fixtures/${SOURCE}")
set(object_path "${CMAKE_CURRENT_BINARY_DIR}/${SOURCE}.obj")

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(cmd "${CMAKE_C_COMPILER}" /nologo /W4 /WX /std:c11 "/I${include_dir}" /c "${source_path}" "/Fo${object_path}")
else()
  set(cmd "${CMAKE_C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wwrite-strings -Wformat=2 -Werror "-I${include_dir}" -c "${source_path}" -o "${object_path}")
endif()

execute_process(
  COMMAND ${cmd}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

set(combined "${out}\n${err}")

if(rc EQUAL 0)
  message(FATAL_ERROR "expected compile failure for ${SOURCE}")
endif()

string(FIND "${combined}" "${SOURCE}" source_pos)
if(source_pos EQUAL -1)
  message(FATAL_ERROR "diagnostic did not mention ${SOURCE}\n${combined}")
endif()

string(FIND "${combined}" "${PATTERN}" pattern_pos)
if(pattern_pos EQUAL -1)
  message(FATAL_ERROR "diagnostic did not mention ${PATTERN}\n${combined}")
endif()

message(STATUS "compile-fail matched ${SOURCE}: ${PATTERN}")
