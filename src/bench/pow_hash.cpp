// Copyright (c) 2025 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <hash_phr.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
//! Bytes to hash per iteration
static constexpr size_t BUFFER_SIZE{1000*1000};

inline void Pow_PHR(benchmark::Bench& bench, const size_t bytes)
{
    uint256 hash{};
    std::vector<uint8_t> in(bytes, 0);
    bench.minEpochIterations(5).batch(in.size()).unit("byte").run([&] {
        hash = HashPHR(in.begin(), in.end());
    });
}
} // namespace

static void Pow_PHR_0032b(benchmark::Bench& bench) { return Pow_PHR(bench, 32); }
static void Pow_PHR_0080b(benchmark::Bench& bench) { return Pow_PHR(bench, 80); }
static void Pow_PHR_0128b(benchmark::Bench& bench) { return Pow_PHR(bench, 128); }
static void Pow_PHR_0512b(benchmark::Bench& bench) { return Pow_PHR(bench, 512); }
static void Pow_PHR_1024b(benchmark::Bench& bench) { return Pow_PHR(bench, 1024); }
static void Pow_PHR_2048b(benchmark::Bench& bench) { return Pow_PHR(bench, 2048); }
static void Pow_PHR_1M(benchmark::Bench& bench) { return Pow_PHR(bench, BUFFER_SIZE); }

BENCHMARK(Pow_PHR_0032b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_0080b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_0128b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_0512b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_1024b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_2048b, benchmark::PriorityLevel::HIGH);
BENCHMARK(Pow_PHR_1M, benchmark::PriorityLevel::HIGH);
