cmake_minimum_required(VERSION 3.16)
project(mayhem-cmake-example)

add_executable(ad_parser ad_parser.c)

## Uncomment these lines after converting ad_parser.c into a libFuzzer target
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Clang is required for libFuzzer!")
endif()
target_compile_options(ad_parser PUBLIC -fsanitize=fuzzer)
target_link_options(ad_parser PUBLIC -fsanitize=fuzzer)

