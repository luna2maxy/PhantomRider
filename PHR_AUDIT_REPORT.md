# PHR-256-YP — Technical Audit — Dash Core Integration

**Date:** 2026-11-19
**Environment:** `/home/ubuntu/Documents/GitHub/PhantomRider`, Linux x86_64, AMD Ryzen 5 4600H (6 vCPU), 16 GiB RAM, GCC 15.2.0, C++20.
**Independent reference:** `/home/ubuntu/Documents/pool/algorithm/phantomrider` (`phr_yp_ref.py`, `phr_yp_selftest.cpp`).
**Scope limitations:** No changes to the algorithm, genesis block, consensus parameters, rewards, difficulty, or header format. The only changes are new audit tooling in `test/phr_audit/` and this report.

---

## 1. Executive Summary

| Aspect | Conclusion | Status |
|---|---|---|
| Implementation conforms to specification & pool vectors | 11/11 vectors PASS, including 3 official vectors | **PASS** |
| Internal primitives (SHA-256, SHA3-256, BLAKE2s) | Matches standard `hashlib` | **PASS** |
| Determinism (executions, -O0…-O3, -march=native, ASan/UBSan) | All identical, no sanitizer errors | **PASS** |
| Distribution & avalanche (5,000 hashes, 2,500 pairs) | All statistical tests PASS at α=0.01 | **PASS** |
| Block hash / target / genesis consistency (4 networks) | 80-byte header reconstruction produces the asserted hash | **PASS** |
| Isolated mining (200 blocks, 2 processes) | 417 attempts, 200/200 validated, identical tip | **PASS** |
| Full `dashd` build / functional tests / node synchronization | Could not be run: depends toolchain & system dependencies absent | **INCONCLUSIVE** |
| GPU benchmark | No GPU implementation in the codebase | **INCONCLUSIVE** |
| Mainnet readiness | **NOT READY** | **FAIL (deployment readiness)** |

**Most important finding:** the algorithm runs correctly and deterministically, but the "memory-hard / ASIC-resistant" claim **cannot be justified** from this implementation. The 1 MiB dataset is rebuilt for every nonce and only 32 of 16,384 rows are read; the *live state* is very small. Cost is dominated ~(94–97%) by sequential ChaCha20 filling (~3.46 ms of ~3.6 ms per hash). In addition, stale consensus metadata (`nMinimumChainWork`, `defaultAssumeValid`) still refers to the old X11 chain — a blocker for Mainnet deployment.

**Warning:** Passing statistical tests is **not** proof of cryptographic security. There is no external cryptanalysis, design paper, or published round-count argument for this 32-round construction.

---

## 2. Source Code Audit

**Method:** manual reading of `src/hash_phr.h`, `src/primitives/block.cpp`, `src/pow.cpp`, `src/kernel/chainparams.cpp`, `src/primitives/block.h`, `src/bench/pow_hash.cpp`, `src/bench/crypto_hash.cpp`; tracing of all callers of `HashPHR`, `GetHash()`, `CheckProofOfWork()`; compilation with sanitizers and an optimization matrix.

### 2.1 Implementation map

| File | Role |
|---|---|
| `src/hash_phr.h` | All PHR-256-YP logic (SHA-256, Keccak/SHA3-256, BLAKE2s, ChaCha20, dataset, 32 rounds, finalization, `HashPHR`) |
| `src/primitives/block.cpp` | `CBlockHeader::GetHash()` → 80-byte serialization → `HashPHR` |
| `src/pow.cpp` | `CheckProofOfWork()` compares `UintToArith256(hash)` with the compact target; DGW/KGW retargeting unchanged |
| `src/kernel/chainparams.cpp` | New genesis for main/testnet/devnet/regtest; `FindDevNetGenesisBlock` mines via `GetHash()` |
| `src/bench/pow_hash.cpp`, `src/bench/crypto_hash.cpp` | Benchmarks (`Pow_PHR_*` names already correct, no change needed) |

### 2.2 Code audit findings

