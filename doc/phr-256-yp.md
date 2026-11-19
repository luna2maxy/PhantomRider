# PHR-256-YP — Implementation Technical Specification

This document describes the **actual implementation** of PHR-256-YP in
`src/hash_phr.h` as integrated into Dash Core in this repository.
This document is **not** a security certification and does not change the algorithm.
Audit reference: `PHR_AUDIT_REPORT.md` (2026-11-19).

---

## 1. Fixed parameters

| Parameter | Value | Source |
|---|---|---|
| Profile name | `PHR-256-YP` | `phr_pow::HashHeaderPHR` |
| Personalization | `"PHR"` (3 bytes, no NUL) | `HashPHR` → `HashHeaderPHR(..., "PHR")` |
| Dataset size | 1,048,576 bytes (1 MiB) | `dataset_bytes = 1024*1024` |
| Dataset row size | 64 bytes | `dataset_bytes / 64 = 16,384` rows |
| Number of rounds | 32 | `rounds = 32` |
| State size | 32 bytes (256 bits) | `phr_pow::Hash` |
| Output size | 32 bytes (256 bits) | final digest |
| PoW input | 80-byte serialized `CBlockHeader` | `CBlockHeader::GetHash()` |
| Stateless | Yes; no epoch/chain context | Dataset is derived from the header itself |

---

## 2. Cryptographic primitives used

The implementation in `src/hash_phr.h` is a self-contained (header-only)
implementation, not an OpenSSL wrapper. All three primitives were verified
against standard `hashlib` vectors in the 2026-11-19 audit.

### 2.1 SHA-256

- Implementation: `Sha256Transform` + `Sha256`.
- The K constants and IV follow FIPS 180-4.
- Padding: `0x80`, zeros, 64-bit **big-endian** message length in the last 8 bytes.
- Output: 32 bytes big-endian (`Store32BE` per word).
- Used for: dataset key derivation, initial state, selector 0, finalization.

### 2.2 SHA3-256

- Implementation: `KeccakF1600` (24 rounds) + `Sha3_256`.
- Rate = 136 bytes; capacity = 64 bytes; output 32 bytes.
- Padding: `0x06` on the first byte of the last block and `0x80` on the last byte
  of the block (`block[kRate-1] |= 0x80`). This is **SHA3** padding, not Keccak-256
  (`0x01`).
- Words are loaded/stored little-endian (`Load64LE` / `Store64LE`).
- Used for: selector 1.

### 2.3 BLAKE2s

- Implementation: `Blake2sCompress` + `Blake2s`.
- IV = SHA-256 IV; parameter block `0x01010000 ^ 32` (digest 32 bytes, key 0).
- 10 compression rounds with the standard BLAKE2s sigma table.
- Counter `t` = number of bytes processed at the start of the block; the final
  block sets the `last` bit (`v[14] = ~v[14]`).
- Output 32 bytes little-endian (`Store32LE` per word).
- Used for: selector 2.

### 2.4 ChaCha20

- Implementation: `ChaCha20Block`.
- State constant: `"expand 32-byte k"`.
- Key = 32 bytes; 64-bit counter; 16-byte zero nonce.
- 20 rounds (10 double-rounds), standard quarter-round with rotations 16/12/8/7.
- Output 64 bytes little-endian.
- Used **only** for dataset generation, not for state mixing.

---

## 3. Tagged input and domain separation

`TaggedInput(tag)` produces:

```
tag_bytes || 0x00
```

Tags used:

| Tag | Function |
|---|---|
| `PHR-YP-DATASET\0` | Derives the dataset key |
| `PHR-YP-INIT\0` | Derives the initial state |
| `PHR-YP-FINAL\0` | Digest finalization |

Domain separation is also achieved by including `header` and `pers` in all three
stages, as well as including the round number `r` and `selector` in each mixing
message. No tag is reused for a different purpose.

---

## 4. Dataset generation

```
dataset_input = "PHR-YP-DATASET" || 0x00 || header || pers
dataset_key   = SHA256(dataset_input)

chain = dataset_key
pos   = 0
while pos < 1 MiB:
    block = ChaCha20Block(key = chain, counter = 0, nonce = 0)
    chunk = min(64, 1 MiB - pos)
    dataset[pos : pos+chunk] = block[0 : chunk]
    chain = block[0 : 32]        // first 32 bytes become the key for the next block
    pos += chunk
```

Key points:

- **Sequential / non-seekable:** to generate the N-th row, all previous blocks
  must be computed because the next block's key comes from the previous block.
- The dataset is **regenerated for every hash**, because the key depends on
  `header` (the nonce is part of the header).
- The buffer is stored as `static thread_local std::vector<uint8_t>`; allocation
  occurs once per thread, and its contents are overwritten on every hash
  (`MakeDataset`).
- Number of ChaCha20 blocks per hash: **16,384**.
- No epoch, disk cache, or other chain data is used.

---

## 5. State initialization

```
init_input = "PHR-YP-INIT" || 0x00 || header || pers
state      = SHA256(init_input)
```

`state` is a 32-byte array. All rounds operate only on this `state` plus one
dataset row and round metadata.

---

## 6. Mixing process — 32 rounds

For `r = 0 … 31`:

```
idx      = LE64(state[0..8]) mod 16384
line     = dataset[idx*64 : (idx+1)*64]
selector = state[8] mod 3

msg = state[0..32]        // 32 byte
    || line               // 64 byte
    || LE32(r)            // 4 byte
    || byte(selector)     // 1 byte
    // total 101 byte

if selector == 0: state = SHA256(msg)
if selector == 1: state = SHA3_256(msg)
if selector == 2: state = BLAKE2s(msg)
```

