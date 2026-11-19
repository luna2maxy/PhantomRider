# PHR Fresh-Chain Reset — Fork Activation Review

**Date:** 2026-11-19
**Status at the time of writing:** **no consensus values have been changed yet**
**References:** `PHR_AUDIT_REPORT.md`, `PHR_CONSENSUS_METADATA_REPORT.md`, `docs/PHR_FRESH_CHAIN_BEFORE.md`

This report was written before implementation in accordance with Stage 5. The values written below are **decisions to be applied**, not the current state.

## 1. Principles

The PHR chain is a **new chain with a new genesis**, not a continuation of Dash X11. Therefore, all activation heights derived from X11 numbering must be removed or rescheduled.

The decisions used:

1. Basic block validation rules are active at **height 1** (the first block after genesis), not 0, because genesis is a special block and does not have a BIP34 coinbase.
2. Dash features previously activated at specific heights are also active at **height 1**, because there is no legacy chain history to preserve and no legacy state to migrate.
3. The old activation hashes (`BIP34Hash`, `DIP0003EnforcementHash`, `nSuperblockStartHash`) are set to `uint256()` so that they do not force an X11 hash on the PHR chain.
4. Activation order dependencies are preserved: `V20 >= DIP0003`, `MN_RR >= V20`, `V20 >= V19`.
5. Time-based / EHF deployments are not removed or activated en masse. The V24 and EVO_SHARES time windows are active policy, not remnants of X11 hash/height references, so they are preserved.
6. Regtest retains override heights for functional tests. These are not active X11 references and have been adjusted in the test framework.

## 2. Mainnet

| Parameter | Before | After | Category | Reason |
|---|---:|---:|---|---|
| `BIP34Height` | 951 | 1 | Basic validation | Remove X11 numbering. |
| `BIP34Hash` | `000001f35e…8012` | `uint256()` | Basic validation | The X11 hash does not exist on the PHR chain. |
| `BIP65Height` | 619382 | 1 | Basic validation | Active from the start. |
| `BIP66Height` | 245817 | 1 | Basic validation | Active from the start. |
| `BIP147Height` | 939456 | 1 | Basic validation | Active from the start. |
| `CSVHeight` | 622944 | 1 | Basic validation | Active from the start. |
| `DIP0001Height` | 782208 | 1 | Dash features | No legacy state. |
| `DIP0003Height` | 1028160 | 1 | Dash features | TPM active from the first block. |
| `DIP0003EnforcementHeight` | 1047200 | 1 | Dash features | Follow DIP0003. |
| `DIP0003EnforcementHash` | `000000000000002d…7f81` | `uint256()` | Dash features | The X11 hash is irrelevant. |
| `DIP0008Height` | 1088640 | 1 | Legacy InstantSend | Active from the start. |
| `BRRHeight` | 1374912 | 1 | Reward reallocation | Active from the start, in accordance with the new dependency chain. |
| `DIP0020Height` | 1516032 | 1 | LLMQ | Active from the start. |
| `DIP0024Height` | 1737792 | 1 | LLMQ DIP0024 | Active from the start. |
| `DIP0024QuorumsHeight` | 1738698 | 1 | LLMQ DIP0024 | Follow DIP0024. |
| `V19Height` | 1899072 | 1 | BLS scheme | Active from the start. |
| `V20Height` | 1987776 | 1 | Coinbase/Platform | `>= DIP0003` and `>= V19`. |
| `MN_RRHeight` | 2128896 | 1 | Reward reallocation | `>= V20`. |
| `WithdrawalsHeight` | 2201472 | 1 | Platform | Follow V20. |
| `IPv4OptionalHeight` | `max()` | `max()` | Policy | Not scheduled; unchanged. |
| `MinBIP9WarningHeight` | 2203488 | 2017 | BIP9 | `WithdrawalsHeight + 2016`. |
| `nSuperblockStartHash` | `0000000000020cb2…f8aa3` | `uint256()` | Governance | X11 hash; disable the check. |
| V24 / EVO_SHARES | 2026-11-09 … 2028-11-09 | preserved | Version Bits / EHF | Time-based, not an X11 chain reference. |

