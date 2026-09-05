#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
DESKTOP_DIR="${IDE_DIR}/desktop"
WASM_DIST="${PROJECT_DIR}/build/wasm/release/wasm/dist"

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+." >&2
    exit 1
fi

if [ -f "${WASM_DIST}/numkit_ide.wasm" ]; then
    cp "${WASM_DIST}/numkit_ide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_ide.wasm" "${IDE_DIR}/public/"
    if [ -d "${DESKTOP_DIR}/dist" ]; then
        cp "${WASM_DIST}/numkit_ide.js"   "${DESKTOP_DIR}/dist/"
        cp "${WASM_DIST}/numkit_ide.wasm" "${DESKTOP_DIR}/dist/"
    fi
    echo "WASM engine found"
else
    echo "WASM not built — fallback mode"
fi

NATIVE_REPL="${PROJECT_DIR}/build/linux/release/apps/numkit/numkit_repl"
if [ -f "$NATIVE_REPL" ] && [ -d "${PROJECT_DIR}/deploy/desktop" ]; then
    cp "$NATIVE_REPL" "${PROJECT_DIR}/deploy/desktop/"
fi

if [ ! -d "${IDE_DIR}/node_modules" ]; then
    echo "Installing IDE dependencies..."
    cd "${IDE_DIR}" && npm install
fi

if [ ! -d "${DESKTOP_DIR}/node_modules" ]; then
    echo "Installing desktop dependencies..."
    cd "${DESKTOP_DIR}" && npm install
fi

echo
echo "Launching Electron desktop dev shell..."
echo

cd "${DESKTOP_DIR}"
npx electron .
