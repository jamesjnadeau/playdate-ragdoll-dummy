#!/usr/bin/env bash
# Clones and builds the pinned pdsim; prints the binary path. Idempotent.
set -euo pipefail
PDSIM_COMMIT=6af8292199ef02a42baf9901719dbb64395e38e8
PDSIM_URL=https://github.com/jamesjnadeau/playdate-performance-simulator
# Optional faster clone source: export PDSIM_MIRROR=/path/to/local/clone
LOCAL_MIRROR="${PDSIM_MIRROR:-}"
cd "$(dirname "$0")/.."
DIR="${PDSIM_DIR:-.pdsim/src}"

if [ ! -d "$DIR/.git" ]; then
    mkdir -p "$(dirname "$DIR")"
    if [ -n "$LOCAL_MIRROR" ] && [ -d "$LOCAL_MIRROR/.git" ]; then
        git clone -q "$LOCAL_MIRROR" "$DIR" >&2 || { rm -rf "$DIR"; git clone -q "$PDSIM_URL" "$DIR" >&2; }
    else
        git clone -q "$PDSIM_URL" "$DIR" >&2
    fi
fi
if [ "$(git -C "$DIR" rev-parse HEAD)" != "$PDSIM_COMMIT" ]; then
    git -C "$DIR" checkout -q "$PDSIM_COMMIT" 2>/dev/null || {
        git -C "$DIR" fetch -q "$PDSIM_URL" "$PDSIM_COMMIT" >&2
        git -C "$DIR" checkout -q "$PDSIM_COMMIT"
    }
fi
(cd "$DIR" && cargo build --release -p pdsim-cli >&2)
echo "$(cd "$DIR" && pwd)/target/release/pdsim"
