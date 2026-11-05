cmake_minimum_required(VERSION 3.20)
project(RiftPulse VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(riftpulse_lib
    src/csv_reader.cpp
    src/analyzer.cpp
    src/json_reporter.cpp
    src/telemetry.cpp
)
target_include_directories(riftpulse_lib PUBLIC include)
if(MSVC)
    target_compile_options(riftpulse_lib PRIVATE /W4 /permissive-)
else()
    target_compile_options(riftpulse_lib PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(riftpulse src/main.cpp)
target_link_libraries(riftpulse PRIVATE riftpulse_lib)

include(CTest)
if(BUILD_TESTING)
    add_executable(riftpulse_tests tests/test_main.cpp)
    target_link_libraries(riftpulse_tests PRIVATE riftpulse_lib)
    add_test(NAME riftpulse_tests COMMAND riftpulse_tests)

    add_executable(riftpulse_telemetry_tests tests/test_telemetry.cpp)
    target_link_libraries(riftpulse_telemetry_tests PRIVATE riftpulse_lib)
    add_test(NAME riftpulse_telemetry_tests COMMAND riftpulse_telemetry_tests)
endif()
