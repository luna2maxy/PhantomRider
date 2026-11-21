# PHR-256-YP audit harness

Standalone, reproducible test/benchmark tooling for the live PHR-256-YP
implementation in `src/hash_phr.h`. It does **not** modify the algorithm,
genesis block, or consensus parameters.

## Build

```bash
./test/phr_audit/build.sh
```

## Modes

```bash
# 1. Independent vectors (pool reference implementation generates expected values)
python3 test/phr_audit/gen_vectors.py > test/phr_audit/out/vectors.tsv
test/phr_audit/out/phr_harness vectors test/phr_audit/out/vectors.tsv

# 2. Internal primitive cross-check against standard SHA-256 / SHA3-256 / BLAKE2s
test/phr_audit/out/phr_harness primitives

# 3. Distribution and avalanche sampling (JSON on stdout)
test/phr_audit/out/phr_harness dist 5000 1337 > test/phr_audit/out/dist.json
test/phr_audit/out/phr_harness avalanche 2500 2026 > test/phr_audit/out/avalanche.json
python3 test/phr_audit/analyze.py > test/phr_audit/out/analyze_stdout.json

# 4. Performance (nonce varies per hash to prevent constant folding)
test/phr_audit/out/phr_harness bench 600 1     # single thread
test/phr_audit/out/phr_harness bench 600 2     # two threads
test/phr_audit/out/phr_harness bench 600 4     # four threads (oversubscribed on 2 vCPU)
test/phr_audit/out/phr_harness bench_sha 200000 0   # SHA-256 primitive
test/phr_audit/out/phr_harness bench_sha 200000 1   # SHA-256d primitive
test/phr_audit/out/phr_harness bench_dataset 300    # dataset fill only

# 5. Memory / allocation accounting
test/phr_audit/out/phr_harness memory 50

# 6. Genesis + PoW target reconstruction (no node binary required)
test/phr_audit/out/phr_harness consensus

# 6b. Compact-target validation mirroring src/pow.cpp:CheckProofOfWork
test/phr_audit/out/phr_harness compact_target

# 7. Isolated chain mining stress test (nonce scan + determinism + memory)
test/phr_audit/out/phr_harness mine 200 0x207fffff

# 8. Full-node regtest validation (requires a built phantomriderd/phantomrider-cli)
python3 test/phr_audit/node_validation.py \
    --phantomriderd build/src/phantomriderd --phantomrider-cli build/src/phantomrider-cli

# 9. All of the above in sequence, saving logs under test/phr_audit/out/
test/phr_audit/run_full_validation.sh
```

## Sanitizers / optimisation matrix

```bash
g++ -std=c++20 -O1 -g -I src -fsanitize=address,undefined \
    test/phr_audit/phr_harness.cpp -o /tmp/phr_harness_san -pthread
/tmp/phr_harness_san vectors test/phr_audit/out/vectors.tsv
/tmp/phr_harness_san mine 5 0x207fffff

for opt in -O0 -O1 -O2 -O3 -Os "-O2 -march=native" "-O2 -fno-strict-aliasing"; do
    g++ -std=c++20 $opt -I src test/phr_audit/phr_harness.cpp -o /tmp/phr_harness_opt -pthread
    /tmp/phr_harness_opt vectors test/phr_audit/out/vectors.tsv
done
```

## In-tree unit test

The official Boost test lives at `src/test/phr_tests.cpp` and is registered in
`src/test/CMakeLists.txt` and `test/util/data/non-backported.txt`. It covers the
same vectors plus genesis hashes, 80-byte header serialization, endianness,
determinism and compact-target validation. The harness modes above reproduce
its assertions in a standalone binary for environments where the full Dash
test binary cannot be built.

## Evidence files

Generated during the 2026-11-19 audit run and kept under `out/`:

- `vectors.tsv`, `dist.json`, `avalanche.json`, `stats.json`, `stats.md`
- `chart_byte_freq.svg`, `chart_avalanche_hist.svg`, `chart_avalanche_bits.svg`
- `mine_run1.txt`, `mine_run2.txt`, `compact_target.txt`, `consensus.txt`
- `retarget_sim.txt` (output of `test/phr_audit/retarget_sim.cpp`, 5,000 blocks per scenario)
- `intree_test_syntax.txt` (standalone syntax check of `src/test/phr_tests.cpp`)