Characteristics:

- `idx` uses the **little-endian** interpretation of the first 8 bytes of the
  state.
- `selector` uses the 9th byte of the state (`state[8]`) modulo 3.
- The selected hash varies depending on the state, so the computation path is
  not fixed.
- The message is always 101 bytes; there is no variable length in the rounds.
- `r` is written as 32-bit little-endian.
- There are no additional round constants besides `r` and `selector` themselves.

---

## 7. Finalization

```
final_input = "PHR-YP-FINAL" || 0x00 || state || header || pers
digest      = SHA256(final_input)
```

`digest` is the 32-byte output; this is called the **PHR-256-YP digest**.

---

## 8. Endianness and output mapping

### 8.1 Block header serialization (80 bytes)

`CBlockHeader::GetHash()` (`src/primitives/block.cpp:13-19`) performs:

```cpp
std::vector<unsigned char> vch(80);
CVectorWriter ss(SER_GETHASH, PROTOCOL_VERSION, vch, 0);
ss << *this;
return HashPHR(vch.data(), vch.data() + vch.size());
```

80-byte layout:

| Offset | Size | Field | Representation |
|---|---|---|---|
| 0 | 4 | `nVersion` | little-endian |
| 4 | 32 | `hashPrevBlock` | `uint256` internal bytes (little-endian) |
| 36 | 32 | `hashMerkleRoot` | `uint256` internal bytes (little-endian) |
| 68 | 4 | `nTime` | little-endian |
| 72 | 4 | `nBits` | little-endian |
| 76 | 4 | `nNonce` | little-endian |

Transaction hashes and the Merkle root remain **Double SHA-256**; PHR is used
only for the block hash / PoW.

### 8.2 Digest → `uint256`

The PHR `digest` is 32 bytes big-endian. `HashPHR` copies it as:

```cpp
for (i = 0; i < 32; ++i)
    result.begin()[i] = digest[31 - i];
```

Because `uint256` internal storage is little-endian (index 0 = least significant
byte), therefore:

- the numeric value of `uint256` equals the big-endian integer of `digest`;
- `uint256::GetHex()` equals the `digest` hex written big-endian;
- `CheckProofOfWork()` compares `UintToArith256(hash) <= target` correctly.

All word conversions use explicit helpers (`Load32LE/BE`, `Store32LE/BE`,
`Load64LE`, `Store64LE/BE`); there is no dependence on host endianness.

### 8.3 Empty input

`HashPHR` replaces a null pointer with the literal `""` when the range is empty,
so `header_len = 0` does not cause pointer arithmetic on nullptr.

---

## 9. Verified properties

Evidence is in `PHR_AUDIT_REPORT.md` and `test/phr_audit/out/`.

| Property | Evidence basis |
|---|---|
| SHA-256 / SHA3-256 / BLAKE2s primitives conform to standards | `"abc"` vectors match `hashlib` |
| Implementation matches the pool reference | 3 official vectors + 8 derived vectors, 11/11 PASS |
| Determinism | 7 compiler/optimization configurations, ASan/UBSan, two identical mining processes |
| Consistent endianness | 80-byte header reconstruction yields 4 asserted genesis hashes |
| Byte/bit distribution | 5,000 hashes; byte χ² 275.19 (df=255, p=0.184); bit 141.11 (p≈1) |
| Avalanche | 2,500 pairs; average 128.15/256 bits (50.06%), σ=7.92 |
| No UB/overflow on tested paths | ASan + UBSan clean |

---

## 10. Properties that are **unproven**

The following points **must not** be claimed as security facts:

1. **Collision resistance / preimage / second-preimage** of the complete
   construction. Finalization uses SHA-256, but the composition of 32 rounds
   with dynamic hash selection has not been formally analyzed.
2. **Sufficiency of 32 rounds.** There is no design document, no security-margin
   argument against reduced-round attacks, and no external publication
   supporting the number 32.
3. **Differential / structural resistance.** The avalanche test is a statistical
   indicator, not proof against differential cryptanalysis.
4. **Memory-hardness.** The 1 MiB dataset is a streaming buffer rebuilt per
   hash; only 32 of 16,384 rows are read. See
   `doc/phr-256-yp-asic-resistance.md`.
5. **ASIC resistance.** It cannot be inferred from the dataset size. This design
   is better described as compute-bound sequential-ChaCha.
6. **Mainnet security.** No independent cryptographic review has been published
   for this construction.

Since the PoW input is public (there is no secret), side channels based on
dataset row selection are not a confidentiality concern; however, this does not
add any cryptographic security evidence.

---

## 11. Complete flow summary

```
header(80) ─┬─► SHA256("PHR-YP-DATASET\0"||header||pers) ─► ChaCha20 chain ─► dataset 1 MiB
            ├─► SHA256("PHR-YP-INIT\0"||header||pers) ─────► state
            │                                                   │
            │                      32× (idx = LE64(state)%16384,
            │                           selector = state[8]%3,
            │                           msg = state||line||LE32(r)||selector,
            │                           state = SHA256/SHA3_256/BLAKE2s(msg))
            │                                                   ▼
            └─► SHA256("PHR-YP-FINAL\0"||state||header||pers) ─► digest 32 B
                                                                   ▼
                                              uint256 (reverse byte order) ─► PoW hash
```
