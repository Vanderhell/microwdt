cmake_minimum_required(VERSION 3.20)
project(microwdt_external_consumer LANGUAGES C)

find_package(microwdt CONFIG REQUIRED)

add_executable(microwdt_external_consumer main.c)
target_link_libraries(microwdt_external_consumer PRIVATE microwdt::microwdt)
