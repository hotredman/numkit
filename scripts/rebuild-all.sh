#!/usr/bin/env bash
# Full from-scratch rebuild: wipe build dirs, build the engine (desktop-fast),
# optionally the WASM/browser stack, run the gtest suite, refresh the npm dist.
#
# Usage:
#   scripts/rebuild-all.sh            engine (desktop-fast) + tests + dist
#   scripts/rebuild-all.sh --wasm     + browser preset and the IDE web bundle
#                                     (needs EMSDK sourced)
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PRESETS=(desktop-fast)
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
if [ "$WASM" = 1 ] && [ -d "$PROJECT_DIR/build/browser" ]; then
    echo "  rm -rf build/browser"
    rm -rf "$PROJECT_DIR/build/browser"
fi

echo "=== engine (desktop-fast) ==="
"$PROJECT_DIR/scripts/engine-build.sh" --fast

if [ "$WASM" = 1 ]; then
    echo "=== wasm + web bundle ==="
    "$PROJECT_DIR/scripts/web-build.sh"
fi

echo "=== gtest suite ==="
"$PROJECT_DIR/build/desktop-fast/tests/gtest/Release/numkit_gtest.exe" \
    || "$PROJECT_DIR/build/desktop-fast/tests/gtest/numkit_gtest"

# The npm dist IS the wasm build — refresh it only when --wasm rebuilt it;
# without a fresh wasm, refresh-dist correctly fails closed on staleness.
if [ "$WASM" = 1 ]; then
    echo "=== npm dist refresh ==="
    node "$PROJECT_DIR/packages/numkit/scripts/refresh-dist.js"
else
    echo "=== npm dist refresh: skipped (no --wasm; dist mirrors the wasm build)"
fi

echo
echo "rebuild-all: OK"
