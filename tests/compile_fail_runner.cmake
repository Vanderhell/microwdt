cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE)
  message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED SOURCE_LINE)
  message(FATAL_ERROR "SOURCE_LINE is required")
endif()
if(NOT DEFINED DIAGNOSTIC_CLASS)
  message(FATAL_ERROR "DIAGNOSTIC_CLASS is required")
endif()

set(include_dir "${CMAKE_CURRENT_LIST_DIR}/../include")
set(source_path "${CMAKE_CURRENT_LIST_DIR}/fixtures/${SOURCE}")
set(project_dir "${CMAKE_CURRENT_BINARY_DIR}/${SOURCE}.cmake")
set(build_dir "${project_dir}/build")
set(project_file "${project_dir}/CMakeLists.txt")

set(is_msvc FALSE)
if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(is_msvc TRUE)
endif()

file(MAKE_DIRECTORY "${project_dir}")

if(is_msvc)
  set(target_compile_options_block "target_compile_options(compile_fail_target PRIVATE /W4 /WX /permissive- /Zc:preprocessor)\n")
else()
  set(target_compile_options_block "target_compile_options(compile_fail_target PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wwrite-strings -Wformat=2 -Werror)\n")
endif()

string(REPLACE "\\" "/" source_path_cmake "${source_path}")
string(REPLACE "\\" "/" include_dir_cmake "${include_dir}")

file(WRITE "${project_file}" "cmake_minimum_required(VERSION 3.20)\n")
file(APPEND "${project_file}" "project(compile_fail_target LANGUAGES C)\n")
file(APPEND "${project_file}" "set(CMAKE_C_EXTENSIONS OFF)\n")
file(APPEND "${project_file}" "set(CMAKE_C_STANDARD_REQUIRED ON)\n")
file(APPEND "${project_file}" "set(CMAKE_C_STANDARD 99)\n")
file(APPEND "${project_file}" "add_executable(compile_fail_target \"${source_path_cmake}\")\n")
file(APPEND "${project_file}" "target_include_directories(compile_fail_target PRIVATE \"${include_dir_cmake}\")\n")
file(APPEND "${project_file}" "${target_compile_options_block}")

set(configure_cmd "${CMAKE_COMMAND}" -S "${project_dir}" -B "${build_dir}")
if(DEFINED CMAKE_GENERATOR AND NOT CMAKE_GENERATOR STREQUAL "")
  list(APPEND configure_cmd -G "${CMAKE_GENERATOR}")
endif()
if(DEFINED CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "")
  list(APPEND configure_cmd -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(DEFINED CMAKE_GENERATOR_TOOLSET AND NOT CMAKE_GENERATOR_TOOLSET STREQUAL "")
  list(APPEND configure_cmd -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
if(DEFINED CMAKE_GENERATOR_INSTANCE AND NOT CMAKE_GENERATOR_INSTANCE STREQUAL "")
  list(APPEND configure_cmd "-DCMAKE_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}")
endif()
if(NOT is_msvc)
  list(APPEND configure_cmd "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
endif()

execute_process(
  COMMAND ${configure_cmd}
  RESULT_VARIABLE configure_rc
  OUTPUT_VARIABLE configure_out
  ERROR_VARIABLE configure_err)

set(combined "${configure_out}\n${configure_err}")

if(NOT configure_rc EQUAL 0)
  message(FATAL_ERROR "failed to configure compile-fail project for ${SOURCE}\n${combined}")
endif()

set(build_cmd "${CMAKE_COMMAND}" --build "${build_dir}" --target compile_fail_target)
if(DEFINED COMPILE_FAIL_CONFIG AND NOT COMPILE_FAIL_CONFIG STREQUAL "")
  list(APPEND build_cmd --config "${COMPILE_FAIL_CONFIG}")
endif()

execute_process(
  COMMAND ${build_cmd}
  RESULT_VARIABLE build_rc
  OUTPUT_VARIABLE build_out
  ERROR_VARIABLE build_err)

set(combined "${build_out}\n${build_err}")

if(build_rc EQUAL 0)
  message(FATAL_ERROR "expected compile failure for ${SOURCE}")
endif()

string(FIND "${combined}" "${SOURCE}" source_pos)
if(source_pos EQUAL -1)
  message(FATAL_ERROR "diagnostic did not mention ${SOURCE}\n${combined}")
endif()

string(REPLACE "|" ";" source_line_list "${SOURCE_LINE}")
set(source_line_found FALSE)
foreach(expected_line IN LISTS source_line_list)
  foreach(line_token "${SOURCE}:${expected_line}" "${SOURCE}(${expected_line})" "${SOURCE}(${expected_line},")
    string(FIND "${combined}" "${line_token}" line_pos)
    if(NOT line_pos EQUAL -1)
      set(source_line_found TRUE)
      set(matched_line "${expected_line}")
      break()
    endif()
  endforeach()
  if(source_line_found)
    break()
  endif()
endforeach()

if(NOT source_line_found)
  message(FATAL_ERROR "diagnostic did not mention ${SOURCE} line ${SOURCE_LINE}\n${combined}")
endif()

if(DIAGNOSTIC_CLASS STREQUAL "CALLBACK_MISMATCH")
  if(is_msvc)
    set(expected_patterns "C4113|C4028|C4047|C4133")
  else()
    set(expected_patterns "incompatible-pointer-types|incompatible function pointer")
  endif()
elseif(DIAGNOSTIC_CLASS STREQUAL "CONST_WRITE")
  if(is_msvc)
    set(expected_patterns "const object|read-only|C3892|C2166")
  else()
    set(expected_patterns "const object|const-qualified type|read-only object")
  endif()
elseif(DIAGNOSTIC_CLASS STREQUAL "OLD_LAYOUT")
  if(is_msvc)
    set(expected_patterns "subscript|initializer|designator|C2059|C2078|C2109|C2143")
  else()
    set(expected_patterns "subscript|array index in non-array initializer|non-array initializer|non-array type")
  endif()
else()
  message(FATAL_ERROR "unknown DIAGNOSTIC_CLASS: ${DIAGNOSTIC_CLASS}")
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
  message(FATAL_ERROR "diagnostic did not mention any expected ${DIAGNOSTIC_CLASS} pattern: ${expected_patterns}\n${combined}")
endif()

message(STATUS "compile-fail matched ${SOURCE}:${matched_line}: ${DIAGNOSTIC_CLASS} -> ${matched_pattern}")
