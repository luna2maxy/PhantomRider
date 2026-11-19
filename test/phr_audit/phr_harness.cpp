// PHR-256-YP audit harness. Compiles the live src/hash_phr.h implementation
// without modifying it. Build: ./test/phr_audit/build.sh
#include <hash_phr.h>

#include <arith_uint256.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

// ---------------------------------------------------------------- allocation tracking
namespace {
std::atomic<std::uint64_t> g_alloc_count{0};
std::atomic<std::uint64_t> g_alloc_bytes{0};
}

void* operator new(std::size_t n) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_alloc_bytes.fetch_add(n, std::memory_order_relaxed);
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_alloc_bytes.fetch_add(n, std::memory_order_relaxed);
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_alloc_bytes.fetch_add(n, std::memory_order_relaxed);
    void* p = std::aligned_alloc(static_cast<std::size_t>(a), (n + static_cast<std::size_t>(a) - 1) / static_cast<std::size_t>(a) * static_cast<std::size_t>(a));
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }

// ---------------------------------------------------------------- helpers
namespace {

using Clock = std::chrono::steady_clock;

std::string Hex(const std::uint8_t* p, std::size_t n) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(d[p[i] >> 4]);
        out.push_back(d[p[i] & 0xf]);
    }
    return out;
}

std::string Hex(const phr_pow::Hash& h) { return Hex(h.data(), h.size()); }

std::vector<std::uint8_t> FromHex(const std::string& s) {
    std::vector<std::uint8_t> out;
    out.reserve(s.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i + 1 < s.size(); i += 2) {
        int hi = nibble(s[i]);
        int lo = nibble(s[i + 1]);
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string HeaderHex(const std::vector<std::uint8_t>& h) { return Hex(h.data(), h.size()); }

void PutU32LE(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

struct SplitMix64 {
    std::uint64_t s;
    explicit SplitMix64(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        s += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
};

std::vector<std::uint8_t> RandomHeader(SplitMix64& rng) {
    std::vector<std::uint8_t> h(80);
    for (int i = 0; i < 10; ++i) {
        std::uint64_t v = rng.next();
        for (int j = 0; j < 8; ++j) h[i * 8 + j] = static_cast<std::uint8_t>(v >> (8 * j));
    }
    return h;
}

std::string ProcField(const char* key) {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind(key, 0) == 0) {
            std::istringstream iss(line);
            std::string k, v;
            iss >> k >> v;
            return v;
        }
    }
    return "n/a";
}

double CpuSeconds() {
    std::ifstream f("/proc/self/stat");
    std::string line;
    std::getline(f, line);
    std::istringstream iss(line);
    // pid comm state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime
    for (int i = 0; i < 14; ++i) {
        std::string tok;
        iss >> tok;
        if (i == 13) return std::stod(tok) / sysconf(_SC_CLK_TCK);
    }
    return 0.0;
}

long PageSize() { return sysconf(_SC_PAGESIZE); }

// ---------------------------------------------------------------- modes
int Vectors(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "cannot open " << path << "\n";
        return 2;
    }
    std::cout << "case\tinput_bytes\tdataset_bytes\trounds\texpected\tactual\tstatus\n";
    int pass = 0, fail = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> col;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '\t')) col.push_back(item);
        if (col.size() < 5) continue;
        auto header = FromHex(col[1]);
        std::size_t dataset = std::stoull(col[2]);
        std::uint32_t rounds = static_cast<std::uint32_t>(std::stoul(col[3]));
        auto digest = phr_pow::HashHeaderPHR(header.data(), header.size(), "PHR", dataset, rounds);
        std::string actual = Hex(digest);
        bool ok = actual == col[4];
        ok ? ++pass : ++fail;
        std::cout << col[0] << '\t' << header.size() << '\t' << dataset << '\t' << rounds
                  << '\t' << col[4] << '\t' << actual << '\t' << (ok ? "PASS" : "FAIL") << '\n';
    }
    std::cerr << "vectors: " << pass << " PASS, " << fail << " FAIL\n";
    return fail == 0 ? 0 : 1;
}

