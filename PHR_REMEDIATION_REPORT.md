# PHR-256-YP — Post-Audit Remediation Report

**Date:** 2026-11-19
**Repository:** `/home/ubuntu/Documents/GitHub/PhantomRider`
**Primary reference:** `PHR_AUDIT_REPORT.md` (audit 2026-11-19)
**Final Mainnet status:** **NOT READY / INCONCLUSIVE** — the fresh-chain reset has been applied, but full node validation is still running at the time of this report. See Section 5 and `docs/PHR_FRESH_CHAIN_MIGRATION.md`.

---

## 1. Initial findings (from the 2026-11-19 audit)

| ID | Finding | Risk | Initial audit status |
|---|---|---|---|
| V1 | `nMinimumChainWork`, `defaultAssumeValid`, checkpoints, and transaction statistics still refer to the old X11 chain | CRITICAL | FAIL |
| V2 | No cryptographic justification/design document for the 32-round construction | HIGH | Unproven |
| V3 | The memory-hard/ASIC-resistant claim cannot be justified | HIGH | Unproven |
| V4 | Low CPU throughput (~274 H/s/thread) | MEDIUM | Measured |
| V5 | `hash_phr.h` relies on `<array>` transitively | LOW | WARN |
| V6 | `-Wmisleading-indentation` at `hash_phr.h:126` | INFORMATIONAL | WARN |
| V7 | Audit tooling bugs (fixed before final results) | INFORMATIONAL | INFORMATIONAL |

---

## 2. Actions performed

| Stage | Action | Status |
|---|---|---|
| 1 | Comprehensive consensus metadata audit; impact report & migration plan written without changing values | **COMPLETED** |
| 2 | Technical documentation of the algorithm based on the actual implementation, separating proven vs unproven properties | **COMPLETED** |
| 3 | Dataset analysis & separate ASIC resistance report | **COMPLETED** |
| 4 | Official Dash Core unit tests (`src/test/phr_tests.cpp`) + build registration | **COMPLETED — `test_dash --run_test=phr_tests` passed 8/8** |
| 5 | Consensus fresh-chain reset (metadata, forks, difficulty, assume-UTXO) | **COMPLETED** in `src/kernel/chainparams.cpp` |
| 6 | `depends` build preparation, full build, unit tests, functional tests, regtest mining, node restart & synchronization | **IN PROGRESS / see Section 5** |
| 7 | This remediation report | **COMPLETED** |

### 2.1 Stage 1 — Consensus metadata audit

Document: `PHR_CONSENSUS_METADATA_REPORT.md`.

Main findings:
- Main: `nMinimumChainWork` and `defaultAssumeValid` in `src/kernel/chainparams.cpp:242-243` from X11 block 2,563,000.
- Testnet: the same at `:453-454` from block 1,550,000.
- Main/testnet: 40 + 20 X11 checkpoints at `:318-361` and `:523-546`.
- Main/testnet: `ChainTxData` and assumed chain sizes from the old chain.
- Regtest: old assume-UTXO snapshot at `:982-991`.
- Fork activation heights and the KGW/DGW schedule still use old chain numbering.
- The PHR genesis itself is **correct** on all four networks.

The migration plan (Option A — fresh-chain reset) was written, but **not executed** because it required consensus owner approval.

### 2.2 Stage 2 — Technical documentation

Document: `doc/phr-256-yp.md`.

Covers: fixed parameters, primitives (SHA-256, SHA3-256, BLAKE2s, ChaCha20), tagged input/domain separation, dataset generation, initialization, 32 mixing rounds, finalization, endianness, digest→`uint256` mapping, and an explicit list of **verified** vs **unproven** properties. No new security claims.

### 2.3 Stage 3 — Dataset & ASIC resistance

Document: `doc/phr-256-yp-asic-resistance.md`.

Conclusion: only 32/16,384 dataset rows are read (≈0.195%); the dataset is generated sequentially per hash; hardware working memory can be ~2–3 KB; ~95% of hash time is ChaCha20 filling. Therefore this design is **compute-bound sequential-ChaCha**, not memory-hard, and offers no provable ASIC resistance advantage.

