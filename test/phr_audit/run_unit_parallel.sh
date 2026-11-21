#!/usr/bin/env bash
set -u

base_dir="test/phr_audit/out/unit_parallel"
mkdir -p "$base_dir"

for i in 1 2 3 4 5 6; do
    rm -f "$base_dir/unit_${i}.log" "$base_dir/unit_${i}.exit" "$base_dir/unit_${i}.pid"
done

for i in 1 2 3 4 5 6; do
    filter_file="$base_dir/filter_${i}.txt"
    log_file="$base_dir/unit_${i}.log"
    exit_file="$base_dir/unit_${i}.exit"
    filter="$(cat "$filter_file")"

    nohup setsid bash -c '
        filter_file="$1"
        log_file="$2"
        exit_file="$3"
        filter="$(cat "$filter_file")"
        stdbuf -oL -eL ./build/src/test/test_dash --run_test="$filter" --log_level=test_suite > "$log_file" 2>&1
        echo $? > "$exit_file"
    ' _ "$filter_file" "$log_file" "$exit_file" >/dev/null 2>&1 </dev/null &

    echo $! > "$base_dir/unit_${i}.pid"
done

echo "launched 6 unit-test processes"
