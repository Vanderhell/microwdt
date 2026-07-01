cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE)
  message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED SOURCE_LINE)
  message(FATAL_ERROR "SOURCE_LINE is required")
endif()
if(NOT DEFINED MSVC_PATTERNS)
  message(FATAL_ERROR "MSVC_PATTERNS is required")
endif()
if(NOT DEFINED OTHER_PATTERNS)
  message(FATAL_ERROR "OTHER_PATTERNS is required")
endif()

set(include_dir "${CMAKE_CURRENT_LIST_DIR}/../include")
set(source_path "${CMAKE_CURRENT_LIST_DIR}/fixtures/${SOURCE}")
set(object_path "${CMAKE_CURRENT_BINARY_DIR}/${SOURCE}.obj")

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(msvc_include_args "/I${include_dir}")
  foreach(dir IN LISTS CMAKE_C_STANDARD_INCLUDE_DIRECTORIES CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
    if(NOT dir STREQUAL "")
      list(APPEND msvc_include_args "/I${dir}")
    endif()
  endforeach()
  if(DEFINED MSVC_INCLUDE_ENV AND NOT MSVC_INCLUDE_ENV STREQUAL "")
    set(cmd ${CMAKE_COMMAND} -E env "INCLUDE=${MSVC_INCLUDE_ENV}" "${CMAKE_C_COMPILER}" /nologo /W4 /WX /std:c11 ${msvc_include_args} /c "${source_path}" "/Fo${object_path}")
  else()
    set(cmd "${CMAKE_C_COMPILER}" /nologo /W4 /WX /std:c11 ${msvc_include_args} /c "${source_path}" "/Fo${object_path}")
  endif()
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

set(source_line_found FALSE)
foreach(line_token "${SOURCE}:${SOURCE_LINE}" "${SOURCE}(${SOURCE_LINE})")
  string(FIND "${combined}" "${line_token}" line_pos)
  if(NOT line_pos EQUAL -1)
    set(source_line_found TRUE)
    break()
  endif()
endforeach()

if(NOT source_line_found)
  message(FATAL_ERROR "diagnostic did not mention ${SOURCE} line ${SOURCE_LINE}\n${combined}")
endif()

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(expected_patterns "${MSVC_PATTERNS}")
else()
  set(expected_patterns "${OTHER_PATTERNS}")
endif()

string(REPLACE "|" ";" expected_pattern_list "${expected_patterns}")
set(pattern_found FALSE)
foreach(expected_pattern IN LISTS expected_pattern_list)
  string(FIND "${combined}" "${expected_pattern}" pattern_pos)
  if(NOT pattern_pos EQUAL -1)
    set(pattern_found TRUE)
    set(matched_pattern "${expected_pattern}")
    break()
  endif()
endforeach()

if(NOT pattern_found)
  message(FATAL_ERROR "diagnostic did not mention any expected pattern: ${expected_patterns}\n${combined}")
endif()

message(STATUS "compile-fail matched ${SOURCE}:${SOURCE_LINE}: ${matched_pattern}")
