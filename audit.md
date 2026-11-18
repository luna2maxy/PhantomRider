# PHR-256-YP PoW Integration Audit

**Project:** `/home/ubuntu/Documents/dash`  
**Date:** 2026-11-19  
**Algorithm source/reference:** `/home/ubuntu/Documents/pool/algorithm/phantomrider` (`phr_yp.h`, `phr_yp.cpp`, `phr_hash.cpp`, `phr_yp_selftest.cpp`, `README.md`)

## 1. Summary

Dash block Proof-of-Work hashing was migrated from **X11** to **PHR-256-YP** (PhantomRider YP profile) for block header hashing/identity. `CBlockHeader::GetHash()` now computes PHR-256-YP over the 80-byte serialized block header (`SER_GETHASH`, `PROTOCOL_VERSION`) and the resulting digest is used consistently for block identity and PoW target checks (`CheckProofOfWork`). New genesis blocks were mined under PHR-256-YP and wired into chain parameters (and dependent references) for main, testnet, devnet base, and regtest.

## 2. PHR-256-YP Parameters (reference)

- Profile: `PHR-256-YP` (stateless, memory-hard per header)
- Dataset: **1 MiB** (`1024 * 1024` bytes), generated per header
- Rounds: **32**
- Personalization: `"PHR"`
- Digest: **32 bytes** (256-bit)
- Input: 80-byte block header serialization (standard Dash/Bitcoin header fields: `nVersion`, `hashPrevBlock`, `hashMerkleRoot`, `nTime`, `nBits`, `nNonce`)
- Pool reference notes: digest is big-endian; node/block hash representation must map correctly to `uint256`/`arith_uint256` for target comparison and display.

## 3. Integration Details

### 3.1 Core implementation
- **`src/hash_phr.h`** (replaced X11 PoW implementation): added header-only PHR-256-YP implementation (`namespace phr_pow`) including SHA-256, SHA3-256 (Keccak), BLAKE2s, ChaCha20 dataset expansion, tagged inputs (`PHR-YP-DATASET`, `PHR-YP-INIT`, `PHR-YP-FINAL`), and `HashHeaderPHR(...)` with dataset=1 MiB, rounds=32, pers=`"PHR"`.
  - `HashPHR(begin, end)`: computes PHR digest over the input range and returns `uint256`.
- **`src/primitives/block.cpp`**: `CBlockHeader::GetHash()` now serializes the 80-byte header and returns `HashPHR(vch.data(), vch.data() + vch.size())` (replacing previous X11 hashing).

### 3.2 Digest ↔ `uint256` / target mapping (audit)
- PHR produces a 32-byte **big-endian** digest. `HashPHR` stores it so that `uint256` numeric value equals the digest: `result.begin()+i = digest[31-i]` (little-endian internal limb storage), making `result.GetHex()` equal to the big-endian digest hex.
- `CheckProofOfWork(uint256 hash, unsigned int nBits, ...)` in `src/pow.cpp` evaluates `UintToArith256(hash) <= bnTarget` (and related checks). This mapping preserves correct PoW comparison semantics and human-readable hash display consistent with the mined digest.
- No changes to compact-target encoding/retarget math; only the hash function feeding the comparison changed.

### 3.3 Build / call-site updates
- **`src/bench/pow_hash.cpp`**: benchmark hashing call switched to `HashPHR(...)` (benchmark function names remain `Pow_PHR_*` as-is).
- **`src/bench/crypto_hash.cpp`**: updated to include `hash_phr.h`.
- **`src/crypto/CMakeLists.txt`**: no new source added (PHR is implemented in `src/hash_phr.h`, included from existing PoW/block paths). Existing `src/crypto/x11/*` sources remain in tree but are no longer used for block PoW.

## 4. PoW / Hash Path Audit

### 4.1 Consistent propagation (block hash / PoW)
- **Canonical source:** `CBlockHeader::GetHash()` is the block hash/PoW hash. All consumers of `block.GetHash()` / `CBlockIndex::GetBlockHash()` now operate on PHR-256-YP hashes.
- **Validation & storage:** `CheckProofOfWork` (`src/pow.*`) compares against the provided hash (now PHR). Consensus/validation paths (e.g. `src/validation.cpp`), RPC/mining paths (e.g. `src/rpc/mining.cpp`), block storage/index/db paths (e.g. `src/node/blockstorage.cpp`, `src/txdb.cpp`), and mining helpers/miner flows (e.g. `src/test/util/mining.cpp`, node miner utilities) rely on `GetHash()`/block-hash identity and therefore follow the new algorithm uniformly.
- **Difficulty/retarget:** logic in `src/pow.*` unchanged aside from evaluating PHR hashes against targets.

