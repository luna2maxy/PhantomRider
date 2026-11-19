# PHR Fresh-Chain Reset — Pre-Migration Configuration

Backup of consensus values before the changes. Kept for documentation and audit.

- Repository: `/home/ubuntu/Documents/GitHub/PhantomRider`
- Branch: `develop`
- HEAD: `9b0a300bc1` (`x11-to-phr integration`)
- File: `src/kernel/chainparams.cpp`
- Git blob: `b317e4539e0f891e06fd8c210be890e0d4aedb13`
- SHA-256: `b948de8a5fc726d6cd4c94c72c57be912b3acbb8a585ac6842486c732266ae63`
- Date: 2026-11-19

## Mainnet

| Parameter | Pre-change value | Line |
|---|---|---|
| `nMinimumChainWork` | `0x00000000000000000000000000000000000000000000c15981a81af44a6abe1a` (X11 block 2,563,000) | 242 |
| `defaultAssumeValid` | `0x000000000000000346a1804da4832b0b69dd9a7f96b14c7cb54585a9ac6ae98f` | 243 |
| `checkpointData` | 40 checkpoints, heights 1,500 … 2,563,000 (X11) | 318–361 |
| `chainTxData` | `{1793218693, 71411418, 0.1306891420721072}` | 368–373 |
| `m_assumed_blockchain_size` | `62` | 259 |
| `m_assumed_chain_state_size` | `1` | 260 |
| `BIP34Height` … `WithdrawalsHeight` | 951 … 2,201,472 (X11 numbering) | 189–207 |
| `nPowKGWHeight` / `nPowDGWHeight` | `15200` / `34140` | 215–216 |
| `powLimit` | `0x00000fff…fff` | 210 |
| Genesis | `67215764…ad2fc` (PHR, unchanged) | 262–265 |

## Testnet

| Parameter | Pre-change value | Line |
|---|---|---|
| `nMinimumChainWork` | `0x00000000000000000000000000000000000000000000000003ed573a939487fc` (X11 block 1,550,000) | 453 |
| `defaultAssumeValid` | `0x000000116cd3c11d44c88cbc0a26804bd277a5d790f684ec3a8aaa820690318c` | 454 |
| `checkpointData` | 20 checkpoints, heights 255 … 1,550,000 (X11) | 523–546 |
| `chainTxData` | `{1788899589, 8494561, 0.0139463532250889}` | 553–558 |
| `m_assumed_blockchain_size` | `10` | 465 |
| `m_assumed_chain_state_size` | `1` | 466 |
| `BIP34Height` … `WithdrawalsHeight` | 76 … 1,148,500 (X11 numbering) | 400–418 |
| `nPowKGWHeight` / `nPowDGWHeight` | `4002` / `4002` | 426–427 |
| `powLimit` | `0x00000fff…fff` | 421 |
| Genesis | `26aea3dd…acea3` (PHR, unchanged) | 468–471 |

## Devnet / Regtest

| Parameter | Pre-change value | Status |
|---|---|---|
| `nMinimumChainWork` | `uint256{}` | Already clean |
| `defaultAssumeValid` | `uint256{}` | Already clean |
| `checkpointData` | PHR genesis (+ devnet genesis) | Already clean |
| `chainTxData` | devnet: 2 tx; regtest: `{0,0,0}` | Already clean |
| `m_assumed_blockchain_size` | `0` (devnet), regtest default | Already clean |
| Regtest `m_assumeutxo_data` | snapshot at heights 110 & 200 from the old chain | **Must be removed** |
| Regtest `nPowKGWHeight` / `nPowDGWHeight` | `15200` / `34140` (mainnet legacy) | Needs review |
| Genesis | `403ac5ae…1077` (PHR, unchanged) | Already correct |

## Other references checked

- `audit.md:85-88` — old/new genesis comparison table; historical documentation, intentionally retained.
- `src/kernel/chainparams.cpp:84` — genesis comment with old X11 hash/nBits/nonce; cosmetic.
- No active X11 genesis hashes found in `contrib/linearize/*` or functional tests.

## Files to be changed (consensus)

Only `src/kernel/chainparams.cpp` contains active consensus values. Documentation and tooling changes are kept separate.