int Primitives() {
    const char* s = "abc";
    auto sha = phr_pow::Sha256(reinterpret_cast<const std::uint8_t*>(s), 3);
    auto sha3 = phr_pow::Sha3_256(reinterpret_cast<const std::uint8_t*>(s), 3);
    auto b2 = phr_pow::Blake2s(reinterpret_cast<const std::uint8_t*>(s), 3);
    std::cout << "sha256_abc\t" << Hex(sha) << "\n";
    std::cout << "sha3_256_abc\t" << Hex(sha3) << "\n";
    std::cout << "blake2s_abc\t" << Hex(b2) << "\n";
    return 0;
}

int Distribution(std::uint64_t n, std::uint64_t seed) {
    std::array<std::uint64_t, 256> bit_ones{};
    std::array<std::uint64_t, 256> byte_freq{};
    std::array<std::uint64_t, 256> pos_bit_ones{};
    std::vector<std::uint64_t> values;
    values.reserve(static_cast<std::size_t>(n));
    SplitMix64 rng(seed);
    for (std::uint64_t i = 0; i < n; ++i) {
        auto h = RandomHeader(rng);
        auto d = phr_pow::HashHeaderPHR(h.data(), h.size());
        std::uint64_t v = 0;
        for (int j = 0; j < 32; ++j) {
            std::uint8_t b = d[static_cast<std::size_t>(j)];
            byte_freq[static_cast<std::size_t>(b)]++;
            for (int k = 0; k < 8; ++k) {
                int bit = (b >> k) & 1;
                std::size_t idx = static_cast<std::size_t>(j) * 8 + static_cast<std::size_t>(k);
                bit_ones[idx] += static_cast<std::uint64_t>(bit);
                pos_bit_ones[idx] += static_cast<std::uint64_t>(bit);
            }
        }
        for (int j = 0; j < 8; ++j) v |= static_cast<std::uint64_t>(d[static_cast<std::size_t>(j)]) << (8 * j);
        values.push_back(v);
    }
    std::cout << "{\"n\":" << n << ",\"seed\":" << seed << ",\"bit_ones\":[";
    for (std::size_t i = 0; i < 256; ++i) std::cout << (i ? "," : "") << bit_ones[i];
    std::cout << "],\"byte_freq\":[";
    for (std::size_t i = 0; i < 256; ++i) std::cout << (i ? "," : "") << byte_freq[i];
    std::cout << "],\"values\":[";
    for (std::size_t i = 0; i < values.size(); ++i) std::cout << (i ? "," : "") << values[i];
    std::cout << "]}\n";
    return 0;
}

int Avalanche(std::uint64_t n, std::uint64_t seed) {
    SplitMix64 rng(seed);
    std::array<std::uint64_t, 256> changed_per_bit{};
    std::vector<std::uint64_t> counts;
    counts.reserve(static_cast<std::size_t>(n));
    for (std::uint64_t i = 0; i < n; ++i) {
        auto a = RandomHeader(rng);
        auto b = a;
        std::size_t pos = static_cast<std::size_t>(rng.next() % (80 * 8));
        b[pos / 8] ^= static_cast<std::uint8_t>(1u << (pos % 8));
        auto da = phr_pow::HashHeaderPHR(a.data(), a.size());
        auto db = phr_pow::HashHeaderPHR(b.data(), b.size());
        std::uint64_t changed = 0;
        for (int j = 0; j < 32; ++j) {
            std::uint8_t x = da[static_cast<std::size_t>(j)] ^ db[static_cast<std::size_t>(j)];
            for (int k = 0; k < 8; ++k) {
                int bit = (x >> k) & 1;
                changed_per_bit[static_cast<std::size_t>(j) * 8 + static_cast<std::size_t>(k)] += static_cast<std::uint64_t>(bit);
                changed += static_cast<std::uint64_t>(bit);
            }
        }
        counts.push_back(changed);
    }
    std::cout << "{\"n\":" << n << ",\"seed\":" << seed << ",\"counts\":[";
    for (std::size_t i = 0; i < counts.size(); ++i) std::cout << (i ? "," : "") << counts[i];
    std::cout << "],\"changed_per_bit\":[";
    for (std::size_t i = 0; i < 256; ++i) std::cout << (i ? "," : "") << changed_per_bit[i];
    std::cout << "]}\n";
    return 0;
}

