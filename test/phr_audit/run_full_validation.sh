#!/usr/bin/env bash
# Full validation for the PHR fresh-chain reset. Run after a successful build.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/out"
mkdir -p "$OUT"

TEST_DASH="$ROOT/build/src/test/test_dash"
DASHD="$ROOT/build/src/dashd"
DASH_CLI="$ROOT/build/src/dash-cli"
FUNC_RUNNER="$ROOT/build/test/functional/test_runner.py"

rc=0
run() {
    local name="$1"
    shift
    echo "=== $name ==="
    "$@" 2>&1 | tee "$OUT/$name.log"
    local status=${PIPESTATUS[0]}
    echo "$name exit=$status"
    if [ "$status" -ne 0 ]; then rc=1; fi
}

if [ -x "$TEST_DASH" ]; then
    run unit_phr "$TEST_DASH" --run_test=phr_tests --log_level=test_suite
    run unit_all "$TEST_DASH"
else
    echo "MISSING $TEST_DASH"; rc=1
fi

if [ -x "$FUNC_RUNNER" ]; then
    run functional_basic "$FUNC_RUNNER" -j1 mining_basic.py feature_block.py
else
    echo "MISSING $FUNC_RUNNER"; rc=1
fi

if [ -x "$DASHD" ] && [ -x "$DASH_CLI" ]; then
    run node_validation python3 "$HERE/node_validation.py" --dashd "$DASHD" --dash-cli "$DASH_CLI"
else
    echo "MISSING dashd/dash-cli"; rc=1
fi

echo "full_validation_exit=$rc"
exit "$rc"