### 2.4 Stage 4 — Official unit tests

New file: `src/test/phr_tests.cpp` (`phr_tests` suite, 8 test cases):

| Test case | Coverage |
|---|---|
| `reference_vectors` | 11 independent vectors (3 official pool + 8 derived), including empty input and non-standard round/dataset parameters |
| `internal_primitives` | SHA-256 / SHA3-256 / BLAKE2s vs `hashlib` vectors |
| `genesis_hash` | Main, testnet, regtest, devnet genesis hashes + `hashGenesisBlock` + `CheckProofOfWork` |
| `header_serialization` | 80-byte serialization exactly matching `CBlockHeader::GetHash()` |
| `endianness_and_output_mapping` | big-endian digest == `uint256::GetHex()`; empty input |
| `determinism` | Identical hashes for the same input, changed for nonce+1 |
| `compact_target_validation` | Valid target, too-difficult target, negative/zero compact, regtest nonce scan |
| `fresh_chain_consensus_metadata` | Asserts the reset metadata: zero minimum work/assume-valid, genesis checkpoints, fork height 1, DGW from genesis, `powLimit` matching genesis, empty regtest assume-UTXO |

Build registration: `src/test/CMakeLists.txt` and `test/util/data/non-backported.txt`.

The suite was compiled and run in the full `test_dash` binary: 8/8 cases PASS, `*** No errors detected` (`test/phr_audit/out/unit_phr.log`). The original syntax-only check and the equivalent standalone harness evidence are retained in `test/phr_audit/out/intree_test_syntax.txt`, `out/consensus.txt`, and `out/compact_target.txt`.

---

## 3. Changed files

| File | Category | Change |
|---|---|---|
| `src/kernel/chainparams.cpp` | Consensus | New — fresh-chain metadata reset; see `docs/PHR_FRESH_CHAIN_CONSENSUS.patch` |
| `PHR_CONSENSUS_METADATA_REPORT.md` | Documentation | New — metadata audit & migration plan |
| `PHR_REMEDIATION_REPORT.md` | Documentation | New — this report |
| `doc/phr-256-yp.md` | Documentation | New — technical specification |
| `doc/phr-256-yp-asic-resistance.md` | Documentation | New — dataset & ASIC analysis |
| `docs/PHR_FRESH_CHAIN_*.md` | Documentation | Before/activation/difficulty/migration reports |
| `src/test/phr_tests.cpp` | Tooling/test | New — official unit tests |
| `src/test/CMakeLists.txt` | Tooling/build | +1 test registration line |
| `src/test/data/blockfilters.json` | Tooling/test | Recomputed PHR hashes and filter-header chain |
| `src/test/util/setup_common.cpp` | Tooling/test | Regtest fixture now checks the PHR genesis instead of stale checkpoint hashes |
| `src/test/evo_utils_tests.cpp` | Tooling/test | Platform LLMQ expectation updated for fresh-chain DIP0020 activation |
| `test/functional/test_framework/phr_hash.py` | Tooling/test | PHR PoW helper for functional tests |
| `test/functional/test_framework/messages.py` | Tooling/test | Uses `phr_hash` instead of `dash_hash` |
| `test/functional/test_framework/p2p.py` | Tooling/test | Propagates caller timeout to `sync_with_ping()` after slow PHR reorgs |
| `test/functional/feature_block.py` | Tooling/test | Extended slow reorg timeout for PHR mining |
| `test/util/data/non-backported.txt` | Tooling/build | +1 Dash-specific marker line |
| `test/phr_audit/*` | Tooling | Harness, analyzer, README, evidence output, node validator, full-validation runner |

**Not changed:** `src/hash_phr.h`, `src/primitives/block.*`, `src/pow.cpp`, genesis, headers, rewards, transaction system, or any Dash consensus rules beyond the fresh-chain metadata reset.

---

## 4. Test results

### 4.1 Audit harness (can be run locally)

