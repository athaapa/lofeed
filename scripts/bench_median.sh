#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <executable> [data_file]" >&2
    exit 1
fi

exe="$1"
data_file="${2:-data/20190730.BX_ITCH_50}"
runs=11

if [[ ! -x "$exe" ]]; then
    echo "Executable not found or not executable: $exe" >&2
    exit 1
fi

if [[ ! -f "$data_file" ]]; then
    echo "Data file not found: $data_file" >&2
    exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

times_file="$tmp_dir/times.txt"

echo "Executable: $exe"
echo "Data file:  $data_file"
echo "Runs:       $runs (discarding run 1)"
echo

for i in $(seq 1 "$runs"); do
    time_output="$tmp_dir/time_$i.txt"
    /usr/bin/time -p "$exe" "$data_file" >/dev/null 2>"$time_output"
    real_time="$(awk '$1 == "real" { print $2 }' "$time_output")"

    if [[ -z "$real_time" ]]; then
        echo "Failed to parse timing for run $i" >&2
        cat "$time_output" >&2
        exit 1
    fi

    if [[ "$i" -eq 1 ]]; then
        printf "run %02d: %ss (warmup, discarded)\n" "$i" "$real_time"
    else
        printf "run %02d: %ss\n" "$i" "$real_time"
        echo "$real_time" >>"$times_file"
    fi
done

median="$(sort -n "$times_file" | awk '
    NR == 5 { lo = $1 }
    NR == 6 { hi = $1 }
    END { printf "%.6f", (lo + hi) / 2 }
')"

echo
echo "Median real time (runs 2-11): ${median}s"