int Bench(std::uint64_t n, int threads) {
    std::uint8_t base[80] = {};
    std::vector<std::uint64_t> per_thread(static_cast<std::size_t>(threads), 0);
    // warm up once per thread to amortise thread_local dataset allocation
    std::vector<std::thread> warm;
    for (int t = 0; t < threads; ++t) {
        warm.emplace_back([&] {
            std::uint8_t h[80];
            std::memcpy(h, base, sizeof(h));
            PutU32LE(h + 76, 0xdeadbeefu);
            volatile auto x = phr_pow::HashHeaderPHR(h, sizeof(h));
            (void)x;
        });
    }
    for (auto& th : warm) th.join();

    double cpu0 = CpuSeconds();
    auto t0 = Clock::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            std::uint64_t local = 0;
            volatile std::uint8_t sink = 0;
            for (std::uint64_t i = t; i < n; i += static_cast<std::uint64_t>(threads)) {
                std::uint8_t h[80];
                std::memcpy(h, base, sizeof(h));
                PutU32LE(h + 76, static_cast<std::uint32_t>(i));  // vary nonce like real mining
                auto d = phr_pow::HashHeaderPHR(h, sizeof(h));
                sink ^= d[0];
                ++local;
            }
            per_thread[static_cast<std::size_t>(t)] = local;
            (void)sink;
        });
    }
    for (auto& th : pool) th.join();
    auto t1 = Clock::now();
    double cpu1 = CpuSeconds();
    double wall = std::chrono::duration<double>(t1 - t0).count();
    double cpu = cpu1 - cpu0;
    std::uint64_t total = 0;
    for (auto v : per_thread) total += v;
    std::cout << "threads=" << threads << " hashes=" << total << " wall_s=" << wall
              << " cpu_s=" << cpu << " cpu_util=" << (wall > 0 ? cpu / wall : 0)
              << " hash_per_s=" << (wall > 0 ? total / wall : 0)
              << " hash_per_min=" << (wall > 0 ? total / wall * 60 : 0)
              << " latency_ms=" << (total ? wall * 1000.0 / static_cast<double>(total) : 0)
              << " rss_kb=" << ProcField("VmRSS:") << " hwm_kb=" << ProcField("VmHWM:")
              << " alloc_count=" << g_alloc_count.load() << " alloc_bytes=" << g_alloc_bytes.load() << "\n";
    return 0;
}

int BenchReference(std::uint64_t n, int mode) {
    const std::uint8_t header[80] = {};
    auto t0 = Clock::now();
    volatile std::uint8_t sink = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        auto d = phr_pow::Sha256(header, sizeof(header));
        if (mode == 1) d = phr_pow::Sha256(d.data(), d.size());  // SHA256d
        sink ^= d[0];
    }
    auto t1 = Clock::now();
    double wall = std::chrono::duration<double>(t1 - t0).count();
    (void)sink;
    std::cout << "reference=" << (mode == 1 ? "sha256d" : "sha256") << " hashes=" << n
              << " wall_s=" << wall << " hash_per_s=" << (wall > 0 ? n / wall : 0)
              << " hash_per_min=" << (wall > 0 ? n / wall * 60 : 0)
              << " latency_us=" << (n ? wall * 1e6 / static_cast<double>(n) : 0) << "\n";
    return 0;
}

int BenchDataset(std::uint64_t n) {
    static thread_local std::vector<std::uint8_t> dataset;
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < n; ++i) {
        phr_pow::Hash key{};
        for (int j = 0; j < 32; ++j) key[static_cast<std::size_t>(j)] = static_cast<std::uint8_t>(i + j);
        phr_pow::MakeDataset(key, 1024 * 1024, dataset);
    }
    auto t1 = Clock::now();
    double wall = std::chrono::duration<double>(t1 - t0).count();
    volatile std::uint8_t sink = dataset[0];
    (void)sink;
    std::cout << "dataset_only fills=" << n << " wall_s=" << wall
              << " fills_per_s=" << (wall > 0 ? n / wall : 0)
              << " latency_ms=" << (n ? wall * 1000.0 / static_cast<double>(n) : 0) << "\n";
    return 0;
}

