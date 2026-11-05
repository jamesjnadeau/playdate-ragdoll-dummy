#!/usr/bin/env bash
# perf/run.sh [scenario...]: runs pdsim scenarios in parallel (default: all in perf/scenarios/).
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
PDSIM="${PDSIM_BIN:-$ROOT/.pdsim/src/target/release/pdsim}"
CAL="${PDSIM_CAL:-$ROOT/.pdsim/src/calibration/rev-a.json}"
export PLAYDATE_SDK_PATH="${PLAYDATE_SDK_PATH:-/root/PlaydateSDK}"

if [ $# -eq 0 ]; then
    set -- $(cd perf/scenarios && ls -d */ | tr -d /)
fi
mkdir -p build/perf

run_one() {
    local s=$1 dir args
    dir=$(mktemp -d)
    trap 'rm -rf "$dir"' RETURN
    cp "perf/scenarios/$s/bench.json" "$dir/bench.json" || return 1
    args=(run build/pdsim-stage --frames 600 --input "perf/scenarios/$s/input.json"
          --calibration "$CAL" --data-dir "$dir" --report "build/perf/$s.json"
          --frames-out "build/perf/$s.jsonl" --screenshot "build/perf/$s.png"
          --max-over-budget 0 -q)
    if [ -f "perf/scenarios/$s/max_avg_ms" ]; then
        args+=(--max-avg-ms "$(tr -d '[:space:]' < "perf/scenarios/$s/max_avg_ms")")
    fi
    "$PDSIM" "${args[@]}" > "build/perf/$s.log" 2>&1
}

pids=()
for s in "$@"; do
    run_one "$s" &
    pids+=($!)
done

failed=()
i=0
for s in "$@"; do
    if wait "${pids[$i]}"; then
        echo "PASS $s"
    else
        echo "FAIL $s (see build/perf/$s.log)"
        failed+=("$s")
    fi
    cat "build/perf/$s.log" 2>/dev/null
    i=$((i + 1))
done
if [ ${#failed[@]} -gt 0 ]; then
    echo "perf failures: ${failed[*]}" >&2
    exit 1
fi
