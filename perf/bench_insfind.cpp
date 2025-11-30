// ===================== bench_insert_lookup.cpp =====================
#include "cpatricia.h" // Adjust include path to your project structure
#include <benchmark/benchmark.h>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <climits>

// Helper: generate random strings
static std::vector<std::string> generate_random_strings(std::size_t count, std::size_t len) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::mt19937 rng(12345);
    static std::uniform_int_distribution<int> dist(0, sizeof(alphabet) - 2);

    std::vector<std::string> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string s;
        s.resize(len);
        for (std::size_t j = 0; j < len; ++j) s[j] = alphabet[dist(rng)];
        out.push_back(std::move(s));
    }
    return out;
}

// ------------------------------------------------------------
// Benchmark: Patricia Insert
// ------------------------------------------------------------
static void BM_Patricia_Insert(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    auto keys = generate_random_strings(N, 16);

    for (auto _ : state) {
        PatriciaMapT tree;
        patricia_init(&tree);

        for (auto &s : keys) {
            patricia_insert(&tree, s.c_str(), s.length()*CHAR_BIT, 0, nullptr);
        }

        patricia_fini(&tree);
    }
}

// Run benchmark with N = 1k, 10k, 50k keys
BENCHMARK(BM_Patricia_Insert)->Arg(1000)->Arg(10000)->Arg(50000);

// ------------------------------------------------------------
// Benchmark: Patricia Lookup (after building)
// ------------------------------------------------------------
static void BM_Patricia_Lookup(benchmark::State &state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    auto keys = generate_random_strings(N, 16);

    // Build a persistent tree once
    PatriciaMapT tree;
    patricia_init(&tree);

    for (auto &s : keys) {
        patricia_insert(&tree, s.c_str(), s.length() * CHAR_BIT, 0, nullptr);
    }

    for (auto _ : state) {
        for (auto &s : keys) {
            patricia_lookup(&tree, s.c_str(), s.length() * CHAR_BIT);
        }
    }

    patricia_fini(&tree);
}

BENCHMARK(BM_Patricia_Lookup)->Arg(1000)->Arg(10000)->Arg(50000);

// ------------------------------------------------------------
// End of benchmark source
// ------------------------------------------------------------