## 3. Testnet

| Parameter | Before | After | Category | Reason |
|---|---:|---:|---|---|
| `BIP34Height` | 76 | 1 | Basic validation | Remove X11 numbering. |
| `BIP34Hash` | `000008ebb1db…cb6` | `uint256()` | Basic validation | The X11 hash does not exist on the PHR chain. |
| `BIP65Height` | 2431 | 1 | Basic validation | Active from the start. |
| `BIP66Height` | 2075 | 1 | Basic validation | Active from the start. |
| `BIP147Height` | 4300 | 1 | Basic validation | Active from the start. |
| `CSVHeight` | 8064 | 1 | Basic validation | Active from the start. |
| `DIP0001Height` | 5500 | 1 | Dash features | No legacy state. |
| `DIP0003Height` | 7000 | 1 | Dash features | TPM active from the first block. |
| `DIP0003EnforcementHeight` | 7300 | 1 | Dash features | Follow DIP0003. |
| `DIP0003EnforcementHash` | `00000055ebc0…f8f` | `uint256()` | Dash features | The X11 hash is irrelevant. |
| `DIP0008Height` | 78800 | 1 | Legacy InstantSend | Active from the start. |
| `BRRHeight` | 387500 | 1 | Reward reallocation | Active from the start. |
| `DIP0020Height` | 414100 | 1 | LLMQ | Active from the start. |
| `DIP0024Height` | 769700 | 1 | LLMQ DIP0024 | Active from the start. |
| `DIP0024QuorumsHeight` | 770730 | 1 | LLMQ DIP0024 | Follow DIP0024. |
| `V19Height` | 850100 | 1 | BLS scheme | Active from the start. |
| `V20Height` | 905100 | 1 | Coinbase/Platform | `>= DIP0003` and `>= V19`. |
| `MN_RRHeight` | 1066900 | 1 | Reward reallocation | `>= V20`. |
| `WithdrawalsHeight` | 1148500 | 1 | Platform | Follow V20. |
| `IPv4OptionalHeight` | 0 | 0 | Policy | Preserved. |
| `MinBIP9WarningHeight` | 1150516 | 2017 | BIP9 | `WithdrawalsHeight + 2016`. |
| V24 / EVO_SHARES | 2026-09-24 … NO_TIMEOUT | preserved | Version Bits / EHF | Time-based, not an X11 chain reference. |

## 4. Devnet and Regtest

| Network | Decision | Reason |
|---|---|---|
| Devnet | **Unchanged** | Activation is already at height 1/2 and the related hashes are already null. In accordance with the fresh-chain principle. |
| Regtest | **Unchanged**, except the assume-UTXO snapshot is removed | Low heights are used by functional tests and are not X11 hash references. `nPowKGWHeight`/`nPowDGWHeight` are irrelevant because `fPowNoRetargeting = true`. |

## 5. Parameters that are **not** changed

- `nSubsidyHalvingInterval`, the masternode/budget/superblock schedule, and `nCreditPoolPeriodBlocks`: part of reward policy. Changing them is prohibited by the scope without separate approval.
- `pchMessageStart`, ports, seeds, address prefixes, coin type, and LLMQ types: network identity, not leftover X11 metadata.
- `TESTDUMMY`, `V24`, and `EVO_SHARES`: time-based / EHF deployments. Not activated or removed automatically.
- `IPv4OptionalHeight`: transaction policy, not a leftover X11 chain reference.

## 6. Consensus impact

This is a **hard-reset** change. Old nodes with X11 metadata will not follow the correct PHR chain. All participants must run the binary built from this change and resync from the PHR genesis. Since the genesis was already replaced with the PHR integration, there has been no compatibility with the X11 chain from the outset.

## 7. Status

This report was written before the change. The actual implementation is in `src/kernel/chainparams.cpp` in a separate consensus commit.