int Memory(std::uint64_t n) {
    auto rss = [] { return ProcField("VmRSS:"); };
    auto hwm = [] { return ProcField("VmHWM:"); };
    std::cout << "before\trss_kb=" << rss() << " hwm_kb=" << hwm()
              << " alloc_count=" << g_alloc_count << " alloc_bytes=" << g_alloc_bytes << "\n";
    const std::uint8_t header[80] = {};
    auto a0 = g_alloc_count.load(), b0 = g_alloc_bytes.load();
    volatile std::uint8_t sink = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        auto d = phr_pow::HashHeaderPHR(header, sizeof(header));
        sink ^= d[0];
    }
    (void)sink;
    auto a1 = g_alloc_count.load(), b1 = g_alloc_bytes.load();
    std::cout << "after\tn=" << n << " rss_kb=" << rss() << " hwm_kb=" << hwm()
              << " alloc_count_delta=" << (a1 - a0) << " alloc_bytes_delta=" << (b1 - b0)
              << " bytes_per_hash=" << (n ? (b1 - b0) / n : 0) << "\n";
    std::cout << "dataset_bytes_expected=" << (1024 * 1024) << " dataset_lines=" << (1024 * 1024 / 64) << "\n";
    return 0;
}

std::vector<std::uint8_t> GenesisHeader(std::uint32_t n_time, std::uint32_t n_bits,
                                        std::uint32_t n_nonce, const std::string& merkle_hex,
                                        std::int32_t n_version = 1) {
    std::vector<std::uint8_t> h(80, 0);
    PutU32LE(h.data(), static_cast<std::uint32_t>(n_version));
    auto merkle = FromHex(merkle_hex);
    std::reverse(merkle.begin(), merkle.end());  // uint256 internal LE storage
    std::copy(merkle.begin(), merkle.end(), h.begin() + 36);
    PutU32LE(h.data() + 68, n_time);
    PutU32LE(h.data() + 72, n_bits);
    PutU32LE(h.data() + 76, n_nonce);
    return h;
}

std::array<std::uint8_t, 32> CompactTarget(std::uint32_t bits) {
    std::array<std::uint8_t, 32> t{};
    int exp = static_cast<int>(bits >> 24);
    std::uint32_t mant = bits & 0x007fffffu;
    if (exp <= 3) {
        std::uint32_t v = mant >> (8 * (3 - exp));
        t[29] = static_cast<std::uint8_t>(v >> 16);
        t[30] = static_cast<std::uint8_t>(v >> 8);
        t[31] = static_cast<std::uint8_t>(v);
    } else {
        // mantissa occupies the highest 3 bytes; trailing zero bytes = exp-3.
        int start = 32 - exp;
        if (start >= 0 && start + 2 < 32) {
            t[static_cast<std::size_t>(start)] = static_cast<std::uint8_t>(mant >> 16);
            t[static_cast<std::size_t>(start + 1)] = static_cast<std::uint8_t>(mant >> 8);
            t[static_cast<std::size_t>(start + 2)] = static_cast<std::uint8_t>(mant);
        }
    }
    return t;
}

int Mine(std::uint64_t blocks, std::uint32_t bits) {
    auto target = CompactTarget(bits);
    std::array<std::uint8_t, 32> prev{};
    std::uint64_t total_attempts = 0;
    std::uint64_t verified = 0;
    auto t0 = Clock::now();
    for (std::uint64_t b = 0; b < blocks; ++b) {
        std::uint8_t h[80] = {};
        PutU32LE(h, 1);
        std::copy(prev.begin(), prev.end(), h + 4);
        for (int i = 0; i < 32; ++i) h[36 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
        PutU32LE(h + 68, 1417713337u + static_cast<std::uint32_t>(b));
        PutU32LE(h + 72, bits);
        std::uint64_t attempts = 0;
        phr_pow::Hash found{};
        bool ok = false;
        for (std::uint32_t nonce = 0; nonce < 1000000u; ++nonce) {
            PutU32LE(h + 76, nonce);
            auto d = phr_pow::HashHeaderPHR(h, sizeof(h));
            ++attempts;
            if (std::memcmp(d.data(), target.data(), 32) <= 0) {
                found = d;
                ok = true;
                break;
            }
        }
        if (!ok) {
            std::cerr << "mine: no nonce found for block " << b << "\n";
            return 1;
        }
        // Determinism check: recompute the winning header.
        auto again = phr_pow::HashHeaderPHR(h, sizeof(h));
        if (again != found) {
            std::cerr << "mine: nondeterministic hash at block " << b << "\n";
            return 1;
        }
        ++verified;
        total_attempts += attempts;
        prev = found;
    }
    auto t1 = Clock::now();
    double wall = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "mine blocks=" << blocks << " bits=0x" << std::hex << bits << std::dec
              << " attempts=" << total_attempts << " avg_attempts_per_block="
              << (blocks ? static_cast<double>(total_attempts) / static_cast<double>(blocks) : 0)
              << " verified=" << verified << " wall_s=" << wall
              << " hash_per_s=" << (wall > 0 ? total_attempts / wall : 0)
              << " rss_kb=" << ProcField("VmRSS:") << " hwm_kb=" << ProcField("VmHWM:")
              << " final_chain_tip=" << Hex(prev.data(), 32) << "\n";
    return 0;
}

// Mirrors src/pow.cpp:CheckProofOfWork using the same arith_uint256 primitive.
bool TargetOk(const uint256& hash, std::uint32_t bits, const uint256& pow_limit)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 target;
    target.SetCompact(bits, &fNegative, &fOverflow);
    if (fNegative || target == 0 || fOverflow || target > UintToArith256(pow_limit)) return false;
    return UintToArith256(hash) <= target;
}

