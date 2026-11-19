# PHR Fresh-Chain Migration — Implementation Report

**Date:** 2026-11-19
**Repository:** `/home/ubuntu/Documents/GitHub/PhantomRider`
**Branch:** `develop`, HEAD `9b0a300bc1` (`x11-to-phr integration`)
**References:**
- `PHR_AUDIT_REPORT.md`
- `PHR_CONSENSUS_METADATA_REPORT.md`
- `docs/PHR_FRESH_CHAIN_BEFORE.md`
- `docs/PHR_FRESH_CHAIN_ACTIVATION_REVIEW.md`
- `docs/PHR_FRESH_CHAIN_DIFFICULTY_REVIEW.md`

**Readiness status:** **NOT MAINNET READY** — the consensus changes have been applied, but the final status depends on the full-node validation results listed in Section 7.

---

## 1. Purpose and scope

The PHR chain is a new network with PHR-256-YP genesis, not a continuation of the Dash X11 chain. The goal of this migration is to clean up consensus metadata that still refers to the old X11 chain so that the entire configuration is consistent with the PHR genesis.

**Not changed:**
- PHR-256-YP algorithm (`src/hash_phr.h`);
- genesis block and the four genesis hashes;
- 80-byte block header format;
- reward and subsidy/halving schedule;
- transaction system;
- Dash features (none were removed);
- seeds, ports, address prefixes, coin type, and LLMQ types.

**Changed:** only consensus metadata in `src/kernel/chainparams.cpp` and supporting documentation/tooling artifacts.

---

## 2. Metadata before migration

The full summary is in `docs/PHR_FRESH_CHAIN_BEFORE.md`. Key findings:

| Network | Parameter | Pre-change value |
|---|---|---|
| Mainnet | `nMinimumChainWork` | work of X11 block 2,563,000 |
| Mainnet | `defaultAssumeValid` | hash of X11 block 2,563,000 |
| Mainnet | `checkpointData` | 40 X11 checkpoints |
| Mainnet | `chainTxData` | 71,411,418 tx, 0.1307 tx/s |
| Mainnet | assumed size | 62 GB blockchain, 1 GB state |
| Testnet | `nMinimumChainWork` | work of X11 block 1,550,000 |
| Testnet | `defaultAssumeValid` | hash of X11 block 1,550,000 |
| Testnet | `checkpointData` | 20 X11 checkpoints |
| Testnet | `chainTxData` | 8,494,561 tx, 0.0139 tx/s |
| Testnet | assumed size | 10 GB blockchain, 1 GB state |
| Regtest | `m_assumeutxo_data` | UTXO snapshot at height 110/200 from the old chain |
| Main/Testnet | fork heights | X11 numbering (BIP34 951/76 … Withdrawals 2,201,472/1,148,500) |
| Main/Testnet | retarget | KGW/DGW 15,200/34,140 and 4,002/4,002 |
| Main/Testnet | `powLimit` | `0x00000fff…fff` (~2^236) |
| Mainnet | `nSuperblockStartHash` | hash of X11 block |

---

## 3. Metadata after migration

| Parameter | Mainnet | Testnet | Devnet | Regtest |
|---|---|---|---|---|
| `nMinimumChainWork` | `uint256{}` | `uint256{}` | `uint256{}` (unchanged) | `uint256{}` (unchanged) |
| `defaultAssumeValid` | `uint256{}` | `uint256{}` | `uint256{}` (unchanged) | `uint256{}` (unchanged) |
| `checkpointData` | PHR genesis `67215764…ad2fc` | PHR genesis `26aea3dd…acea3` | genesis + devnet genesis (unchanged) | PHR genesis (unchanged) |
| `chainTxData` | `{1390095618, 1, 0.0}` | `{1390666206, 1, 0.0}` | `devnetGenesis.nTime, 2, 0.01` (unchanged) | `{0,0,0}` (unchanged) |
| `m_assumed_blockchain_size` | 1 GB | 1 GB | 0 (unchanged) | 0 (unchanged) |
| `m_assumed_chain_state_size` | 0 GB | 0 GB | 0 (unchanged) | 0 (unchanged) |
| `m_assumeutxo_data` | empty (unchanged) | empty (unchanged) | absent (unchanged) | **empty** |
| `nSuperblockStartHash` | `uint256()` | `uint256()` (unchanged) | `uint256()` (unchanged) | `uint256()` (unchanged) |
| `BIP34Hash` | `uint256()` | `uint256()` | `uint256()` (unchanged) | `uint256()` (unchanged) |
| `DIP0003EnforcementHash` | `uint256()` | `uint256()` | `uint256()` (unchanged) | `uint256()` (unchanged) |
| BIP/DIP/V19/V20/MN_RR/Withdrawals heights | 1 | 1 | 1/2 (unchanged) | test-framework value (unchanged) |
| `MinBIP9WarningHeight` | 2017 | 2017 | 2018 (unchanged) | 0 (unchanged) |
| `powLimit` | `0x7fff…fff` | `0x7fff…fff` | `0x7fff…fff` (unchanged) | `0x7fff…fff` (unchanged) |
| `nPowKGWHeight` | 0 | 0 | 4001 (unchanged) | 15200 (unchanged, inert) |
| `nPowDGWHeight` | 0 | 0 | 4001 (unchanged) | 34140 (unchanged, inert) |
| `nPowTargetSpacing` | 150 s (unchanged) | 150 s (unchanged) | can be overridden | 150 s (unchanged) |
| `nPowTargetTimespan` | 86400 s (unchanged) | 86400 s (unchanged) | 86400 s (unchanged) | 86400 s (unchanged) |
| `fPowNoRetargeting` | false (unchanged) | false (unchanged) | false (unchanged) | true (unchanged) |

