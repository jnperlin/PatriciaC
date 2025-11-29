# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

add_library(testutils STATIC helper_build_tree.c)

foreach(t IN ITEMS test_basicapi test_iterator_basic test_iterator_modes test_iterator_fuzz)
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} PRIVATE PatriciaC testutils unity)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
