# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)
project(PatriciaC VERSION 0.1.0 LANGUAGES C)

# Ensure the Unity submodule is available
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Unity/CMakeLists.txt"
   AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Unity/src/unity.c")
    message(STATUS "Unity submodule missing — initializing")

    execute_process(
        COMMAND git submodule update --init --recursive Unity
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE SUBMOD_RESULT
    )

    if(NOT SUBMOD_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to initialize Unity submodule.\n"
            "Please run: git submodule update --init --recursive")
    endif()
endif()

# -----------------------------------------------------------------------------
# Global strict warnings, conservative approach
# -----------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            -Wmissing-prototypes
            -fsanitize=address,undefined
            -Werror
        )
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            -Wmissing-prototypes
            -fsanitize=address,undefined
            -Werror
        )
        add_link_options(-fsanitize=address)
    endif()
elseif(MSVC)
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 19.10)  # MSVC 2017+
        add_compile_options(
            /W4
            /WX        # warnings as errors
            /permissive- # enforce standard conformance
            /RTC1          # sanitizer-like runtime checks
        )
        add_link_options(-fsanitize=address)
    endif()
else()
    message(STATUS "Unknown compiler, strict warnings skipped")
endif()

subdirs(src)
subdirs(Unity)
subdirs(tests)

include_directories(src Unity/src)

add_custom_target(default ALL DEPENDS
    test_basicapi
    test_iterator_basic
    test_iterator_modes
    test_iterator_fuzz
)