| # | Area | Finding | Risk | Status |
|---|---|---|---|---|
| S1 | Endianness | All conversions are explicit (`Load32LE/BE`, `Store32LE/BE`, `Load64LE`, `Store64BE`). `HashPHR` writes `digest[31-i]` to `uint256.begin()+i`, so `GetHex()` == the big-endian digest. No host endianness dependency. | — | **PASS** |
| S2 | 80-byte input | `CBlockHeader::SERIALIZE_METHODS` = 4+32+32+4+4+4 = 80 bytes. Manual 80-byte reconstruction in the harness produces a genesis hash exactly matching the `assert` in `chainparams.cpp`. | — | **PASS** |
| S3 | Dataset initialization | `MakeDataset` creates 16,384 64-byte ChaCha20 blocks; the first 32 bytes of each block become the key for the next block (sequential, cannot be skipped). The `thread_local` 1 MiB buffer is allocated once per thread. | — | **PASS** |
| S4 | 32 rounds | `idx = Load64LE(state) % 16384`; `selector = state[8] % 3`; SHA-256 / SHA3-256 / BLAKE2s selected per round; message = `state(32) || line(64) || r(4 LE) || selector(1)` = 101 bytes. Matches the pool reference. | — | **PASS** |
| S5 | Finalization | `SHA256("PHR-YP-FINAL\0" || state || header || pers)` → 32-byte digest. Domain separation via tags (`DATASET`, `INIT`, `FINAL`) is present. | — | **PASS** |
| S6 | Integer overflow / UB | No shifts by n=0 in rotations; `len*8` is safe for 80-byte inputs; `idx*64` < 1 MiB. ASan+UBSan clean on vectors and mining. | — | **PASS** |
| S7 | Buffer overflow | All array accesses are in bounds; sanitizers reported nothing. | — | **PASS** |
| S8 | Empty input | `HashPHR` replaces a null pointer with the string literal `""`, `len=0`; the `empty_input` vector PASSes. | — | **PASS** |
| S9 | Portability include | `hash_phr.h` uses `std::array` but does not `#include <array>` directly; it is inherited from `uint256.h`. | LOW | **WARN** |
| S10 | API misuse | `HashHeaderPHR(..., const char* pers_c, ...)` would be UB if `pers_c == nullptr`; unreachable from `HashPHR` (always `"PHR"`). | LOW | **INFORMATIONAL** |
| S11 | Compiler warning | `-Wall -Wextra` triggers `-Wmisleading-indentation` at `hash_phr.h:126` (two statements on one line). Cosmetic, does not change results. | INFORMATIONAL | **WARN** |
| S12 | X11 remnants | `src/crypto/x11/*` still exists and is referenced by CMake, but is not used for block PoW. No active `GetPoWHash()`. | INFORMATIONAL | **INFORMATIONAL** |

