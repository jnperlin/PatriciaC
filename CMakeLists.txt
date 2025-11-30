# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)
project(PatriciaC VERSION 0.1.0 LANGUAGES C CXX)

find_package(Git QUIET)

# ThrowTheSwitch Unity integration for PatriciaC
# ===============================================================================
set(PATRICIAC_UNITY_SUBMODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Unity")
set(PATRICIAC_HAVE_UNITY OFF)

# Try to auto-initialize the submodule
# -----------------------------------------------------------------------------
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init Unity
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _submodule_result
        OUTPUT_QUIET ERROR_QUIET
    )
endif()

# Check whether Unity sources are present
# -----------------------------------------------------------------------------
if (EXISTS "${PATRICIAC_UNITY_SUBMODULE_PATH}/src/unity.c")
    set(PATRICIAC_HAVE_UNITY ON)
endif()
message(STATUS "PatriciaC: Unity submodule available: ${PATRICIAC_HAVE_UNITY}")


# Google Benchmark integration for PatriciaC — WITHOUT GoogleTest
# ===============================================================================
# This version:
#   - Does NOT build or require GoogleTest
#   - Forces GBENCHMARK_ENABLE_TESTING=OFF and BENCHMARK_ENABLE_INSTALL=OFF
#   - Attempts submodule init just like Unity integration
#   - Adds benchmark targets only if the submodule is present
#
# Submodule expected at:
#     external/benchmark
set(PATRICIAC_BENCHMARK_SUBMODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/GoogleBench")
set(PATRICIAC_HAVE_BENCHMARK OFF)

# Try to auto-initialize the submodules (Unity-style behavior)
# -----------------------------------------------------------------------------
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init GoogleBench
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _submodule_result
        OUTPUT_QUIET ERROR_QUIET
    )
endif()

# Check whether benchmark sources are present
# -----------------------------------------------------------------------------
if(EXISTS "${PATRICIAC_BENCHMARK_SUBMODULE_PATH}/src/benchmark.cc")
    set(PATRICIAC_HAVE_BENCHMARK ON)
endif()
message(STATUS "PatriciaC: Benchmark submodule available: ${PATRICIAC_HAVE_BENCHMARK}")

# Global strict warnings, conservative approach
# -----------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
            -Werror
        )
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
            -Wno-c2y-extensions
            -Werror
        )
    endif()
elseif(MSVC)
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 19.10)  # MSVC 2017+
        add_compile_options(
            /W4
            /WX        # warnings as errors
            /permissive- # enforce standard conformance
            /RTC1          # sanitizer-like runtime checks
        )
    endif()
else()
    message(STATUS "Unknown compiler, strict warnings skipped")
endif()

# posibly integrate Google Benchmark with GoogleTest explicitly disabled
# -----------------------------------------------------------------------------
if(PATRICIAC_HAVE_BENCHMARK)
    # Disable bundled tests (which require gtest)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    include_directories(
        "${PATRICIAC_BENCHMARK_SUBMODULE_PATH}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

    add_subdirectory(GoogleBench)
    add_subdirectory(perf)
endif()

# possibly integrate UNITY
# -----------------------------------------------------------------------------
if(PATRICIAC_HAVE_UNITY)
    enable_testing()
    include_directories(src Unity/src)
    subdirs(Unity)
    subdirs(tests)

    add_custom_target(default ALL DEPENDS
        test_basicapi
        test_iterator_basic
        test_iterator_modes
        test_iterator_fuzz
    )
endif()

# compile our only source file...
# -----------------------------------------------------------------------------
subdirs(src)

# -*- that's all folks -*-
