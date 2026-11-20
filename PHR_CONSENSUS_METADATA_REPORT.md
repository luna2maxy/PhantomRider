# PHR-256-YP — Consensus Metadata Audit & Migration Plan

**Date:** 2026-11-19 (follow-up to the PHR-256-YP audit)
**Repository:** `/home/ubuntu/Documents/GitHub/PhantomRider`
**Change status:** This report was written before implementation. Option A was subsequently implemented on 2026-11-19 in `src/kernel/chainparams.cpp` after written approval from the consensus owner. Final details are in `docs/PHR_FRESH_CHAIN_MIGRATION.md`.

---

## 1. Executive summary

The PHR-256-YP integration replaced the PoW hash function and **genesis block** from X11 to PHR (`src/kernel/chainparams.cpp:262-265`, `:468-471`, `:653-656`, `:953-955`). However, other consensus metadata is still written for the **historical X11 chain**. As a result:

- new nodes have no realistic work base (`nMinimumChainWork`) for the PHR chain;
- `defaultAssumeValid` and all checkpoints refer to blocks that will never exist on the PHR chain;
- `ChainTxData` contains transaction statistics from the old chain;
- fork activation heights (BIP/DIP/V19/V20/etc.) and retarget transition heights (`nPowKGWHeight`, `nPowDGWHeight`) still use old chain numbering;
- regtest assume-UTXO data also comes from the old chain.

This is not a bug in the PHR algorithm, but a **consensus metadata mismatch**. For Mainnet this is **CRITICAL**; for regtest/devnet **HIGH** (historical assessment at audit time). **Status update:** Option A (fresh-chain reset) was implemented in `src/kernel/chainparams.cpp`; the CRITICAL/HIGH/MEDIUM metadata items identified below are marked **PASSED** where resolved in code, or remain open/not applicable as noted.

---

## 2. Detailed findings

### 2.1 `nMinimumChainWork` and `defaultAssumeValid`

| Network | Lines | `nMinimumChainWork` | `defaultAssumeValid` | Problem |
|---|---|---|---|---|
| Main | `chainparams.cpp:242-243` | `0x…c15981a81af44a6abe1a` (comment: block 2,563,000) | `0x000000000000000346a1804…e98f` (block 2,563,000) | Both from the X11 chain; the assume-valid block will not exist. |
| Testnet | `chainparams.cpp:453-454` | `0x…03ed573a939487fc` (block 1,550,000) | `0x000000116cd3c11d44c88…318c` (block 1,550,000) | Same. |
| Devnet | `chainparams.cpp:637-638` | `uint256{}` | `uint256{}` | Already clean. |
| Regtest | `chainparams.cpp:933-934` | `uint256{}` | `uint256{}` | Already clean. |

Impact: `nMinimumChainWork` is too large for a chain newly started from genesis with the easy target `0x207fffff`; nodes may reject/ignore a valid tip because cumulative work never reaches the old value in a reasonable time. `defaultAssumeValid` makes nodes assume the validity of a block that never existed.

### 2.2 Checkpoint

| Network | Lines | Contents | Problem |
|---|---|---|---|
| Main | `chainparams.cpp:318-361` | 40 checkpoints, heights 1,500 … 2,563,000 | All hashes come from the X11 chain. None match the PHR chain. |
| Testnet | `chainparams.cpp:523-546` | 20 checkpoints, heights 255 … 1,550,000 | Same. |
| Devnet | `chainparams.cpp:717-722` | `{0, PHR genesis}`, `{1, devnetGenesis.GetHash()}` | **Correct** (already PHR). |
| Regtest | `chainparams.cpp:976-980` | `{0, PHR genesis}` | **Correct** (already PHR). |

Impact: X11 checkpoints can cause PHR nodes to reject blocks at those heights (`CheckBlock` uses checkpoints to force specific hashes), or send false signals to peers.

### 2.3 Chain transaction history assumptions

| Network | Lines | Value | Problem |
|---|---|---|---|
| Main | `chainparams.cpp:367-373` | timestamp `1793218693`, 71,411,418 tx, 0.1307 tx/s (block 2,563,000) | Old chain statistics. |
| Testnet | `chainparams.cpp:552-558` | timestamp `1788899589`, 8,494,561 tx, 0.0139 tx/s (block 1,550,000) | Old chain statistics. |
| Devnet | `chainparams.cpp:724-728` | devnet genesis time, 2 tx, 0.01 tx/s | Correct. |
| Regtest | `chainparams.cpp:993-997` | `0,0,0` | Correct. |

Impact: `getchaintxstats` and chain size estimates (`m_assumed_blockchain_size = 62`, `m_assumed_chain_state_size = 1` at `chainparams.cpp:259-260`) are misleading for the PHR chain.

### 2.4 Assume-UTXO

