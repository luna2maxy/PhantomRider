#!/usr/bin/env bash
# Build the standalone PHR-256-YP audit harness against the live implementation.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CXX="${CXX:-g++}"
COMMON_FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -I"$ROOT/src")
mkdir -p "$HERE/out"
"$CXX" "${COMMON_FLAGS[@]}" "$HERE/phr_harness.cpp" "$ROOT/src/arith_uint256.cpp" \
    "$ROOT/src/uint256.cpp" "$ROOT/src/util/strencodings.cpp" \
    -o "$HERE/out/phr_harness" -pthread
echo "built $HERE/out/phr_harness"
