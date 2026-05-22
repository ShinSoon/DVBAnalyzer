cmake_minimum_required(VERSION 3.14)

project(DVBAnalyzer
    VERSION 1.0.0
    DESCRIPTION "DVB SI parser: text format and binary MPEG-2 Transport Stream"
    LANGUAGES CXX)

# --- Language standard (portable C++17, no compiler extensions) ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Debug build for single-config generators if unspecified.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Warnings ---
if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

# --- Core library: every parser source except the application entry point ---
# Both the app and the test binary link this, so production code is compiled once.
add_library(dvbsi STATIC
    dvb_crc32.cpp
    dvb_descriptors.cpp
    dvb_eit.cpp
    dvb_sdt.cpp
    dvb_section_parser.cpp
    dvb_si_parser.cpp
    ts_demux.cpp
    ts_generator.cpp
)
target_include_directories(dvbsi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Application ---
add_executable(DVBAnalyzer main.cpp)
target_link_libraries(DVBAnalyzer PRIVATE dvbsi)

# --- Tests (GoogleTest, vendored under third_party/googletest) ---
option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt")
    enable_testing()

    # On Windows, link GoogleTest against the same dynamic CRT as everything else.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

    add_executable(DVBAnalyzerTests
        tests/test_crc32.cpp
        tests/test_dvb_time.cpp
        tests/test_descriptors.cpp
        tests/test_section_parser.cpp
        tests/test_end_to_end.cpp
    )
    target_link_libraries(DVBAnalyzerTests PRIVATE dvbsi GTest::gtest GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(DVBAnalyzerTests)
endif()