| Network | Lines | Contents | Problem |
|---|---|---|---|
| Regtest | `chainparams.cpp:982-991` | snapshots at heights 110 and 200 with old hashes | Snapshots were produced from the old X11 regtest chain; invalid on the PHR chain. |
| Main/Testnet | `chainparams.cpp:363-365`, `:548-550` | `// TODO to be specified in a future patch.` | No data, safe. |

### 2.5 Genesis configuration

| Network | New genesis (PHR) | Status |
|---|---|---|
| Main | `67215764…ad2fc` (`chainparams.cpp:264`) | Correct & verified by the audit harness. |
| Testnet | `26aea3dd…acea3` (`:470`) | Correct. |
| Devnet base | `403ac5ae…1077` (`:655`) | Correct. |
| Regtest | `403ac5ae…1077` (`:954`) | Correct. |

Cosmetic note: the genesis documentation comment at `chainparams.cpp:84` still states `hash=00000ffd590b14…`, `nBits=1e0ffff0`, `nNonce=28917698` (old X11 values). This is a **comment only**, not an active value, but it misleads reviewers.

### 2.6 Chain activation parameters (fork level)

With the new genesis, all of the following activation heights still refer to X11 chain numbering:

| Network | Lines | Example | Problem |
|---|---|---|---|
| Main | `chainparams.cpp:189-209` | `BIP34Height=951`, `DIP0003Height=1028160`, `V20Height=1987776`, `WithdrawalsHeight=2201472`, `MinBIP9WarningHeight=2201472+2016` | Forks will activate at heights irrelevant to the PHR chain; e.g. BIP34 activates ~951 blocks after the new genesis. |
| Main | `chainparams.cpp:215-216` | `nPowKGWHeight=15200`, `nPowDGWHeight=34140` | Retargeting stays BTC until 15,200, then KGW, then DGW — a legacy schedule from the old chain. |
| Testnet | `chainparams.cpp:400-420`, `:426-427` | `BIP34Height=76`, `DIP0003Height=7000`, `nPowKGWHeight=nPowDGWHeight=4002` | Same. |
| Devnet | `chainparams.cpp:585-611` | activation at heights 1/2, `nPowKGWHeight=4001` | Mostly active immediately; KGW/DGW inactive (`>=` means "no KGW"). Relatively consistent for devnet. |
| Regtest | `chainparams.cpp:878-906` | `DIP0003Height=432`, `WithdrawalsHeight=600`, `nPowKGWHeight=15200`, `nPowDGWHeight=34140` | Legacy heights for the test framework; need review if regtest is used for long-term PHR validation. |

**Time-based** parameters (`vDeployments[...].nStartTime/nTimeout`, e.g. `chainparams.cpp:225-240`) are not directly tied to the PoW hash, but still need review because the new chain's timeline differs from the old chain.

---

## 3. Complete list of old X11 chain references

| # | Location | Reference | Category | Risk / Status |
|---|---|---|---|---|
| 1 | `chainparams.cpp:84` | comment `hash=00000ffd…`, `nBits=1e0ffff0`, `nNonce=28917698` | Documentation | LOW |
| 2 | `chainparams.cpp:242` | `nMinimumChainWork` main | Work assumption | CRITICAL — **PASSED** (cleared to `uint256{}`) |
| 3 | `chainparams.cpp:243` | `defaultAssumeValid` main | Assume-valid | CRITICAL — **PASSED** (cleared to `uint256{}`) |
| 4 | `chainparams.cpp:318-361` | 40 checkpoints main | Checkpoint | HIGH — **PASSED** (replaced with PHR genesis checkpoint) |
| 5 | `chainparams.cpp:367-373` | `chainTxData` main | History | MEDIUM — **PASSED** (reset to fresh-chain genesis stats) |
| 6 | `chainparams.cpp:453` | `nMinimumChainWork` testnet | Work assumption | HIGH — **PASSED** (cleared to `uint256{}`) |
| 7 | `chainparams.cpp:454` | `defaultAssumeValid` testnet | Assume-valid | HIGH — **PASSED** (cleared to `uint256{}`) |
| 8 | `chainparams.cpp:523-546` | 20 checkpoints testnet | Checkpoint | HIGH — **PASSED** (replaced with PHR genesis checkpoint) |
| 9 | `chainparams.cpp:552-558` | `chainTxData` testnet | History | MEDIUM — **PASSED** (reset to fresh-chain genesis stats) |
| 10 | `chainparams.cpp:189-216` | Main fork & retarget heights | Activation | HIGH — **PASSED** (BIP/DIP/V19/V20/MN_RR/Withdrawals set to height 1; `nPowKGWHeight=0`, `nPowDGWHeight=0`, DGW immediate; see migration docs) |
| 11 | `chainparams.cpp:400-427` | Testnet fork & retarget heights | Activation | HIGH — **PASSED** (fresh-chain activation/retarget schedule applied) |
| 12 | `chainparams.cpp:878-906` | Regtest fork & retarget heights | Activation | MEDIUM — **NOT PASSED / retained by design** (regtest keeps legacy/test-framework activation and retarget parameters in places for compatibility; review if long-term PHR regtest validation requires change) |
| 13 | `chainparams.cpp:982-991` | regtest assume-UTXO | Snapshot | MEDIUM — **PASSED** (old snapshot cleared; no PHR regtest snapshot available) |
| 14 | `audit.md:85-88` | Old/new genesis comparison table | Historical documentation | INFORMATIONAL (intentional) |

