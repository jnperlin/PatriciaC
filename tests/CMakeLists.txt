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
set(TEST_EXTRA_LIBS "")

# enable sanitizer build if appropriate
# -------------------------------------------------------------------------------------
if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        set(TEST_EXTRA_CFLAGS -fsanitize=address,undefined)
        set(TEST_EXTRA_LFLAGS -fsanitize=address,undefined)
        set(TEST_EXTRA_LIBS atomic)
    endif()
endif()

# compile the object-under-test and the helpers into a small lib
# -------------------------------------------------------------------------------------
add_library(testutils STATIC
    helper_build_tree.c
    ${CMAKE_SOURCE_DIR}/src/cpatricia_set.c
    ${CMAKE_SOURCE_DIR}/src/cpatricia_map.c
    ${CMAKE_SOURCE_DIR}/src/vmbumppool.c
)
target_compile_options(testutils PRIVATE ${TEST_EXTRA_CFLAGS})
target_compile_definitions(testutils PRIVATE PATRICIA_TEST_LINKCNT )
if(PATRIMAP_USE_ARENA)
    target_compile_definitions(testutils PRIVATE PATRIMAP_USE_ARENA=1)
endif()
if(VMARENA_USE_MADVISE)
    target_compile_definitions(testutils PRIVATE VMEMARENA_USE_MADVISE=1)
endif()

# now create the test prgrams according to "schema F"
# -------------------------------------------------------------------------------------
foreach(t IN ITEMS test_bitops test_basicapi test_iterator_basic
                   test_iterator_modes test_iterator_fuzz)
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} PRIVATE testutils unity ${TEST_EXTRA_LIBS})
    target_compile_options(${t} PRIVATE ${TEST_EXTRA_CFLAGS})
    target_compile_definitions(${t} PRIVATE PATRICIA_TEST_LINKCNT)
    target_link_options(${t} PRIVATE ${TEST_EXTRA_LFLAGS})
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# -*- that's all folks -*-
