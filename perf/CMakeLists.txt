# -------------------------------------------------------------------------------------
# This file is part of "PatriciaC" by J.Perlinger.
#
# PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
#    visit https://creativecommons.org/publicdomain/zero/1.0/
#
# -------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

add_executable(patriciac_bench bench_main.cpp bench_insfind.cpp)
target_link_libraries(patriciac_bench PRIVATE PatriciaC benchmark::benchmark)
target_compile_features(patriciac_bench PRIVATE cxx_std_17)

# -*- that's all folks -*-