Verified unchanged genesis hashes:

| Network | Hash |
|---|---|
| Mainnet | `67215764b843ce5cbb30a6166c083fe4808019e844c3b57d4cc117d9186ad2fc` |
| Testnet | `26aea3dd1aad231fc1cf628b1ba2946bcdb709a814eff5d08f4538ed86bacea3` |
| Devnet | `403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077` |
| Regtest | `403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077` |

---

## 4. List of changed files

### 4.1 Consensus (separate commit)

| File | Change |
|---|---|
| `src/kernel/chainparams.cpp` | Reset main/testnet metadata, remove X11 checkpoints, reset fork activation, align `powLimit` with genesis, remove the regtest assume-UTXO snapshot, update genesis comments. Pure consensus diff: `docs/PHR_FRESH_CHAIN_CONSENSUS.patch`. |

### 4.2 Documentation

| File | Change |
|---|---|
| `docs/PHR_FRESH_CHAIN_BEFORE.md` | Backup of pre-migration values. |
| `docs/PHR_FRESH_CHAIN_ACTIVATION_REVIEW.md` | Audit and fork activation decisions. |
| `docs/PHR_FRESH_CHAIN_DIFFICULTY_REVIEW.md` | Difficulty audit + simulation evidence. |
| `docs/PHR_FRESH_CHAIN_MIGRATION.md` | This final report. |
| `PHR_CONSENSUS_METADATA_REPORT.md` | Status updated after implementation. |
| `PHR_REMEDIATION_REPORT.md` | Follow-up status updated. |
| `PHR_AUDIT_REPORT.md` | Baseline audit (not changed by the migration). |
| `doc/phr-256-yp.md` | PHR-256-YP technical specification. |
| `doc/phr-256-yp-asic-resistance.md` | Dataset/ASIC analysis. |

### 4.3 Tooling and tests

| File | Change |
|---|---|
| `src/test/phr_tests.cpp` | PHR unit tests in `test_dash`. |
| `src/test/CMakeLists.txt` | Test source registration. |
| `src/test/data/blockfilters.json` | Recomputed PHR block hashes and filter-header chain for the JSON vectors. |
| `src/test/util/setup_common.cpp` | Regtest fixture no longer asserts legacy checkpoint hashes; verifies the PHR genesis instead. |
| `src/test/evo_utils_tests.cpp` | Platform LLMQ expectation updated for DIP0020 active from height 1 on the fresh chain. |
| `test/functional/test_framework/phr_hash.py` | PHR PoW helper for the Python functional-test framework. |
| `test/functional/test_framework/messages.py` | Functional-test PoW helper switched from `dash_hash` to `phr_hash`. |
| `test/functional/test_framework/p2p.py` | `send_blocks_and_test()` now passes the caller timeout to `sync_with_ping()`; needed after slow PHR reorg processing. |
| `test/functional/feature_block.py` | Extended the slow 1088-block alternate-chain timeout for PHR mining. |
| `test/util/data/non-backported.txt` | Dash-specific marker. |
| `test/phr_audit/phr_harness.cpp` | Audit harness; `MainPowLimit` aligned with the new PHR `powLimit`. |
| `test/phr_audit/retarget_sim.cpp` | 5,000-block simulator using the production `GetNextWorkRequired()`. |
| `test/phr_audit/README.md` | Commands and evidence locations. |
| `test/phr_audit/node_validation.py` | Regtest validation: genesis, mining, blocks, restart, synchronization of 2 nodes. |
| `test/phr_audit/run_full_validation.sh` | Runs all validation and saves logs. |
| `test/phr_audit/out/*` | Evidence vectors, genesis, compact target, retarget, syntax check, and validation results. |

---

## 5. Parameters retained

