#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PLAY_DIR="${PROJECT_DIR}/play"
WASM_DIST="${PROJECT_DIR}/build/wasm/release/wasm/dist"

SKIP_WASM=0
for arg in "$@"; do
    case "$arg" in
        --skip-wasm) SKIP_WASM=1 ;;
        -h|--help)
            echo "Usage: play-run.sh [--skip-wasm]"
            echo
            echo "Builds the wasm engine (unless --skip-wasm), syncs the glue into"
            echo "play/public/, regenerates the gallery index, and starts the NK"
            echo "Play dev server at http://localhost:5173."
            exit 0 ;;
    esac
done

echo "=== NK Play dev server ==="
echo

if ! command -v node >/dev/null 2>&1; then
    echo "Node.js not found. Install from https://nodejs.org/"
    exit 1
fi

if [ "$SKIP_WASM" = "0" ]; then
    if [ ! -f "${PROJECT_DIR}/build/wasm/release/wasm/dist/nk_ide.js" ]; then
        echo "NOTE: no wasm artifact yet — building it first..."
        "${SCRIPT_DIR}/engine-build.sh" --wasm
    else
        echo "[WASM] artifact found — reuse (delete build/wasm or pass --skip-wasm to skip this check)"
    fi
fi

if [ ! -d "${PLAY_DIR}/node_modules" ]; then
    echo "Installing play dependencies..."
    (cd "${PLAY_DIR}" && npm install --no-fund --no-audit)
fi

echo "Syncing wasm glue into play/public/ ..."
if [ ! -f "${WASM_DIST}/nk_ide.js" ]; then
    echo "ERROR: ${WASM_DIST}/nk_ide.js not found."
    echo "       Build it first: scripts/linux/engine-build.sh --wasm"
    exit 1
fi
node "${PLAY_DIR}/scripts/sync-wasm.mjs"

echo "Generating gallery index..."
node "${PLAY_DIR}/scripts/build-gallery.mjs"

echo
echo "Starting NK Play at http://localhost:5173 ..."
echo "(Ctrl+C to stop)"
cd "${PLAY_DIR}"
exec npx vite --host