Old genesis references in `contrib/linearize/*` and functional tests were **already updated** to PHR hashes in the previous integration (see `audit.md:103-109`); no old hash remnants were found there via searches for `00000ffd…`, `00000baf…`, `000008ca…`.

---

## 4. Impact analysis

1. **Node consensus:** incorrect checkpoints and assume-valid can cause PHR nodes to reject valid blocks or accept false assumptions. An unattainable `nMinimumChainWork` can prevent the "chain active" status during headers-first synchronization.
2. **Economics & mining:** the retarget schedule (BTC→KGW→DGW) was designed for the old chain. With `powLimit = 0x00000fff…` and genesis `nBits = 0x207fffff`, the PHR chain's difficulty dynamics cannot be equated with those of the X11 chain.
3. **Compatibility:** changing this metadata is a hard reset decision. Since the genesis has already been replaced, the PHR chain is not a continuation of the X11 chain — this is a new network, not a soft fork.
4. **Testing:** functional tests expecting specific activation heights (e.g. DIP3 at regtest height 432) may still pass because the framework overrides heights, but long-term validation needs review.

---

## 5. Migration plan (requires approval)

> **Historical note:** this section was written before approval. The actual implementation subsequently followed Option A with adjustments documented in
> `docs/PHR_FRESH_CHAIN_ACTIVATION_REVIEW.md`, `docs/PHR_FRESH_CHAIN_DIFFICULTY_REVIEW.md`,
> and `docs/PHR_FRESH_CHAIN_MIGRATION.md`.

### Option A — Fresh-chain reset (recommended, consistent with the new genesis)

1. Main/Testnet:
   - `nMinimumChainWork = uint256{}` (or genesis work resulting from `0x207fffff`, ~2 units; choose one and document it).
   - `defaultAssumeValid = uint256{}`.
   - `checkpointData`: empty it, or only `{0, hashGenesisBlock}`.
   - `chainTxData = { genesis.nTime, 1, 0.0 }` (1 genesis coinbase transaction).
   - `m_assumed_blockchain_size`, `m_assumed_chain_state_size`: small initial values, not 62 GB/1 GB.
2. All networks:
   - Review fork activation heights; for a fresh chain, activate from 0/1 or reschedule according to the new policy.
   - Set `nPowKGWHeight = 0`, `nPowDGWHeight = 0` (DGW active immediately) **or** write a new retarget policy; do not leave the old BTC/KGW schedule.
   - Remove the old regtest assume-UTXO snapshot.
3. Update the genesis comment at `chainparams.cpp:84`.
4. Apply as a **separate consensus commit**, with a commit message explaining that this is a network reset, not a backport.

### Option B — Hard fork on the old chain

Cannot be applied directly because the genesis and PoW function have already been replaced. Keeping the old metadata while replacing PoW does not produce a compatible chain. This option only makes sense if the decision is reversed and X11 is restored — outside the scope of this audit.

### Safe execution order

1. Approve Option A in writing (consensus owner).
2. Apply the metadata changes in a separate commit.
3. Update `audit.md` and this report with the new hashes/work.
4. Run a full build + regtest + functional tests (Stage 5) **before** naming the network "mainnet".
5. Publish the seed node list, snapshots, and resync instructions for network participants.

---

## 6. Current status

| Item | Status |
|---|---|
| Metadata audit | **COMPLETED** |
| Impact report & migration plan | **COMPLETED (this document)** |
| Consensus owner approval | **OBTAINED** (implementation instructions 2026-11-19) |
| Consensus value changes | **ALREADY PERFORMED** in `src/kernel/chainparams.cpp` — identified CRITICAL/HIGH/MEDIUM stale-metadata findings marked **PASSED** in Section 3 where resolved (item 12 retained by design) |
| Final migration report | **SEE** `docs/PHR_FRESH_CHAIN_MIGRATION.md` |
| Mainnet readiness | **NOT READY** until full node validation is complete and remaining open HIGH findings are addressed |

---

## 7. Note on environment limitations

This report did not run a full node; the `dashd` build failed because system/depends dependencies were unavailable (details in the final remediation report). The findings above were obtained from static inspection of `src/kernel/chainparams.cpp` and cross-repository reference searches, as well as genesis hash verification by the audit harness dated 2026-11-19.
