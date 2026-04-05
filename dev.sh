#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
WASM_DIST="${PROJECT_DIR}/build-wasm/wasm/dist"

if [ ! -d "${IDE_DIR}" ]; then
    echo "ide/ not found"
    exit 1
fi

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+."
    exit 1
fi

# Copy WASM artifacts if available
if [ -f "${WASM_DIST}/mlab_repl.wasm" ]; then
    cp "${WASM_DIST}/mlab_repl.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/mlab_repl.wasm" "${IDE_DIR}/public/"
    echo "WASM engine found"
else
    echo "WASM not built — fallback mode (build with: ./build.sh --wasm)"
fi

cd "${IDE_DIR}"

if [ ! -d "node_modules" ]; then
    echo "Installing dependencies..."
    npm install
fi

echo ""
echo "Starting dev server at http://localhost:3000"
echo ""

npm run dev
