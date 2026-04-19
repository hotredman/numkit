#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
WASM_DIST="${PROJECT_DIR}/build-wasm/wasm/dist"
PAGES_DIR="${PROJECT_DIR}/docs"

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+."
    exit 1
fi

# Build WASM if emcc available
if command -v emcc &>/dev/null; then
    if [ ! -f "${WASM_DIST}/numkit_mide.wasm" ]; then
        echo "Building WASM..."
        bash "${PROJECT_DIR}/build.sh" --wasm
    fi
    echo "Copying WASM files into ide/public/..."
    cp "${WASM_DIST}/numkit_mide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_mide.wasm" "${IDE_DIR}/public/"
else
    echo "emcc not found — building without WASM (fallback mode only)"
fi

# Generate examples manifest
if [ -f "${IDE_DIR}/scripts/generate-manifest.js" ]; then
    echo "Generating examples manifest..."
    node "${IDE_DIR}/scripts/generate-manifest.js"
fi

# Install deps and build
cd "${IDE_DIR}"
[ ! -d "node_modules" ] && npm install
echo "Building Vite production bundle..."
npm run build

# Copy to docs/ for GitHub Pages
rm -rf "${PAGES_DIR}"
mkdir -p "${PAGES_DIR}"
cp -r "${IDE_DIR}/dist/"* "${PAGES_DIR}/"
touch "${PAGES_DIR}/.nojekyll"

echo ""
echo "Deploy complete! Files in docs/"
echo ""
echo "  git add docs/"
echo "  git commit -m 'Deploy to GitHub Pages'"
echo "  git push"
