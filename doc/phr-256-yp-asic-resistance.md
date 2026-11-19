# PHR-256-YP — Dataset & ASIC Resistance Analysis

**Date:** 2026-11-19 (follow-up to the PHR-256-YP audit)
**Scope:** qualitative analysis based on the implementation in `src/hash_phr.h`
and measurements in `test/phr_audit/`. **No algorithm changes.**
This report complements `PHR_AUDIT_REPORT.md` and `doc/phr-256-yp.md`.

---

## 1. Brief conclusion

PHR-256-YP **cannot be considered memory-hard or ASIC-resistant merely because
its dataset is 1 MiB**. The current implementation is better categorized as:

> **compute-bound, sequential-ChaCha20-stream PoW with a 1 MiB working buffer
> and a small live memory footprint.**

Qualitative assessment:

| Platform | Assessment | Confidence |
|---|---|---|
| CPU | Relatively friendly | Measured |
| GPU | Unfriendly as-is, but not impossible to optimize | Qualitative |
| FPGA | Fairly friendly | Qualitative |
| ASIC | Friendly | Qualitative |

---

## 2. Dataset utilization

| Metric | Value | Source |
|---|---|---|
| Dataset size | 1,048,576 bytes | `dataset_bytes = 1024*1024` |
| 64-byte rows | 16,384 | `dataset_bytes / 64` |
| Rows read per hash | exactly 32 (one per round) | 32 rounds |
| Percentage of rows read | 32 / 16,384 ≈ **0.195%** | calculation |
| Percentage of rows written only | ≈ **99.805%** | calculation |
| Dataset fill per hash | 16,384 ChaCha20 blocks | `MakeDataset` |
| Socket / random index | 32 indices, 64 bytes each | `idx = LE64(state) % 16384` |

This means: almost the entire dataset cost is **sequential stream writing**,
not reading scattered data with high DRAM latency.

---

## 3. Sequential dependencies

Dataset generation is a chain:

```
key = SHA256("PHR-YP-DATASET\0" || header || pers)
block_0 = ChaCha20(key, counter=0)
key_1   = block_0[0..32]
block_1 = ChaCha20(key_1, counter=0)
key_2   = block_1[0..32]
...
```

Consequences:

1. **Cannot skip to row N.** To obtain row N, all blocks 0..N-1 must be
   computed because the next block's key comes from the previous block.
2. **No free parallelization within a single hash.** This chain is serial.
   Parallelism is only possible across hashes (different nonces).
3. **Average number of blocks that must still be computed:** with 32 uniform
   indices in [0, 16383], the expected maximum is ≈ 16,384 × 32/33 ≈ 15,890
   blocks, or **≈97% of the dataset**. So there is no significant shortcut from
   stopping generation early.
4. However, **storing the entire 1 MiB is not necessary**. The hardware only
   needs to store:
   - 32-byte chain state,
   - 32 selected rows (2,048 bytes),
   - 32-byte hash state and 101-byte message buffer.

Point (4) is the main reason why dataset size does not automatically yield ASIC
resistance.

---

## 4. Random access & working memory requirements

| Aspect | CPU implementation | Possible hardware optimization |
|---|---|---|
| Dataset access during generation | Sequential (64-byte memcpy) | Register/shift pipeline, no DRAM needed |
| Dataset access during mixing | 32 64-byte reads at state-determined indices | SIMD/ASIC can store 32 rows in SRAM |
| Live state | 32 bytes | Registers |
| Working buffer | 1 MiB per thread (`thread_local`) | 2–3 KB is sufficient if rows are stored selectively |
| Inter-block dependencies | Serial | ChaCha20 pipeline with 32-byte feedback |

---

## 5. Cache behavior

Measurements on Ryzen 5 4600H (2 vCPU, GCC 15.2.0 -O2):