### 4.2 Intentionally unchanged (non-PoW)
- Transaction IDs, merkle roots, and block merkle computation remain **double-SHA256d** (genesis coinbase/merkle construction unchanged).
- `SER_GETHASH`/serialization-based hashes used in Evo/special transactions, provider/quorum/DMN-related hashing, script/address hashing, and other non-PoW subsystems were **not** changed to PHR (separate semantics from block PoW).
- Consensus parameters unrelated to PoW algorithm (subsidy schedules, deployments, LLMQ/governance logic, etc.) were not modified except genesis-related values noted below.

### 4.3 Performance / operational note
- PHR-256-YP derives a 1 MiB dataset from `(header || pers)` per hash attempt (nonce changes the header and thus the dataset), making naive nonce scanning and `FindDevNetGenesisBlock` CPU/memory intensive per attempt relative to X11. This is expected for the algorithm.

## 5. Official Vector Verification

### 5.1 Pool reference selftest
Command: `/home/ubuntu/Documents/pool/algorithm/phantomrider/build/phr_yp_selftest`  
Result: **PASS (all)**

| Vector | Expected digest (hex) | Result |
|---|---|---|
| `64B/1r` | `4363c05142a95741a8e0cb1d938e6b400cc41cb7d9bb247ac111404193eb4243` | PASS |
| `1MiB/32r` | `6e1e85ee9a61847bef13adaa190635674bb2809332dca0ddd7707fa0fe8e3e1e` | PASS |
| `80B/1MiB/32r` | `ade861711ac82e11193cbf03c6432f4eb23afb4a629d9e0b3fb20a86ed04eae2` | PASS |

### 5.2 Integrated implementation logic check
A standalone check compiled from the integrated PHR implementation logic (matching `src/hash_phr.h` behavior) was executed against the same official vectors.

| Vector | Input/params | Digest (hex) | Result |
|---|---|---|---|
| `64B/1r` | header `00010203`, dataset 64B, rounds 1, pers `"PHR"` | `4363c05142a95741a8e0cb1d938e6b400cc41cb7d9bb247ac111404193eb4243` | PASS |
| `1MiB/32r` | header `00010203`, dataset 1 MiB, rounds 32, pers `"PHR"` | `6e1e85ee9a61847bef13adaa190635674bb2809332dca0ddd7707fa0fe8e3e1e` | PASS |
| `80B/1MiB/32r` | header bytes `0x00..0x4f` (80B), dataset 1 MiB, rounds 32, pers `"PHR"` | `ade861711ac82e11193cbf03c6432f4eb23afb4a629d9e0b3fb20a86ed04eae2` | PASS |

## 6. Genesis Blocks (PHR-256-YP)

### 6.1 Coinbase / merkle (verified)
- Wired-timestamp genesis coinbase (pszTimestamp `"Wired 09/Jan/2014 The Grand Experiment Goes Live: Overstock.com Is Now Accepting Bitcoins"`, 50 COIN, standard genesis output script): **unchanged**.
- Genesis merkle root (unchanged): `0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7`

### 6.2 New mined genesis parameters and hashes
Genesis headers were mined/selected to satisfy `CheckProofOfWork` under PHR-256-YP and written to `src/kernel/chainparams.cpp` (with asserts/checkpoints updated).

| Network | nTime | nBits | nNonce | nVersion | Merkle root | New genesis hash (`GetHash()` / PHR) | Previous genesis hash (X11) |
|---|---:|---:|---:|---:|---|---|---|
| main | 1390095618 | `0x207fffff` | 0 | 1 | `0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7` | `0x67215764b843ce5cbb30a6166c083fe4808019e844c3b57d4cc117d9186ad2fc` | `0x00000ffd590b1485b3caadc19b22e6379c733355108f107a430458cdf3407ab6` |
| testnet | 1390666206 | `0x207fffff` | 1 | 1 | `0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7` | `0x26aea3dd1aad231fc1cf628b1ba2946bcdb709a814eff5d08f4538ed86bacea3` | `0x00000bafbc94add76cb75e2ec92894837288a481e5c005f6563d91623bf8bc2c` |
| devnet (base) | 1417713337 | `0x207fffff` | 0 | 1 | `0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7` | `0x403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077` | `0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e` |
| regtest | 1417713337 | `0x207fffff` | 0 | 1 | `0xe0028eb9648db56b1ac77cf090b99048a8007e2bb64b68f092c03c7f56a662c7` | `0x403ac5ae6a06f8d677cce9fd5cf2bfef5dde8378f4126154be1d30f830e51077` | `0x000008ca1832a4baf228eb1553c03d3a2c8e02399550dd6ea8d65cec3ef23d2e` |

