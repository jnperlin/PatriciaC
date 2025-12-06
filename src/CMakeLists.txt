# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

add_library(PatriciaC STATIC cpatricia_set.c cpatricia_map.c vmbumppool.c)
if(VMARENA_USE_MADVISE)
    target_compile_definitions(PatriciaC PRIVATE VMEMARENA_USE_MADVISE=1)
endif()
