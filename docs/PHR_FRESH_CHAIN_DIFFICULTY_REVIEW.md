# PHR Fresh-Chain Reset — Difficulty Adjustment Review

**Date:** 2026-11-19
**Status at the time of writing:** **no consensus values have been changed yet**
**Simulation evidence:** `test/phr_audit/out/retarget_sim.txt`
**Simulator:** `test/phr_audit/retarget_sim.cpp` (calls the production `GetNextWorkRequired()` from `src/pow.cpp`)

## 1. Parameters reviewed

| Parameter | Main before | Testnet before | Fresh-chain decision |
|---|---:|---:|---|
| `nPowTargetSpacing` | 150 seconds | 150 seconds | **Preserved** |
| `nPowTargetTimespan` | 86400 seconds | 86400 seconds | **Preserved** |
| `nPowKGWHeight` | 15200 | 4002 | **0** |
| `nPowDGWHeight` | 34140 | 4002 | **0** |
| `powLimit` | `0x00000fff…fff` (~2^236) | `0x00000fff…fff` (~2^236) | **`0x7fff…fff` (~2^255)** |
| `fPowAllowMinDifficultyBlocks` | false | true | **Preserved** |
| `fPowNoRetargeting` | false | false | **Preserved** |
| `nBits` genesis | `0x207fffff` | `0x207fffff` | **Preserved (genesis unchanged)** |

`DifficultyAdjustmentInterval()` = `86400 / 150 = 576` blocks. `nMinimumDifficultyBlocks` remains 0 for mainnet/testnet.

## 2. Critical finding

The PHR genesis `nBits` is `0x207fffff`, which means a target of `0x7fffff << 232` (~2^255). This target is **looser** than the old mainnet/testnet `powLimit` (~2^236).

`CheckProofOfWork()` rejects a block if the compact target exceeds `powLimit`. Because `GetNextWorkRequired()` under the old schedule returns the genesis `nBits` for the early blocks, **the first block will be rejected**.

Simulation results for 5,000 blocks:

| Scenario | `valid_bits` | `first_invalid_height` | `bits_h1` | `bits_h5000` |
|---|---|---|---:|---|
| `old_main_powlimit_old_schedule` | false | **1** | `207fffff` | `1e0fce77` |
| `old_main_powlimit_fresh_schedule` | true | - | `1e0fffff` | `1b06d395` |
| `fresh_powlimit_fresh_dgw` | true | - | `207fffff` | `1c00a747` |
| `fresh_powlimit_btc_until_576` | true | - | `207fffff` | `1f00bed5` |
| `regtest_no_retarget` | true | - | `207fffff` | `207fffff` |

The first scenario is the pre-change condition and fails at height 1. This is not a problem with the PHR hash function; it is a mismatch between the legacy X11 `powLimit` and the PHR genesis header.

## 3. Options considered

### Option 1 — Keep the old `powLimit`, DGW from genesis

Valid after the first block, but `GetNextWorkRequired()` immediately clamps the target to ~2^236 (`0x1e0fffff`). This ignores the genesis target and continues to use the legacy X11 difficulty limit. Inconsistent with a new chain.

### Option 2 — `powLimit` matching genesis, old BTC/KGW schedule preserved

Valid, but the first 576 blocks use the genesis difficulty without adjustment, and only then does it change. It still retains the legacy transition schedule of the old chain.

### Option 3 — `powLimit` matching genesis, DGW active immediately **(selected)**

`powLimit = 0x7fff…fff`, `nPowKGWHeight = 0`, `nPowDGWHeight = 0`. The first block uses `0x207fffff`, valid against `CheckProofOfWork()`, and DGW adjusts every block. There is no legacy X11 BTC/KGW schedule. The 5,000-block simulation shows the target stays within the range `0x207fffff … 0x1c00a747` and never exceeds `powLimit`.

## 4. Implementation decision

- Mainnet and testnet use the same `powLimit` as PHR regtest/devnet: `0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff`.
- `nPowKGWHeight = 0` and `nPowDGWHeight = 0` for mainnet and testnet.
- Spacing, timespan, and the retarget flag are unchanged.
- Regtest is unchanged: `fPowNoRetargeting = true` makes KGW/DGW unreachable, so the old values are inert. The assume-UTXO snapshot is still removed in a separate step.

## 5. Impact and risks

- The initial difficulty is very low (genesis target ~2^255). This is a direct consequence of the genesis that must not be changed. The network must be monitored as DGW adjusts.
- The new `powLimit` is much looser than the X11 value. This drastically changes the initial hashing cost and is part of the hard-reset.
- The simulation uses ideal timestamps (constant spacing). Future timestamps, hashrate spikes, or slow blocks need to be tested with functional tests/regtest.
- This change **does not** alter the PHR-256-YP algorithm, genesis hash, headers, rewards, or the transaction system.

## 6. Status

This report was written before the change. The actual implementation is in `src/kernel/chainparams.cpp` in a separate consensus commit.