uint256 MainPowLimit()
{
    // Fresh PHR chain uses the same powLimit as regtest/devnet, matching the
    // genesis nBits 0x207fffff.
    uint256 v;
    std::memset(v.begin(), 0xff, 32);
    v.begin()[31] = 0x7f;
    return v;
}

uint256 RegtestPowLimit()
{
    uint256 v;
    std::memset(v.begin(), 0xff, 32);
    v.begin()[31] = 0x7f;
    return v;
}

uint256 HashOne()
{
    uint256 v{};
    v.begin()[0] = 1;
    return v;
}

uint256 DigestToUint256(const phr_pow::Hash& d)
{
    uint256 v;
    for (std::size_t i = 0; i < 32; ++i) v.begin()[i] = d[31 - i];
    return v;
}

int CompactTarget()
{
    const uint256 main_limit = MainPowLimit();
    const auto genesis = GenesisHeader(1417713337u, 0x207fffffu, 0u,
                                       "e0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7");
    const uint256 genesis_hash = DigestToUint256(phr_pow::HashHeaderPHR(genesis.data(), genesis.size()));

    struct Case { const char* name; uint256 hash; std::uint32_t bits; uint256 limit; bool expected; };
    const Case cases[] = {
        {"hash_one_vs_0x1e0ffff0", HashOne(), 0x1e0ffff0u, main_limit, true},
        {"genesis_vs_0x1e0ffff0", genesis_hash, 0x1e0ffff0u, main_limit, false},
        {"negative_compact", HashOne(), 0x1e8ffff0u, main_limit, false},
        {"zero_compact", HashOne(), 0x00000000u, main_limit, false},
        {"genesis_vs_regtest_0x207fffff", genesis_hash, 0x207fffffu, RegtestPowLimit(), true},
    };
    int fails = 0;
    std::cout << "case\texpected\tactual\tstatus\n";
    for (const auto& c : cases) {
        const bool actual = TargetOk(c.hash, c.bits, c.limit);
        const bool ok = actual == c.expected;
        if (!ok) ++fails;
        std::cout << c.name << '\t' << (c.expected ? "true" : "false") << '\t'
                  << (actual ? "true" : "false") << '\t' << (ok ? "PASS" : "FAIL") << '\n';
    }

    // Nonce scan under regtest consensus, same shape as the in-tree Boost test.
    std::uint8_t h[80] = {};
    PutU32LE(h, 1);
    PutU32LE(h + 68, 1417713337u);
    PutU32LE(h + 72, 0x207fffffu);
    std::uint32_t nonce = 0;
    while (nonce < 1000 && !TargetOk(DigestToUint256(phr_pow::HashHeaderPHR(h, sizeof(h))),
                                     0x207fffffu, RegtestPowLimit())) {
        PutU32LE(h + 76, ++nonce);
    }
    const bool scan_ok = nonce < 1000;
    std::cout << "nonce_scan\tfound_nonce=" << nonce << '\t' << (scan_ok ? "PASS" : "FAIL") << '\n';
    if (!scan_ok) ++fails;
    return fails == 0 ? 0 : 1;
}

