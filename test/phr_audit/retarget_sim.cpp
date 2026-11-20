// Standalone retarget simulator using the live src/pow.cpp implementation.
// Build: g++ -std=c++20 -O2 -DHAVE_CONFIG_H -I <stubs> -I src \
//        test/phr_audit/retarget_sim.cpp src/pow.cpp src/chain.cpp \
//        src/util/time.cpp src/util/check.cpp src/arith_uint256.cpp \
//        src/uint256.cpp src/util/strencodings.cpp -o ...
#include <pow.h>

#include <chain.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <uint256.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

// Stub required only because util/check.cpp references FormatFullVersion().
std::string FormatFullVersion() { return "22.0.0-audit"; }

namespace {
constexpr std::uint32_t kGenesisBits = 0x207fffffu;

Consensus::Params MakeParams(const std::string& pow_limit_hex,
                             std::uint32_t kgw_height,
                             std::uint32_t dgw_height,
                             bool no_retarget)
{
    Consensus::Params p{};
    p.powLimit = uint256S(pow_limit_hex);
    p.nPowTargetTimespan = 24 * 60 * 60;
    p.nPowTargetSpacing = 2.5 * 60;
    p.fPowAllowMinDifficultyBlocks = false;
    p.fPowNoRetargeting = no_retarget;
    p.nPowKGWHeight = static_cast<int>(kgw_height);
    p.nPowDGWHeight = static_cast<int>(dgw_height);
    return p;
}

std::string CompactHex(unsigned int bits)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08x", bits);
    return buf;
}
} // namespace

int main()
{
    constexpr int kBlocks = 5000;
    const std::string old_limit = "00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    const std::string fresh_limit = "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

    struct Scenario {
        const char* name;
        std::string pow_limit;
        std::uint32_t kgw;
        std::uint32_t dgw;
        bool no_retarget;
    };
    const Scenario scenarios[] = {
        {"old_main_powlimit_old_schedule", old_limit, 15200, 34140, false},
        {"old_main_powlimit_fresh_schedule", old_limit, 0, 0, false},
        {"fresh_powlimit_fresh_dgw", fresh_limit, 0, 0, false},
        {"fresh_powlimit_btc_until_576", fresh_limit, 15200, 34140, false},
        {"regtest_no_retarget", fresh_limit, 15200, 34140, true},
    };

    int failures = 0;
    for (const auto& s : scenarios) {
        const auto params = MakeParams(s.pow_limit, s.kgw, s.dgw, s.no_retarget);
        std::vector<CBlockIndex> chain(static_cast<std::size_t>(kBlocks) + 1);
        chain[0].nHeight = 0;
        chain[0].nTime = 1795153201u;
        chain[0].nBits = kGenesisBits;
        chain[0].pprev = nullptr;

        unsigned int min_bits = 0xffffffffu;
        unsigned int max_bits = 0;
        bool valid = true;
        int first_invalid_height = -1;
        for (int h = 1; h <= kBlocks; ++h) {
            auto& index = chain[static_cast<std::size_t>(h)];
            index.nHeight = h;
            index.nTime = chain[static_cast<std::size_t>(h - 1)].nTime +
                          static_cast<std::uint32_t>(params.nPowTargetSpacing);
            index.pprev = &chain[static_cast<std::size_t>(h - 1)];

            CBlockHeader next_header{};
            next_header.nTime = index.nTime;
            next_header.nBits = kGenesisBits;
            const unsigned int bits = GetNextWorkRequired(&chain[static_cast<std::size_t>(h - 1)], &next_header, params);
            index.nBits = bits;

            arith_uint256 target;
            bool neg = false;
            bool overflow = false;
            target.SetCompact(bits, &neg, &overflow);
            const bool bits_ok = !neg && !overflow && target != 0 && target <= UintToArith256(params.powLimit);
            if (!bits_ok && first_invalid_height < 0) first_invalid_height = h;
            if (!bits_ok) valid = false;
            min_bits = std::min(min_bits, bits);
            max_bits = std::max(max_bits, bits);
        }
        if (!valid) ++failures;
        std::cout << s.name
                  << "\tblocks=" << kBlocks
                  << "\tvalid_bits=" << (valid ? "true" : "false")
                  << "\tfirst_invalid_height=" << first_invalid_height
                  << "\tbits_h1=" << CompactHex(chain[1].nBits)
                  << "\tbits_h100=" << CompactHex(chain[100].nBits)
                  << "\tbits_h576=" << CompactHex(chain[576].nBits)
                  << "\tbits_h2016=" << CompactHex(chain[2016].nBits)
                  << "\tbits_h5000=" << CompactHex(chain[static_cast<std::size_t>(kBlocks)].nBits)
                  << "\tmin_bits=" << CompactHex(min_bits)
                  << "\tmax_bits=" << CompactHex(max_bits)
                  << "\n";
    }
    std::cout << "retarget_sim_failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
