#!/usr/bin/env bash
# Stages build/pdsim-stage/: pdex.elf plus Source/ assets (pdsim reads PNG next to pdex.elf).
# bench.json is never copied: it must not ship, and perf/run.sh puts it in the data dir.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f build/pdex.elf ] || { echo "build/pdex.elf missing: run make device_bin" >&2; exit 1; }
STAGE=build/pdsim-stage
rm -rf "$STAGE"
mkdir -p "$STAGE"
tar -C Source --exclude=pdex.elf --exclude=pdex.so --exclude=bench.json -cf - . | tar -C "$STAGE" -xf -
cp build/pdex.elf "$STAGE/pdex.elf"
echo "$STAGE"