| Parameter | Reason |
|---|---|
| `nSubsidyHalvingInterval`, masternode/budget/superblock schedule, `nCreditPoolPeriodBlocks` | Reward policy; prohibited from being changed by the scope. |
| `nPowTargetSpacing = 150` and `nPowTargetTimespan = 86400` | There is no technical reason to change the target spacing of the new chain. |
| `fPowAllowMinDifficultyBlocks` (main false, testnet true) | Testnet/mainnet behavior retained. |
| `fPowNoRetargeting` (regtest true) | Regtest intentionally does not retarget. |
| `pchMessageStart`, ports, seeds, address prefixes, coin type, LLMQ types | Network identity, not X11 leftovers. |
| Deployment `TESTDUMMY`, `V24`, `EVO_SHARES` | Time-based / EHF, not X11 height references. Not broadly activated. |
| `IPv4OptionalHeight` | Transaction policy, not an X11 chain leftover. |
| Regtest `nPowKGWHeight`/`nPowDGWHeight` | Unreachable because `fPowNoRetargeting = true`; changing them brings no technical benefit. |

## 6. Parameters changed

1. `nMinimumChainWork` and `defaultAssumeValid`: X11 value → `uint256{}`.
2. `checkpointData`: 40/20 X11 checkpoints → PHR genesis only.
3. `chainTxData`: X11 statistics → PHR genesis statistics.
4. `m_assumed_blockchain_size` / `m_assumed_chain_state_size`: 62/1 and 10/1 → 1/0.
5. `nSuperblockStartHash`, `BIP34Hash`, `DIP0003EnforcementHash`: X11 hashes → null.
6. All BIP/DIP/V19/V20/MN_RR/Withdrawals fork heights on main/testnet → 1; `MinBIP9WarningHeight` → 2017.
7. main/testnet `powLimit` → `0x7fff…fff` so that the genesis `nBits` `0x207fffff` is valid.
8. `nPowKGWHeight`/`nPowDGWHeight` on main/testnet → 0 (DGW active immediately).
9. Regtest `m_assumeutxo_data`: old snapshot → empty.

---

## 7. Test results

All validation was run on 2026-11-19 with one active validation stream at a time to respect the
6-core / 6 GB soft limit stated for this workspace.

### 7.1 Build

| Step | Command / setting | Result |
|---|---|---|
| Host tools | `pkg-config`, `gperf`, `bison`, `flex`, `lld` installed | PASS |
| Depends | `make -C depends -j2 HOST=x86_64-pc-linux-gnu NO_QT=1` | PASS |
| Configure | `cmake -B build --toolchain depends/x86_64-pc-linux-gnu/toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_HARDENING=OFF -DBUILD_GUI=OFF -DBUILD_TESTS=ON -DWITH_ZMQ=OFF -DWERROR=OFF` | PASS |
| Build | `cmake --build build` with low parallelism; `test_dash` relinked with `-fuse-ld=lld` because the default `ld` hung under memory pressure | PASS |
| Incremental rebuild | `cmake --build build --target test_bitcoin -j1` after removing temporary fixture debug output | PASS |
| Toolchain | CMake 4.2.3, GCC 15.2.0, LLD 21.1.8 | PASS |

Produced binaries:

- `build/src/dashd` — 370,963,992 bytes
- `build/src/dash-cli` — 15,690,088 bytes
- `build/src/test/test_dash` — 675,011,632 bytes after the `lld` relink

`CMAKE_BUILD_TYPE=Debug` was intentional: the genesis-hash `assert`s in
`src/kernel/chainparams.cpp` remain active.

### 7.2 Static checks

| Check | Result |
|---|---|
| `test/lint/lint-whitespace.py` | PASS |
| `test/lint/lint-circular-dependencies.py` | PASS |
| `test/lint/lint-includes.py` | PASS |
| `test/lint/lint-python.py` | SKIPPED — `flake8` is not installed and `python3 -m pip` is unavailable |

### 7.3 Standalone audit harness

- Independent vectors: **11/11 PASS** (`out/vectors.tsv`)
- Genesis/consensus: **4/4 networks PASS** plus `uint256` mapping (`out/consensus.txt`)
- Compact target: **5/5 cases + nonce scan PASS** (`out/compact_target.txt`)
- Difficulty simulation: fresh `powLimit` + immediate DGW stayed valid for 5,000 blocks;
  old `powLimit` failed at height 1 (`out/retarget_sim.txt`)

### 7.4 In-tree PHR unit suite

`./build/src/test/test_dash --run_test=phr_tests --log_level=test_suite`
→ **8/8 cases PASS**, `*** No errors detected` (`out/unit_phr.log`).

### 7.5 Full `test_dash` suite

The first full run exited 134 with 460 reported failures. The first real failure was
`evo_utils_tests/utils_IsQuorumTypeEnabled_tests_mainnet`: the old test expected the Platform
LLMQ to be enabled only on test chains, but the fresh chain activates DIP0020 from height 1 on
mainnet too. That assertion aborted the process and caused a cascade of `ArgsManager::AddArg`
duplicate-registration aborts in later fixtures.