| Test | Result | Status |
|---|---|---|
| Independent vectors | 11/11 PASS | PASS |
| Internal primitives | 3/3 match `hashlib` | PASS |
| 4-network genesis + target | 4/4 hashes match asserts; PoW valid | PASS |
| `uint256` mapping | big-endian digest == `GetHex()` | PASS |
| Compact target (mirror `CheckProofOfWork`) | 5/5 cases + nonce scan PASS | PASS |
| Isolated mining | 2×200 blocks, identical tip across processes | PASS |
| ASan/UBSan sanitizers | Clean | PASS |
| Optimization matrix -O0…-O3/-march=native | 7/7 identical | PASS |
| Statistics & avalanche | All PASS at α=0.01 | PASS |

### 4.2 Official Dash Core unit tests

`./build/src/test/test_dash --run_test=phr_tests --log_level=test_suite` ran 8/8 cases with
`*** No errors detected` (`test/phr_audit/out/unit_phr.log`). Full-suite and functional results
are recorded in Section 5.2.

---

## 5. Full node validation

### 5.1 Fresh-chain reset (already applied)

The consensus implementation is in `src/kernel/chainparams.cpp`. Full report:
`docs/PHR_FRESH_CHAIN_MIGRATION.md`. Supporting audits:
`docs/PHR_FRESH_CHAIN_ACTIVATION_REVIEW.md` and
`docs/PHR_FRESH_CHAIN_DIFFICULTY_REVIEW.md`.

- Mainnet/testnet: `nMinimumChainWork` and `defaultAssumeValid` = `uint256{}`;
  X11 checkpoints replaced with PHR genesis checkpoints; `chainTxData` and assumed
  sizes reset; X11 hashes (`nSuperblockStartHash`, `BIP34Hash`,
  `DIP0003EnforcementHash`) zeroed; BIP/DIP/V19/V20/MN_RR/Withdrawals fork heights
  = 1 with dependencies preserved.
- Mainnet/testnet: `powLimit = 0x7fff…fff` so that genesis `nBits` `0x207fffff`
  passes `CheckProofOfWork()`; `nPowKGWHeight = nPowDGWHeight = 0` (DGW immediate).
- Regtest: the old `m_assumeutxo_data` snapshot was cleared. Devnet and regtest
  were not otherwise changed.
- All four genesis hashes were verified unchanged by the audit harness
  (`test/phr_audit/out/consensus.txt`).

### 5.2 Build and runtime

*(To be filled from the depends/toolchain build results, `test_dash`, functional tests, and
`test/phr_audit/node_validation.py` — see status in Section 2.)*

---

## 6. Remaining risks

| Risk | Level | Reason |
|---|---|---|
| X11 consensus metadata not yet fixed | ~~CRITICAL~~ RESOLVED | Fresh-chain reset applied in `src/kernel/chainparams.cpp` |
| No independent cryptographic security evidence | HIGH | 32 rounds have no public justification |
| ASIC resistance unproven | HIGH | Analysis shows the opposite |
| Full node / synchronization not yet validated | HIGH/INCONCLUSIVE | Depends on the results of Section 5.2 |
| Official unit tests have never been run in CI | MEDIUM | Need a build with Boost.Test |
| Transitive `<array>` include & indentation | LOW/INFORMATIONAL | Cosmetic/portability |

---

## 7. Next recommendations

1. Keep the consensus changes as a commit separate from documentation/tooling.
2. Run `test_dash --run_test=phr_tests` in an environment with the full toolchain.
3. Complete Section 5.2 (full build, functional tests, regtest mining, restart, multi-node sync).
4. Publish the specification + request an independent cryptographic review before mentioning Mainnet.
5. Remove memory-hard/ASIC-resistant claims from public materials.
6. Add explicit `<array>` and tidy indentation as a separate cosmetic commit.

---

## 8. Conclusion

The metadata audit, documentation, ASIC analysis, official unit tests, and consensus fresh-chain reset have been performed. The algorithm, genesis, headers, rewards, and transaction system were not changed. However, it is **NOT READY for Mainnet** until full node validation (Section 5.2) is completed and the HIGH findings are addressed.
