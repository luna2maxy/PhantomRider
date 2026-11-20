#!/usr/bin/env python3
# Copyright (c) 2016-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# address.py
#
# This file encodes and decodes BASE58 P2PKH and P2SH addresses
#

import unittest

from .script import hash160, hash256, CScript
from test_framework.script_util import (
    keyhash_to_p2pkh_script,
    scripthash_to_p2sh_script,
)

# Note unlike in bitcoin, this address isn't bech32 since we don't (at this time) support bech32.
ADDRESS_BCRT1_UNSPENDABLE = 'rPJ947uQfo6Bax9ZLj1ifeUbYWBug5mz3gz'
ADDRESS_BCRT1_UNSPENDABLE_DESCRIPTOR = 'addr(rPJ947uQfo6Bax9ZLj1ifeUbYWBug5mz3gz)#dezw8tlm'
ADDRESS_BCRT1_P2SH_OP_TRUE = 'sEnxHKy7YDD5jXdVcoZ9s95cJqSb5N9oS8'


b58chars = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'

# PHR regtest (default for functional tests) and mainnet version prefixes.
# Regtest P2PKH uses a 2-byte prefix ("rP"); P2SH is 1 byte ("r").
REGTEST_PUBKEY_PREFIX = bytes([27, 206])
REGTEST_SCRIPT_PREFIX = bytes([124])
MAINNET_PUBKEY_PREFIX = bytes([55])
MAINNET_SCRIPT_PREFIX = bytes([16])
TESTNET_PUBKEY_PREFIX = bytes([28, 238])
TESTNET_SCRIPT_PREFIX = bytes([19])


def _version_bytes(version):
    if isinstance(version, (bytes, bytearray)):
        return bytes(version)
    # support single-byte ints and legacy packed ints
    if isinstance(version, int):
        if version <= 0xff:
            return bytes([version])
        out = []
        v = version
        while v > 0:
            out.append(v & 0xff)
            v >>= 8
        return bytes(reversed(out))
    raise TypeError(version)


def byte_to_base58(b, version):
    result = ''
    ver = _version_bytes(version)
    b = ver + b  # prepend version
    b += hash256(b)[:4]       # append checksum
    value = int.from_bytes(b, 'big')
    while value > 0:
        result = b58chars[value % 58] + result
        value //= 58
    while b[0] == 0:
        result = b58chars[0] + result
        b = b[1:]
    return result


def base58_to_byte(s):
    """Converts a base58-encoded string to its (payload, version).

    Throws if the base58 checksum is invalid. PHR regtest/testnet/devnet
    P2PKH use 2-byte prefixes; these are detected explicitly. All other
    encodings keep legacy behavior (1-byte version, remaining bytes payload)
    for backward compatibility with framework unit tests and callers."""
    if not s:
        return b'', 0
    n = 0
    for c in s:
        n *= 58
        assert c in b58chars
        digit = b58chars.index(c)
        n += digit
    res = n.to_bytes((n.bit_length() + 7) // 8, 'big') if n else b''
    pad = 0
    for c in s:
        if c == b58chars[0]:
            pad += 1
        else:
            break
    res = b'\x00' * pad + res

    if hash256(res[:-4])[:4] != res[-4:]:
        raise ValueError('Invalid Base58Check checksum')

    payload = res[:-4]
    multibyte_pkh = (REGTEST_PUBKEY_PREFIX, TESTNET_PUBKEY_PREFIX)
    if len(payload) >= 2 and payload[:2] in multibyte_pkh:
        return payload[2:], payload[:2]
    ver = payload[:1]
    return payload[1:], (ver[0] if ver else 0)


def keyhash_to_p2pkh(hash, main=False):
    assert len(hash) == 20
    version = MAINNET_PUBKEY_PREFIX if main else REGTEST_PUBKEY_PREFIX
    return byte_to_base58(hash, version)

def scripthash_to_p2sh(hash, main=False):
    assert len(hash) == 20
    version = MAINNET_SCRIPT_PREFIX if main else REGTEST_SCRIPT_PREFIX
    return byte_to_base58(hash, version)

def key_to_p2pkh(key, main=False):
    key = check_key(key)
    return keyhash_to_p2pkh(hash160(key), main)

def script_to_p2sh(script, main=False):
    script = check_script(script)
    return scripthash_to_p2sh(hash160(script), main)

def check_key(key):
    if (type(key) is str):
        key = bytes.fromhex(key)  # Assuming this is hex string
    if (type(key) is bytes and (len(key) == 33 or len(key) == 65)):
        return key
    assert False

def check_script(script):
    if (type(script) is str):
        script = bytes.fromhex(script)  # Assuming this is hex string
    if (type(script) is bytes or type(script) is CScript):
        return script
    assert False


def address_to_scriptpubkey(address):
    """Converts a given address to the corresponding output script (scriptPubKey)."""
    payload, version = base58_to_byte(address)
    v = _version_bytes(version)
    if v in (REGTEST_PUBKEY_PREFIX, MAINNET_PUBKEY_PREFIX, TESTNET_PUBKEY_PREFIX):
        return keyhash_to_p2pkh_script(payload)
    elif v in (REGTEST_SCRIPT_PREFIX, MAINNET_SCRIPT_PREFIX, TESTNET_SCRIPT_PREFIX):
        return scripthash_to_p2sh_script(payload)
    # TODO: also support other address formats
    else:
        raise AssertionError(f"Unsupported address version bytes: {list(v)}")


class TestFrameworkScript(unittest.TestCase):
    def test_base58encodedecode(self):
        def check_base58(data, version):
            self.assertEqual(base58_to_byte(byte_to_base58(data, version)), (data, version))

        check_base58(bytes.fromhex('1f8ea1702a7bd4941bca0941b852c4bbfedb2e05'), 111)
        check_base58(bytes.fromhex('3a0b05f4d7f66c3ba7009f453530296c845cc9cf'), 111)
        check_base58(bytes.fromhex('41c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('0041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('000041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('00000041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('1f8ea1702a7bd4941bca0941b852c4bbfedb2e05'), 0)
        check_base58(bytes.fromhex('3a0b05f4d7f66c3ba7009f453530296c845cc9cf'), 0)
        check_base58(bytes.fromhex('41c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('0041c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('000041c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('00000041c1eaf111802559bad61b60d62b1f897c63928a'), 0)
