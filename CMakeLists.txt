cmake_minimum_required(VERSION 3.20)

project(microwdt LANGUAGES C CXX)

include(CTest)
include(CMakePackageConfigHelpers)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

add_library(microwdt src/mwdt.c)
add_library(microwdt::microwdt ALIAS microwdt)

target_include_directories(
  microwdt
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(microwdt PUBLIC c_std_99)

if(MSVC)
  target_compile_options(microwdt PRIVATE /W4 /WX /permissive- /Zc:preprocessor)
else()
  target_compile_options(
    microwdt
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wstrict-prototypes
      -Wmissing-prototypes
      -Wcast-qual
      -Wwrite-strings
      -Wformat=2
      -Werror)
endif()

add_executable(mwdt_runtime_c99 tests/test_all.c)
target_link_libraries(mwdt_runtime_c99 PRIVATE microwdt)
set_property(TARGET mwdt_runtime_c99 PROPERTY C_STANDARD 99)

add_library(microwdt_c11 STATIC src/mwdt.c)
target_include_directories(microwdt_c11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(microwdt_c11 PRIVATE /W4 /WX /permissive- /Zc:preprocessor)
else()
  target_compile_options(
    microwdt_c11
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wstrict-prototypes
      -Wmissing-prototypes
      -Wcast-qual
      -Wwrite-strings
      -Wformat=2
      -Werror)
endif()
set_property(TARGET microwdt_c11 PROPERTY C_STANDARD 11)

add_executable(mwdt_runtime_c11 tests/test_all.c)
target_link_libraries(mwdt_runtime_c11 PRIVATE microwdt_c11)
set_property(TARGET mwdt_runtime_c11 PROPERTY C_STANDARD 11)
target_include_directories(mwdt_runtime_c11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(mwdt_harness_fail tests/test_harness_fail.c)
add_test(NAME runtime_c99 COMMAND mwdt_runtime_c99)
add_test(NAME runtime_c11 COMMAND mwdt_runtime_c11)
add_test(NAME harness_failure_path COMMAND mwdt_harness_fail)
set_tests_properties(harness_failure_path PROPERTIES WILL_FAIL TRUE)

add_executable(include_twice tests/fixtures/include_twice.c)
target_link_libraries(include_twice PRIVATE microwdt)

add_executable(mtu_c tests/fixtures/mtu_a.c tests/fixtures/mtu_b.c tests/fixtures/mtu_main.c)
target_link_libraries(mtu_c PRIVATE microwdt)

add_executable(multi_call_line tests/fixtures/multi_call_line.c)
target_link_libraries(multi_call_line PRIVATE microwdt)

add_executable(readme_example tests/fixtures/readme_example.c)
target_link_libraries(readme_example PRIVATE microwdt)

add_executable(cpp_consumer tests/fixtures/cpp_consumer.cpp)
target_link_libraries(cpp_consumer PRIVATE microwdt)

add_test(NAME include_twice COMMAND include_twice)
add_test(NAME mtu_c COMMAND mtu_c)
add_test(NAME multi_call_line COMMAND multi_call_line)
add_test(NAME readme_example COMMAND readme_example)
add_test(NAME cpp_consumer COMMAND cpp_consumer)

function(add_compile_fail_test test_name source source_line diagnostic_class)
  add_test(
    NAME ${test_name}
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE=${source}"
      "-DSOURCE_LINE=${source_line}"
      "-DDIAGNOSTIC_CLASS=${diagnostic_class}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_C_COMPILER_ID=${CMAKE_C_COMPILER_ID}"
      "-DCMAKE_C_COMPILER_FRONTEND_VARIANT=${CMAKE_C_COMPILER_FRONTEND_VARIANT}"
      "-DCMAKE_C_SIMULATE_ID=${CMAKE_C_SIMULATE_ID}"
      "-DCMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES=${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}"
      "-DCMAKE_C_STANDARD_INCLUDE_DIRECTORIES=${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES}"
      "-DMSVC_INCLUDE_ENV=$ENV{INCLUDE}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail_runner.cmake)
endfunction()

add_compile_fail_test(compile_fail_bad_clock compile_fail_bad_clock.c "16|3" CALLBACK_MISMATCH)
add_compile_fail_test(compile_fail_bad_timeout compile_fail_bad_timeout.c "11|3" CALLBACK_MISMATCH)
add_compile_fail_test(compile_fail_bad_reset compile_fail_bad_reset.c "10|3" CALLBACK_MISMATCH)
add_compile_fail_test(compile_fail_const_event compile_fail_const_event.c "6|3" CONST_WRITE)
add_compile_fail_test(compile_fail_old_layout compile_fail_old_layout.c "6|5" OLD_LAYOUT)
add_compile_fail_test(compile_fail_bad_assignment compile_fail_bad_assignment.c "11|3" CALLBACK_MISMATCH)

install(TARGETS microwdt EXPORT microwdtTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(FILES include/mwdt.h DESTINATION include)

install(EXPORT microwdtTargets
  NAMESPACE microwdt::
  FILE microwdtTargets.cmake
  DESTINATION lib/cmake/microwdt)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/microwdtConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/microwdtConfig.cmake
  INSTALL_DESTINATION lib/cmake/microwdt
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/microwdtConfig.cmake DESTINATION lib/cmake/microwdt)
