#!/usr/bin/env bash
# Full from-scratch rebuild for Linux: wipe build dirs, build engine (linux-release),
# optionally the WASM/browser stack, run the gtest suite, refresh npm dist.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

PRESETS=(linux/release linux/debug linux/portable)
WASM=0
for a in "$@"; do
    [ "$a" = "--wasm" ] && WASM=1
done

echo "=== rebuild-all: wiping build dirs ==="
for p in "${PRESETS[@]}"; do
    if [ -d "$PROJECT_DIR/build/$p" ]; then
        echo "  rm -rf build/$p"
        rm -rf "$PROJECT_DIR/build/$p"
    fi
done
if [ "$WASM" = 1 ] && [ -d "$PROJECT_DIR/build/wasm/release" ]; then
    echo "  rm -rf build/wasm/release"
    rm -rf "$PROJECT_DIR/build/wasm/release"
fi

echo
echo "=== engine (linux-release) ==="
"$SCRIPT_DIR/engine-build.sh"

if [ "$WASM" = 1 ]; then
    echo
    echo "=== wasm + web bundle ==="
    "$SCRIPT_DIR/web-build.sh"
fi

echo
echo "=== gtest suite ==="
"$PROJECT_DIR/build/linux/release/tests/gtest/numkit_gtest"

if [ "$WASM" = 1 ]; then
    echo
    echo "=== npm dist refresh ==="
    node "$PROJECT_DIR/packages/numkit/scripts/refresh-dist.js"
else
    echo "=== npm dist refresh: skipped (no --wasm; dist mirrors the wasm build)"
fi

echo
echo "rebuild-all: OK"
