#!/usr/bin/env python3
"""Generate independent PHR-256-YP vectors using the pool reference implementation.

Output TSV columns: name, header_hex, dataset_bytes, rounds, expected_digest.
The reference file is intentionally external to this repository so vectors are
not derived from the code under test.
"""
from __future__ import annotations

import pathlib
import sys

POOL_REF = pathlib.Path("/home/ubuntu/Documents/pool/algorithm/phantomrider")
sys.path.insert(0, str(POOL_REF))
from phr_yp_ref import phr_yp_hash  # noqa: E402

PERS = b"PHR"
DATASET = 1024 * 1024
ROUNDS = 32


def base_header() -> bytes:
    """Canonical 80-byte block-header-shaped input used for mutation cases."""
    h = bytearray(80)
    h[0:4] = (1).to_bytes(4, "little")       # nVersion
    h[36:68] = bytes(range(32))              # hashMerkleRoot (distinct bytes)
    h[68:72] = (1795153201).to_bytes(4, "little")  # nTime
    h[72:76] = (0x207FFFFF).to_bytes(4, "little")  # nBits
    h[76:80] = (0).to_bytes(4, "little")     # nNonce
    return bytes(h)


def emit(name: str, header: bytes, dataset: int = DATASET, rounds: int = ROUNDS) -> None:
    digest = phr_yp_hash(header, PERS, dataset, rounds)
    print(f"{name}\t{header.hex()}\t{dataset}\t{rounds}\t{digest.hex()}")


def main() -> None:
    official = bytes(range(4))
    emit("official_64B_1r", official, 64, 1)
    emit("official_1MiB_32r", official, DATASET, ROUNDS)
    emit("official_80B_1MiB_32r", bytes(range(80)), DATASET, ROUNDS)

    emit("empty_input", b"", DATASET, ROUNDS)
    emit("all_zero_80B", bytes(80), DATASET, ROUNDS)
    emit("all_ff_80B", b"\xff" * 80, DATASET, ROUNDS)
    emit("normal_header", base_header(), DATASET, ROUNDS)

    single_bit = bytearray(base_header())
    single_bit[0] ^= 0x01
    emit("single_bit_flip", bytes(single_bit), DATASET, ROUNDS)

    nonce = bytearray(base_header())
    nonce[76:80] = (1).to_bytes(4, "little")
    emit("nonce_plus_one", bytes(nonce), DATASET, ROUNDS)

    timestamp = bytearray(base_header())
    timestamp[68:72] = (1390095619).to_bytes(4, "little")
    emit("timestamp_plus_one", bytes(timestamp), DATASET, ROUNDS)

    merkle = bytearray(base_header())
    merkle[36] ^= 0x80
    emit("merkle_root_bit_flip", bytes(merkle), DATASET, ROUNDS)


if __name__ == "__main__":
    main()
