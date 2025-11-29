# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

add_library(testutils STATIC helper_build_tree.c)

add_executable(test_basicapi test_basicapi.c)
target_link_libraries(test_basicapi PatriciaC testutils unity)

add_executable(test_iterator_basic test_iterator_basic.c)
target_link_libraries(test_iterator_basic PatriciaC testutils unity)

add_executable(test_iterator_modes test_iterator_modes.c)
target_link_libraries(test_iterator_modes PatriciaC testutils unity)

add_executable(test_iterator_fuzz test_iterator_fuzz.c)
target_link_libraries(test_iterator_fuzz PatriciaC testutils unity)
