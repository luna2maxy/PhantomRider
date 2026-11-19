#!/usr/bin/env python3
"""PHR-256-YP proof-of-work hash for the functional test framework.

This mirrors src/hash_phr.h. The returned bytes are in the same internal
(uint256) byte order as dash_hash.getPoWHash(), so callers that reverse the
result still obtain the display hash.
"""

from __future__ import annotations

import hashlib
import struct

DATASET_BYTES = 1024 * 1024
ROUNDS = 32

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms
except ImportError:  # pragma: no cover - fallback for hosts without cryptography
    Cipher = None
    algorithms = None


def _rotl32(v: int, n: int) -> int:
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def _quarter_round(x: list[int], a: int, b: int, c: int, d: int) -> None:
    x[a] = (x[a] + x[b]) & 0xFFFFFFFF
    x[d] = _rotl32(x[d] ^ x[a], 16)
    x[c] = (x[c] + x[d]) & 0xFFFFFFFF
    x[b] = _rotl32(x[b] ^ x[c], 12)
    x[a] = (x[a] + x[b]) & 0xFFFFFFFF
    x[d] = _rotl32(x[d] ^ x[a], 8)
    x[c] = (x[c] + x[d]) & 0xFFFFFFFF
    x[b] = _rotl32(x[b] ^ x[c], 7)


def _chacha20_block_slow(key: bytes) -> bytes:
    state = list(struct.unpack("<4I", b"expand 32-byte k"))
    state += list(struct.unpack("<8I", key))
    state += [0, 0, 0, 0]  # counter 0 and an all-zero 64-bit nonce
    x = state[:]
    for _ in range(10):
        _quarter_round(x, 0, 4, 8, 12)
        _quarter_round(x, 1, 5, 9, 13)
        _quarter_round(x, 2, 6, 10, 14)
        _quarter_round(x, 3, 7, 11, 15)
        _quarter_round(x, 0, 5, 10, 15)
        _quarter_round(x, 1, 6, 11, 12)
        _quarter_round(x, 2, 7, 8, 13)
        _quarter_round(x, 3, 4, 9, 14)
    return struct.pack("<16I", *[(x[i] + state[i]) & 0xFFFFFFFF for i in range(16)])


_ZERO_BLOCK = bytes(64)
_CHACHA_NONCE = bytes(16)


def _chacha20_block(key: bytes) -> bytes:
    if Cipher is None:
        return _chacha20_block_slow(key)
    encryptor = Cipher(algorithms.ChaCha20(key, _CHACHA_NONCE), None).encryptor()
    return encryptor.update(_ZERO_BLOCK) + encryptor.finalize()


def _chained_dataset(key: bytes, length: int) -> bytes:
    out = bytearray()
    chain = key
    while len(out) < length:
        block = _chacha20_block(chain)
        out += block
        chain = block[:32]
    return bytes(out[:length])


def _tagged_hash(tag: bytes, *parts: bytes) -> bytes:
    h = hashlib.sha256()
    h.update(tag)
    h.update(b"\x00")
    for part in parts:
        h.update(part)
    return h.digest()


def phr_yp_digest(header: bytes, pers: bytes = b"PHR", dataset_bytes: int = DATASET_BYTES,
                  rounds: int = ROUNDS) -> bytes:
    """Return the big-endian PHR digest, matching the public hash result."""
    if dataset_bytes < 64 or dataset_bytes % 64:
        raise ValueError("dataset_bytes must be >= 64 and divisible by 64")
    if rounds <= 0:
        raise ValueError("rounds must be > 0")

    dataset = _chained_dataset(_tagged_hash(b"PHR-YP-DATASET", header, pers), dataset_bytes)
    state = _tagged_hash(b"PHR-YP-INIT", header, pers)
    lines = dataset_bytes // 64

    for r in range(rounds):
        idx = int.from_bytes(state[:8], "little") % lines
        line = dataset[idx * 64:(idx + 1) * 64]
        selector = state[8] % 3
        msg = state + line + struct.pack("<I", r) + bytes([selector])
        if selector == 0:
            state = hashlib.sha256(msg).digest()
        elif selector == 1:
            state = hashlib.sha3_256(msg).digest()
        else:
            state = hashlib.blake2s(msg).digest()

    return _tagged_hash(b"PHR-YP-FINAL", state, header, pers)


def phr_hash(header: bytes) -> bytes:
    """PoW hash in the internal byte order expected by the test framework."""
    return phr_yp_digest(header)[::-1]