Updated `src/test/evo_utils_tests.cpp` to expect Platform LLMQ active on both regtest and the
fresh mainnet. Targeted suite now passes (`out/evo_utils.log`); full suite rerun in progress
(`out/unit_all.log` / `out/unit_all.exit`).

### 7.6 Regtest node validation

`python3 test/phr_audit/node_validation.py --dashd build/src/dashd --dash-cli build/src/dash-cli --blocks 12`
→ **PASS** (`out/node_validation.log`):

- genesis hash `403ac5ae…e51077`;
- 12 blocks mined with `bits=207fffff`;
- all block heights validated;
- restart preserved the tip;
- two-node synchronization reached the same tip;
- `node_validation_failures=0`.

### 7.7 Functional tests

`build/test/functional/test_runner.py -j1 mining_basic.py feature_block.py`

- `mining_basic.py`: **PASS** (21 internal checks, 138 s)
- `feature_block.py`: first run **FAIL** after 2,187 s. The 1088-block alternate chain was
  accepted block-by-block, but `send_blocks_and_test()` used the default 60 s
  `sync_with_ping()` timeout while the node was still validating the force-sent chain.
  Adapted `test/functional/test_framework/p2p.py` to pass the caller's timeout to
  `sync_with_ping()`, then reran the test alone: **PASS** in 2,796 s
  (`out/functional_feature_block.log`).

Overall functional status: **PASS** (both requested tests pass with `-j1`).
Evidence: `out/functional_basic.log`, `out/feature_block_combined.log`,
`out/functional_feature_block.log`.

### 7.8 Branch basis decision

The repository was kept on `develop` (HEAD `9b0a300bc1`, one PHR integration commit on top of
a 2026-11-18 Dash `develop` snapshot). `origin/master` / `upstream/master` points to v24.0.1
and is 1,296 commits behind this branch; tag v24.0.2 is also divergent (10 commits ahead of
our base, 1,297 behind it). Rebasing onto either would discard a large amount of newer Dash
work and was therefore rejected.

A fresh `git fetch upstream develop` showed that upstream `develop` has since advanced by
75 commits (2026-11-18). They are mainly CMake/GUIX backports, Qt/wallet fixes, an LLMQ DKG
guard, and a BLS-scheme refactor. None is required by PHR-256-YP, and adopting them would
require a full rebuild and revalidation. The observed test failures were traced to stale test
expectations/vectors and timeout assumptions, not to an unstable Dash base.

---

## 8. Consensus impact

- This is a **hard reset**. Nodes with old binaries cannot follow the correct PHR chain; all participants must resync from the PHR genesis.
- `nMinimumChainWork = 0` removes an artificial work gate that is impossible to reach.
- Genesis-only checkpoints prevent PHR blocks from being rejected because of X11 hashes.
- Fork activation from height 1 removes X11 numbering and preserves the dependencies (`V20 >= DIP0003`, `MN_RR >= V20`, `V20 >= V19`).
- The new `powLimit` is required for the first block with `nBits = 0x207fffff` to pass `CheckProofOfWork()`. Without this change, the first block is rejected (see `docs/PHR_FRESH_CHAIN_DIFFICULTY_REVIEW.md`).
- DGW active from genesis replaces the legacy X11 BTC→KGW→DGW schedule.

---

## 9. Remaining risks

| Risk | Level | Notes |
|---|---|---|
| Full-node validation not complete | HIGH / INCONCLUSIVE | See Section 7. |
| Very low initial difficulty | MEDIUM | Consequence of genesis `nBits`; DGW needs to be monitored on the live chain. |
| All Dash features active at height 1 | MEDIUM | No transition period; LLMQ/Platform depend on real masternodes. |
| Cryptographic security claims not proven | HIGH | 32 rounds have no independent justification; see the PHR doc. |
| ASIC resistance not proven | HIGH | Analysis indicates compute-bound sequential-ChaCha. |
| Bootstrapping / seed list | MEDIUM | Old seeds still target the historical Dash network and need operator review. |

---

## 10. Final recommendations

1. Complete the build and all testing in Section 7 before release.
2. Obtain an independent cryptographic review of the 32-round construction.
3. Remove memory-hard/ASIC-resistant claims from public materials.
4. Review the seed list and coordinate resync for network participants.
5. Do not call the network "Mainnet ready" until Section 7 returns PASS with no failure notes.

---

## 11. Conclusion

The Fresh-Chain Reset has been applied to the consensus metadata without changing the algorithm, genesis, header, reward, or transaction system. All active X11 hash/checkpoint references have been removed from `src/kernel/chainparams.cpp`. Mainnet status remains **NOT READY** until full-node validation is complete and all HIGH findings have been addressed.
