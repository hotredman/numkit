#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
WASM_DIST="${PROJECT_DIR}/build/wasm/release/wasm/dist"
DEPLOY_DIR="${PROJECT_DIR}/deploy"

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+."
    exit 1
fi

SKIP_WASM=0
for arg in "$@"; do
    if [ "$arg" == "--skip-wasm" ]; then SKIP_WASM=1; fi
done

if command -v emcc &>/dev/null; then
    if [ "$SKIP_WASM" -eq 1 ]; then
        echo "Skipping WASM rebuild (--skip-wasm)..."
    else
        echo "Building WASM (wasm-release)..."
        bash "$SCRIPT_DIR/engine-build.sh" --wasm
    fi
    echo "Copying freshly-built WASM into ide/public/..."
    cp "${WASM_DIST}/numkit_ide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_ide.wasm" "${IDE_DIR}/public/"
else
    echo "ERROR: emcc not on PATH — refusing to silently reuse a possibly-stale WASM." >&2
    echo "       Source the emsdk environment, or pass --skip-wasm to reuse the" >&2
    echo "       existing ${WASM_DIST} artifacts explicitly." >&2
    exit 1
fi

if [ -f "${IDE_DIR}/scripts/generate-manifest.js" ]; then
    echo "Generating examples manifest..."
    node "${IDE_DIR}/scripts/generate-manifest.js"
fi

cd "${IDE_DIR}"
[ ! -d "node_modules" ] && npm install
echo "Building Vite production bundle..."
npm run build

rm -rf "${DEPLOY_DIR}"
mkdir -p "${DEPLOY_DIR}"
cp -r "${IDE_DIR}/dist/"* "${DEPLOY_DIR}/"
touch "${DEPLOY_DIR}/.nojekyll"

echo
echo "=== Build complete! Static IDE site in deploy/ ==="