Notes:
- `nBits` for main/testnet set to easy compact target `0x207fffff` (and mined) to make PHR genesis (re)mining feasible; this constitutes a consensus/network reset relative to the original chain.
- Devnet-specific runtime genesis is still derived via `FindDevNetGenesisBlock(...)` (now mined against PHR `GetHash()`); the base genesis/hash updated above anchors devnet initialization parameters/checkpoints as applicable.
- Height-0 genesis checkpoints for devnet/regtest updated to the new hash.

## 7. Files Modified

| File | Change |
|---|---|
| `src/hash_phr.h` | Replaced X11 with PHR-256-YP implementation; implements `HashPHR` with correct big-endian-digest→`uint256` mapping |
| `src/primitives/block.cpp` | `CBlockHeader::GetHash()` uses `HashPHR` over 80-byte serialized header |
| `src/bench/pow_hash.cpp` | Rewritten to benchmark PHR (`Pow_PHR_*`) using `HashPHR` |
| `src/kernel/chainparams.cpp` | New PHR-mined genesis params/nonces/hashes, asserts, and genesis checkpoints for main/testnet/devnet/regtest |
| `test/functional/rpc_getblockstats.py` | Regtest genesis hash updated |
| `test/functional/wallet_transactiontime_rescan.py` | Regtest genesis hash in expected log updated |
| `test/functional/wallet_importdescriptors.py` | Regtest genesis hash in expected log updated |
| `contrib/linearize/example-linearize.cfg` | Genesis hash(es) updated |
| `contrib/linearize/example-linearize-testnet.cfg` | Testnet genesis hash updated |
| `contrib/linearize/linearize-data.py` | Default mainnet genesis hash updated |
| `contrib/linearize/README.md` | Default genesis hash documentation updated |

## 8. Validation & Test Status

- **Completed:** PHR algorithm parity verified against pool official vectors; integrated logic matches vectors (including 80-byte header case); genesis blocks mined with valid PHR hashes for target; chainparams/genesis references updated consistently.
- **Not completed in this environment:** no Dash build artifacts or depends toolchain present (`/home/ubuntu/Documents/dash` has no `build/`, not a git repo; `dashd`/`test_dash` not available). Therefore, **full in-tree compilation and runtime mining/functional tests were not executed here**.
- **Recommended verification (regtest):**
  1. Build Dash with the platform toolchain (e.g. depends toolchain as per project docs).
  2. Run targeted tests, e.g. `ctest --test-dir build` and/or `build/test/functional/test_runner.py -j1` (regtest now anchored to new PHR genesis).
  3. Mine/generate blocks on regtest and confirm produced block hashes satisfy `CheckProofOfWork` under PHR-256-YP and are accepted, e.g. `build/src/dashd -regtest -daemon` then `build/src/dash-cli -regtest generatetoaddress <n> <address>` (or equivalent generate commands) and inspect returned block hashes/headers.

## 9. Residual / Follow-up Audit Items

- **Chain history assumptions (main/testnet):** `nMinimumChainWork`, `defaultAssumeValid`, and historical checkpoints for main/testnet remain from the pre-PHR chain and require re-evaluation/reset for any real PHR network deployment.
- **Cosmetic/cleanup:** benchmark labels still named `Pow_PHR_*`; `src/crypto/x11/*` sources and related includes are no longer required for block PoW and can be cleaned up if desired (kept to avoid broader build refactors in this change).
- **Performance:** per-nonce dataset derivation is expensive by design; mining/generation tooling and any devnet genesis mining paths should be expected to be slower than X11.
- **Scope:** devnet per-name genesis hash (`hashDevnetGenesisBlock`) is computed at runtime by `FindDevNetGenesisBlock` under PHR; ensure runtime environments allow sufficient time to mine it for configured devnet parameters.
- **Broader compatibility:** this is a hard consensus change (block hash/PoW function and genesis hashes changed); existing chain data, peers, and address/chain assumptions tied to old genesis are incompatible by design.