| Measurement | Result |
|---|---|
| Full PHR hash, 1 thread, varying nonce | ~274 H/s (3.64 ms/hash) |
| Dataset fill only | ~289 fill/s (3.46 ms/fill) |
| Fraction of dataset fill time | ≈ **95%** of total hash time |
| SHA-256 primitive (80 B) | ~1.74 million H/s |
| SHA-256d primitive (80 B) | ~1.13 million H/s |
| SHA-256 / PHR ratio | ≈ 6,300× |

Interpretation:

- Hash time is dominated by 16,384 ChaCha20 compressions, not by the 32 mixing
  hashes (SHA-256/SHA3/BLAKE2s) and not by random DRAM reads.
- The 1 MiB per-thread buffer generally fits in the L2/L3 cache of modern CPUs,
  so there is no memory bandwidth pressure typical of memory-hard algorithms.
- 32 random reads per hash are too few to dominate latency.

---

## 6. Potential hardware optimization

### 6.1 GPU

Limiting factors:

- each thread needs its own ChaCha20 chain;
- 1 MiB per thread does not fit in shared memory → global memory or register
  streaming;
- 32 selector branches lead to three different hash kernels;
- serial inter-block dependencies reduce coalescing.

However, modern GPUs have thousands of cores; if the chain is kept in registers
and only 32 rows are retained, the kernel can be designed without a 1 MiB
buffer. Thus, GPU-unfriendly **at present**, not impossible.

### 6.2 FPGA

- The ChaCha20 quarter-round uses only addition, XOR, and fixed rotations — very
  well suited to FPGA logic.
- The 32-byte feedback chain is easily pipelined.
- The three hashes (SHA-256, Keccak, BLAKE2s) are complex, but can be
  implemented as separate IP blocks with a selector multiplexer.
- Small SRAM is sufficient; no 1 MiB DRAM is needed.

Assessment: **fairly friendly**.

### 6.3 ASIC

- No floating-point operations, no large data sizes, no unpredictable DRAM
  access.
- Area & power are dominated by tens of thousands of identical ChaCha20 moves —
  highly pipeline-able.
- The static cost for SHA-256/Keccak/BLAKE2s can be amortized.
- There is no evidence that the 1 MiB buffer gives CPUs an advantage over ASICs.

Assessment: **friendly** (qualitative, not an actual silicon cost measurement).

---

## 7. Why "1 MiB" is not proof of ASIC resistance

Credible ASIC resistance usually relies on one of the following:

1. high-latency random memory access that cannot be prefetched;
2. a large working set that does not fit in SRAM/registers;
3. dependence on CPU features that are expensive to emulate (e.g., branch
   predictor, cache hierarchy) — even this is often only temporary.

PHR-256-YP does not satisfy (1) strongly, does not satisfy (2), and provides no
CPU-specific advantage in (3). The 1 MiB dataset belongs to **each hash**, not
shared memory that can be optimized once and reused across nonces. Consequently:

- CPU miners pay the same ChaCha20 cost as specialized miners;
- specialized miners can instead save on storage and bandwidth costs;
- the CPU's relative advantage is small or even negative.

---

## 8. Recommendations (without changing the algorithm)

1. Do not use the phrases "memory-hard" or "ASIC-resistant" in public
   documentation, whitepapers, or marketing materials for this implementation.
2. If ASIC resistance is a design goal, undertake a separate redesign process:
   for example, repeated random DRAM access with a much larger working set, then
   measure the relative CPU/GPU/FPGA/ASIC costs. This **requires algorithm
   changes and a hard fork**, and is therefore outside this follow-up.
3. For the current purpose, position PHR-256-YP as a **stateless PoW with high
   per-nonce cost**, not as an ASIC defense.
4. Run target-hardware benchmarks (server CPU, GPU, FPGA) with varying nonces
   before claiming any platform advantage.

---

## 9. Status

| Item | Status |
|---|---|
| Dataset utilization analysis | COMPLETE |
| Sequential dependency analysis | COMPLETE |
| Random access & working memory analysis | COMPLETE |
| Cache & bottleneck analysis | COMPLETE (measured) |
| GPU/FPGA/ASIC assessment | COMPLETE (qualitative) |
| Algorithm changes | NOT Done |
| Security claims | NOT Made |
