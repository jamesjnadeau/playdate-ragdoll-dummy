#!/usr/bin/env bash
# perf/run.sh [scenario...]: runs pdsim scenarios in parallel (default: all in perf/scenarios/).
# Rebuilds and restages first; set PERF_NO_BUILD=1 to skip that.
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
PDSIM="${PDSIM_BIN:-$ROOT/.pdsim/src/target/release/pdsim}"
CAL="${PDSIM_CAL:-$ROOT/.pdsim/src/calibration/rev-a.json}"
export PLAYDATE_SDK_PATH="${PLAYDATE_SDK_PATH:-/root/PlaydateSDK}"

if [ $# -eq 0 ]; then
    set -- $(cd perf/scenarios && ls -d */ 2>/dev/null | tr -d /)
fi
if [ $# -eq 0 ]; then
    echo "perf/run.sh: no scenarios found in perf/scenarios/" >&2
    exit 1
fi
mkdir -p build/perf

# Rebuild and restage once, before the parallel runs, so the stage is never stale.
if [ "${PERF_NO_BUILD:-0}" != "1" ]; then
    make device_bin > build/perf/build.log 2>&1 || { cat build/perf/build.log >&2; echo "perf/run.sh: make device_bin failed" >&2; exit 1; }
    tools/stage_pdsim.sh > build/perf/stage.log 2>&1 || { cat build/perf/stage.log >&2; echo "perf/run.sh: stage_pdsim.sh failed" >&2; exit 1; }
fi

run_one() {
    local s=$1 dir args
    rm -f build/perf/"$s".*
    if [ ! -f "perf/scenarios/$s/bench.json" ] || [ ! -f "perf/scenarios/$s/input.json" ]; then
        echo "scenario '$s': perf/scenarios/$s/bench.json or input.json missing" > "build/perf/$s.log"
        return 1
    fi
    dir=$(mktemp -d)
    trap 'rm -rf "$dir"' RETURN
    cp "perf/scenarios/$s/bench.json" "$dir/bench.json" || return 1
    args=(run build/pdsim-stage --frames 600 --input "perf/scenarios/$s/input.json"
          --calibration "$CAL" --data-dir "$dir" --report "build/perf/$s.json"
          --frames-out "build/perf/$s.jsonl" --screenshot "build/perf/$s.png"
          --max-over-budget 0)
    if [ -f "perf/scenarios/$s/max_avg_ms" ]; then
        args+=(--max-avg-ms "$(tr -d '[:space:]' < "perf/scenarios/$s/max_avg_ms")")
    fi
    "$PDSIM" "${args[@]}" > "build/perf/$s.log" 2>&1 || return 1
    # A bad bench.json makes the game fall back to a normal run: never let that pass.
    if grep -q "bench.json INVALID" "build/perf/$s.log" || ! grep -q "bench.json loaded" "build/perf/$s.log"; then
        echo "scenario '$s': bench.json was not loaded by the game" >> "build/perf/$s.log"
        return 1
    fi
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
