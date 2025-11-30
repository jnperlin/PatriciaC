# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

# enable sanitizer if possible
# -------------------------------------------------------------------------------------
set(TEST_EXTRA_CFLAGS "")
set(TEST_EXTRA_LFLAGS "")

# enable sanitizer build if appropriate
# -------------------------------------------------------------------------------------
if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        set(TEST_EXTRA_CFLAGS -fsanitize=address,undefined)
        set(TEST_EXTRA_LFLAGS -fsanitize=address,undefined)
    endif()
endif()

# compile the object-under-test and the helpers into a small lib
# -------------------------------------------------------------------------------------
add_library(testutils STATIC helper_build_tree.c ${CMAKE_SOURCE_DIR}/src/cpatricia.c)
target_compile_options(testutils PRIVATE ${TEST_EXTRA_CFLAGS})

# now create the test prgrams according to "schema F"
# -------------------------------------------------------------------------------------
foreach(t IN ITEMS test_basicapi test_iterator_basic test_iterator_modes test_iterator_fuzz)
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} PRIVATE testutils unity)
    target_compile_options(${t} PRIVATE ${TEST_EXTRA_CFLAGS})
    target_link_options(${t} PRIVATE ${TEST_EXTRA_LFLAGS})
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# -*- that's all folks -*-