**Compiler/optimization matrix:** `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, `-O2 -march=native`, `-O2 -fno-strict-aliasing` → 11/11 vectors PASS in every configuration. No dependency on optimization level observed.

---

## 3. Hash Correctness

**Method:** expected values were produced by an independent Python implementation (`phr_yp_ref.py` from the pool), not by the code under test. The harness includes `src/hash_phr.h` directly.

**Tools:** `test/phr_audit/gen_vectors.py`, `phr_harness vectors`, `phr_harness primitives`, GCC, ASan/UBSan.
**Sample:** 11 vectors + 3 primitives + 7 compiler configurations + sanitizers.

| Case | Input | Expected | Actual | Status |
|---|---|---|---|---|
| official_64B_1r | `00010203`, 64B, 1 round | `4363c051…4243` | same | **PASS** |
| official_1MiB_32r | `00010203` | `6e1e85ee…3e1e` | same | **PASS** |
| official_80B_1MiB_32r | `00..4f` | `ade86171…eae2` | same | **PASS** |
| empty_input | 0 bytes | `8effbd29…8eeb` | same | **PASS** |
| all_zero_80B | 80×`00` | `76d1dd57…8209` | same | **PASS** |
| all_ff_80B | 80×`ff` | `d8419a25…c079` | same | **PASS** |
| normal_header | 80B header, nonce 0 | `7348f556…4240` | same | **PASS** |
| single_bit_flip | bit 0 of byte 0 flipped | `82b4bee5…98d2` | same | **PASS** |
| nonce_plus_one | nonce 0→1 | `5285e912…ca6e` | same | **PASS** |
| timestamp_plus_one | time +1 second | `a346fec3…b0b3` | same | **PASS** |
| merkle_root_bit_flip | merkle bit flipped | `4891fb36…7d33` | same | **PASS** |

Full evidence (full hex): `test/phr_audit/out/vectors.tsv`.

**Primitive check (`"abc"`):**

| Primitive | Harness | `hashlib` | Status |
|---|---|---|---|
| SHA-256 | `ba7816bf…15ad` | `ba7816bf…15ad` | **PASS** |
| SHA3-256 | `3a985da7…3532` | `3a985da7…3532` | **PASS** |
| BLAKE2s | `508c5e8c…5982` | `508c5e8c…5982` | **PASS** |

**Overall status: PASS (11/11 + 3/3).**

---

## 4. Hash Distribution

**Method:** 5,000 deterministic random 80-byte headers (SplitMix64, seed 1337) → 5,000 digests; byte chi-square (df=255), bit-position chi-square (df=255), global bit-1 proportion (z-test), lag 1–10 autocorrelation on the first 64-bit numeric value, serial bit correlation.
**Tools:** `phr_harness dist`, `analyze.py` (stdlib incomplete gamma implementation).
**Sample:** 5,000 hashes = 1,280,000 bits = 160,000 bytes.

| Test | Statistic | p-value | Status |
|---|---|---|---|
| Byte frequency (χ², df=255) | 275.19 (expected ≈255) | 0.1839 | **PASS** |
| Bit frequency per position (χ², df=255) | 141.11 | ~1.000 | **PASS** |
| Bit-1 proportion | 0.499612 (z = −0.879) | 0.3796 | **PASS** |
| Lag 1–10 autocorrelation | −0.0150 … +0.0032 | within ±0.0424 band | **PASS** |
| Lag-1 serial bit correlation | −0.000154 | within band | **PASS** |

Evidence: `out/dist.json`, `out/stats.json`, `out/stats.md`, `out/chart_byte_freq.svg`.

**Methodological note:** during the initial audit the harness had two bugs — (a) `values` was stored as `double`, so the 64-bit low byte was discarded (p~0), and (b) the expected byte frequency incorrectly used 256 bytes per hash, even though the digest is only 32 bytes. Both were fixed before the results above were taken; these findings were tooling bugs, not algorithm bugs.

---

## 5. Avalanche Effect

**Method:** 2,500 header pairs; one input bit flipped at a random position (0..639); count the output bits that change.
**Tools:** `phr_harness avalanche`, `analyze.py`.
**Sample:** 2,500 pairs = 5,000 hashes = 640,000 output bits compared.

| Metric | Result | Ideal | Status |
|---|---|---|---|
| Average bits changed | 128.154 (50.06%) | 128 (50%) | **PASS** |
| Stddev | 7.920 | √64 = 8 | **PASS** |
| Min / Max | 98 / 160 | — | **PASS** |
| z-test mean vs 128 | 0.970 (p=0.332) | |a|<1.96 | **PASS** |
| χ² per output bit position (df=255) | 130.72 (p≈1) | ~255 | **PASS** |

Evidence: `out/avalanche.json`, `out/chart_avalanche_hist.svg`, `out/chart_avalanche_bits.svg`.

---

## 6. Performance Benchmark

**Method:** 600 hashes per configuration, nonce changed each iteration (to prevent *constant folding*), 3 repetitions. Time via `steady_clock`, CPU via `/proc/self/stat`, RSS/VmHWM via `/proc/self/status`, allocations via a global `operator new` override.
**Tools:** `phr_harness bench`, `bench_sha`, `bench_dataset`.
**Hardware:** AMD Ryzen 5 4600H, 2 vCPU, GCC 15.2.0 `-O2`.

### 6.1 PHR-256-YP, 80-byte input, varying nonce

| Thread | H/s (3 runs) | Median H/s | H/minute | Latency/hash | RSS |
|---|---|---|---|---|---|
| 1 | 276.6 / 274.4 / 273.3 | **274.4** | 16,464 | 3.64 ms | 5.5 MB |
| 2 | 355.7 / 397.9 / 349.1 | **355.7** | 21,340 | 2.81 ms | 6.5 MB |
| 4 (oversubscribed) | 447.4 / 427.1 / 545.9 | **447.4** | 26,842 | 2.24 ms | 8.6 MB |

**Scaling:** 1→2 threads = 1.30×; 1→4 threads = 1.63×; 4 threads on 2 vCPU is not linear. Observed CPU utilization 0.45–0.96 (using 10 ms ticks; approximate).

### 6.2 Reference (same hardware & compiler)

| Function | H/s | H/minute | Latency | Ratio vs PHR (1T) |
|---|---|---|---|---|
| SHA-256 primitive (80 B) | 1,737,880 | 104,272,800 | 0.58 µs | ~6,333× faster |
| SHA-256d primitive (80 B) | 1,130,390 | 67,823,200 | 0.88 µs | ~4,119× faster |
| Dataset fill only (1 MiB) | 289.2 fill/s | — | 3.46 ms | ~95% of total hash time |

**Status: PASS (measured).** **INCONCLUSIVE for GPU:** no GPU implementation in the codebase. **INCONCLUSIVE for full X11 comparison:** no active X11 build; the comparison above is only against SHA-256/SHA-256d primitives on the same machine — do not interpret it as a like-for-like PoW comparison.

Note: an initial benchmark with a constant header produced ~400 H/s because the compiler could hoist invariant work; those numbers are **not used** in this report.

---

## 7. Memory Analysis

**Method:** global allocation tracking + `/proc/self/status`, 50 consecutive hashes, ASan leak detection, fill-only benchmark.
**Tools:** `phr_harness memory`, `bench_dataset`, ASan.

| Question | Evidence | Status |
|---|---|---|
| Is the 1 MiB dataset actually used? | Yes: 16,384 64-byte ChaCha20 blocks; one block = the next block's key | **PASS** |
| Rebuilt for every hash? | Yes, its contents are regenerated; the key depends on `header||pers`, so a new nonce → a new dataset | **PASS** |
| Can the buffer be reused? | Yes, `thread_local vector::resize` does not reallocate after warm-up | **PASS** |
| Allocation per hash | 1,103,176 bytes / 50 hashes = 1,048,576 (dataset once) + 54,600 small ≈ **1,092 bytes/hash**, ~13 small allocations (TaggedInput vectors) | **PASS** |
| Peak memory | VmHWM 5.3 MB (1 thread), 6.5 MB (2 thread), 8.6 MB (4 thread) → +~1 MiB/thread | **PASS** |
| Memory leak | None; ASan `detect_leaks=1` clean; VmHWM stable | **PASS** |
| Cache efficiency | 32 random accesses to the 1 MiB buffer per hash (≈32/16,384 = 0.2% of rows used); the remaining 99.8% is only written, never read | **WARN** |
| Bottleneck | Yes, but **compute-bound**, not memory-bandwidth-bound: ChaCha20 fill ~3.46 ms of ~3.6 ms/hash | **WARN** |

**Key conclusion:** this is **not memory-hard** in the Ethash/Romix sense. The dataset does not require high-latency DRAM: because generation is sequential, hardware only needs to store the 32-byte chain state and the 32 selected rows. The 1 MiB size must not be equated with ASIC resistance.

---

## 8. Cryptographic Security Analysis

**Method:** structural review + the empirical tests above + comparison of primitives with standards. **No** external cryptanalysis was run or is available.

| Property | Assessment | Evidence / limitation |
|---|---|---|
| Collision resistance | Unproven; no direct structural findings | 256-bit SHA-256 finalization; no differential analysis/publication |
| Preimage / second-preimage | Unproven; no visible shortcut | Dataset depends entirely on the header; 32-bit state? no — 32 bytes; domain separation tags present |
| Weak initialization | None seen | `INIT` = SHA256(tag||header||pers), dataset key = SHA256(tag||header||pers) with different tags |
| Weak finalization | None seen | `FINAL` binds state + header + pers |
| Repeated-state behavior | None seen | state is always a 256-bit hash; no state passes without an update |
| Differential cryptanalysis | Not yet performed | 50.06% avalanche is only a statistical indicator, not proof of differential resistance |
| Rounds = 32 | **No security justification** in the repository | No design document/argument of margin against reduced-round attacks |
| Overall risk | **HIGH (unproven)** | Do not claim safety merely because the output is 256 bits or statistical tests pass |

No exploitable weakness was found within the scope of this audit, but there is **no adequate cryptographic security evidence** for Mainnet.

---

## 9. ASIC Resistance Analysis

| Platform | Qualitative assessment | Reason |
|---|---|---|
| CPU | **Friendly (relatively)** | 274 H/s/thread; 1 MiB buffer per thread; no special instructions |
| GPU | **Not friendly currently** | Each thread needs a 1 MiB working set or a sequential stream; sequential dependencies between ChaCha20 blocks hinder coalescing; 32 selector branches |
| FPGA | **Fairly friendly** | 32-bit integer operations, rotations, and batched ChaCha20; small SRAM suffices if only selected rows are stored |
| ASIC | **Friendly** | 32-byte state chain + 32 selected rows; no 1 MiB DRAM needed; ChaCha20 pipelining is very efficient |

**Conclusion:** the ASIC resistance claim is **weak / unproven**. The 1 MiB dataset is a *streaming working buffer*, not expensive random memory. This design is better described as **CPU-friendly, compute-bound, sequential-ChaCha-bound**. If the goal is ASIC resistance, a design with repeated random DRAM access (e.g. timely/latency-bound ROM) and hardware cost analysis is required — not merely increasing the buffer size.

---

## 10. Blockchain Consensus Test

**Method:** reconstruction of the 80-byte header from genesis parameters in `chainparams.cpp`, hashing with the active implementation, comparison with the genesis `assert` and compact target; static audit of all callers of `CheckProofOfWork()`.
**Tools:** `phr_harness consensus`, grep/code inspection.
**Sample:** 4 networks + uint256 mapping + 20+ call sites.

| Network | Reconstructed hash vs assert | PoW vs target 0x207fffff |
|---|---|---|
| main | `67215764…ad2fc` — **PASS** | **PASS** |
| testnet | `26aea3dd…acea3` — **PASS** | **PASS** |
| devnet (base) | `403ac5ae…1077` — **PASS** | **PASS** |
| regtest | `403ac5ae…1077` — **PASS** | **PASS** |
| `uint256` mapping | digest big-endian == `GetHex()` — **PASS** | — |

Static audit: all paths (validation.cpp:4519, blockstorage.cpp:924, txdb.cpp:323, rpc/mining.cpp:147, miner_tests, mining util) call `GetHash()` → `HashPHR`. No active X11 `GetPoWHash()`. Merkle root & tx hashes remain Double SHA-256.

### Critical consensus findings

| ID | Finding | Risk | Status |
|---|---|---|---|
| C1 | `consensus.nMinimumChainWork` (main: `…be1a`, testnet: `…87fc`) and `defaultAssumeValid` (old chain hash) are still from the X11 chain | **CRITICAL** | **FAIL** |
| C2 | main/testnet genesis uses `nBits = 0x207fffff` (consensus reset; already documented in `audit.md`) | HIGH (intentional change) | **INFORMATIONAL** |
| C3 | main/testnet historical checkpoints are still from the old chain | HIGH | **FAIL** |

Impact of C1/C3: nodes expecting the old chain may reject/ignore the PHR chain or store a valid assumption for a block that never existed. **Not changed** per instructions; must be fixed as part of an approved deployment decision.

**Overall status: PASS for hash/target, FAIL for stale consensus metadata.**

---

## 11. Mining Stress Test

**Method:** isolated nonce-scan mining (not a public network) with the regtest target `0x207fffff`; each block hash recomputed for determinism; two separate processes compared the chain tip; RSS/VmHWM monitored.
**Tools:** `phr_harness mine`.
**Sample:** 2 × 200 blocks = 834 hashes + 200 re-verifications.

| Metric | Run 1 | Run 2 | Status |
|---|---|---|---|
| Blocks accepted | 200/200 | 200/200 | **PASS** |
| Total attempts | 417 | 417 | **PASS** |
| Average attempts/block | 2.085 | 2.085 | **PASS** |
| Time | 2.65 s | 2.04 s | — |
| Final RSS / VmHWM | 5320 / 5320 KB | 5344 / 5344 KB | **PASS (stable)** |
| Chain tip | `2b14ce6a…16f` | `2b14ce6a…16f` | **PASS (identical across processes)** |

Evidence: `out/mine_run1.txt`, `out/mine_run2.txt`, sanitizer mine run (5 blocks, 12 attempts, clean).

**Limitation:** this is not `dashd -regtest generatetoaddress`. The full build failed during CMake configuration because system dependencies are unavailable:

```
CMake Error: Could NOT find SQLite3 (missing: SQLite3_LIBRARY SQLite3_INCLUDE_DIR)
```

In addition, there is no built `depends/` toolchain, no system Boost/libevent, and no `dashd` binary. Therefore, **inter-node synchronization, node restart, and full functional tests: INCONCLUSIVE**.

---

## 12. Identified Vulnerabilities

| ID | Finding | Risk | Evidence | Consensus impact |
|---|---|---|---|---|
| V1 | stale `nMinimumChainWork` / `defaultAssumeValid` / checkpoints | **CRITICAL** | `chainparams.cpp:242-243,453-454` | Changing them = a hard-fork/reset decision; must be documented & approved |
| V2 | No cryptographic justification for 32 rounds & the 3-hash selection construction | **HIGH** | No design document in the repo | No direct impact, but security risk is unmeasured |
| V3 | The memory-hard/ASIC-resistant claim is technically incorrect | **HIGH** | Dataset fill 3.46 ms vs 32 rounds ~0.15 ms; 32/16384 rows read | No consensus impact; impact on mining policy |
| V4 | Very low CPU throughput (~274 H/s/thread) | **MEDIUM** | Benchmark in table 6.1 | Mining/pool ecosystem must support it; initial difficulty needs review |
| V5 | `hash_phr.h` relies on `<array>` transitively | **LOW** | Include list lines 10–15 | No consensus impact; add explicit include |
| V6 | `-Wmisleading-indentation` at `hash_phr.h:126` | **INFORMATIONAL** | GCC -Wall output | Cosmetic |
| V7 | Audit tooling bugs (compact target, double precision, expected byte count) | **INFORMATIONAL** | Fixed before final results | No product impact |

No implementation bug causing incorrect hashes, overflow, UB, or consensus inconsistency was found on the tested paths.

---

## 13. Recommendations

1. **Do not deploy Mainnet** until V1–V3 are addressed.
2. Reset/`clear` `nMinimumChainWork`, `defaultAssumeValid`, and checkpoints for the PHR chain; do this as a separate consensus commit with documentation of the hard-fork impact (V1).
3. Publish the PHR-256-YP format specification: domain separation, byte order, round selection, and **the rationale for 32 rounds**; request an independent cryptographic review (V2).
4. Do not market the algorithm as "memory-hard/ASIC-resistant". If ASIC resistance is the goal, redesign with genuinely expensive random memory access, and measure hardware cost (V3).
5. Add an in-tree unit test (`src/test/phr_tests.cpp`) using vectors from `gen_vectors.py`, and register it in `CMakeLists.txt`; do not rely solely on the standalone harness.
6. Run a full build with the `depends` toolchain, then `ctest --test-dir build`, regtest functional tests, and `generatetoaddress` + node restart + two-node sync tests before deployment.
7. Add an explicit `#include <array>` in `src/hash_phr.h` and tidy the indentation of line 126 (V5/V6) — **after** the audit is complete and as a separate commit.
8. Run benchmarks on target hardware (server CPU, GPU, FPGA) with varying nonces; do not use these Ryzen 5 4600H numbers for network claims.

---

## 14. Mainnet Readiness Assessment

**Decision: NOT READY.**

| Criterion | Status |
|---|---|
| Correctness & determinism | PASS |
| Distribution/avalanche statistics | PASS |
| Hash & target consensus (4 networks) | PASS |
| Consensus metadata & old chain | **FAIL (CRITICAL)** |
| Cryptographic security evidence | **FAIL / absent** |
| Full node validation & synchronization | **INCONCLUSIVE** (build unavailable) |
| Mining / GPU / ASIC ecosystem | **INCONCLUSIVE / not ready** |
| Performance | Measured, very low compared to SHA-256 |

The algorithm **may be considered for further testing on regtest/devnet** after the consensus metadata is fixed. It **must not** be considered safe or Mainnet-ready merely because the vectors, statistics, and genesis match.

---

### Appendix: how to reproduce

See `test/phr_audit/README.md`. All raw output is in `test/phr_audit/out/`. All work changes only add `test/phr_audit/` and this report; no algorithm lines were changed.