int Consensus() {
    const std::string merkle = "e0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7";
    struct Case { const char* name; std::uint32_t time; std::uint32_t nonce; const char* expected; };
    const Case cases[] = {
        {"main", 1390095618u, 0u, "67215764b843ce5cbb30a6166c083fe4808019e844c3b57d4cc117d9186ad2fc"},
        {"testnet", 1390666206u, 1u, "26aea3dd1aad231fc1cf628b1ba2946bcdb709a814eff5d08f4538ed86bacea3"},
        {"devnet_base", 1417713337u, 0u, "403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077"},
        {"regtest", 1417713337u, 0u, "403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077"},
    };
    int fails = 0;
    std::cout << "network\tserialized_header_hex\texpected\tactual\tstatus\tpow_valid\n";
    for (const auto& c : cases) {
        auto h = GenesisHeader(c.time, 0x207fffffu, c.nonce, merkle);
        auto digest = phr_pow::HashHeaderPHR(h.data(), h.size());
        std::string actual = Hex(digest);
        bool ok = actual == c.expected;
        // compact 0x207fffff => target = 0x7fffff << (8*(0x20-3)), 32-byte BE:
        // 7f ff ff 00 ... 00 (29 zero bytes after the 3 mantissa bytes).
        std::uint8_t target[32] = {0};
        target[0] = 0x7f; target[1] = 0xff; target[2] = 0xff;
        int cmp = std::memcmp(digest.data(), target, 32);
        bool pow = cmp <= 0;
        if (!ok || !pow) ++fails;
        std::cout << c.name << '\t' << HeaderHex(h) << '\t' << c.expected << '\t' << actual
                  << '\t' << (ok ? "PASS" : "FAIL") << '\t' << (pow ? "PASS" : "FAIL") << '\n';
    }
    // Verify HashPHR uint256 mapping equals raw big-endian digest.
    std::vector<std::uint8_t> h = GenesisHeader(1390095618u, 0x207fffffu, 0u, merkle);
    uint256 u = HashPHR(h.data(), h.data() + h.size());
    auto raw = phr_pow::HashHeaderPHR(h.data(), h.size());
    std::string mapped;
    for (int i = 31; i >= 0; --i) mapped += Hex(&u.begin()[static_cast<std::size_t>(i)], 1);
    bool map_ok = mapped == Hex(raw);
    std::cout << "uint256_mapping\t" << (map_ok ? "PASS" : "FAIL") << '\t' << mapped << '\n';
    if (!map_ok) ++fails;
    return fails == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: phr_harness <vectors|primitives|dist|avalanche|bench|memory|compact_target|consensus> [args]\n";
        return 2;
    }
    std::string mode = argv[1];
    try {
        if (mode == "vectors") return Vectors(argc > 2 ? argv[2] : "vectors.tsv");
        if (mode == "primitives") return Primitives();
        if (mode == "dist") return Distribution(argc > 2 ? std::stoull(argv[2]) : 200, argc > 3 ? std::stoull(argv[3]) : 1);
        if (mode == "avalanche") return Avalanche(argc > 2 ? std::stoull(argv[2]) : 100, argc > 3 ? std::stoull(argv[3]) : 2);
        if (mode == "bench") return Bench(argc > 2 ? std::stoull(argv[2]) : 20, argc > 3 ? std::stoi(argv[3]) : 1);
        if (mode == "bench_sha") return BenchReference(argc > 2 ? std::stoull(argv[2]) : 1000, argc > 3 ? std::stoi(argv[3]) : 0);
        if (mode == "bench_dataset") return BenchDataset(argc > 2 ? std::stoull(argv[2]) : 200);
        if (mode == "memory") return Memory(argc > 2 ? std::stoull(argv[2]) : 20);
        if (mode == "mine") return Mine(argc > 2 ? std::stoull(argv[2]) : 20, argc > 3 ? static_cast<std::uint32_t>(std::stoul(argv[3], nullptr, 0)) : 0x207fffffu);
        if (mode == "compact_target") return CompactTarget();
        if (mode == "consensus") return Consensus();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
    std::cerr << "unknown mode " << mode << "\n";
    return 2;
}
