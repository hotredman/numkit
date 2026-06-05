#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
WASM_DIST="${PROJECT_DIR}/build/browser/wasm/dist"
PAGES_DIR="${PROJECT_DIR}/docs"

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+."
    exit 1
fi

# Source emsdk env if present so emcc is in PATH for sub-shells.
if [ -f "${PROJECT_DIR}/.claude_emsdk_env.sh" ]; then
    # shellcheck source=/dev/null
    source "${PROJECT_DIR}/.claude_emsdk_env.sh"
fi

# Build WASM if emcc available; otherwise reuse a pre-built one if it exists.
if command -v emcc &>/dev/null; then
    if [ ! -f "${WASM_DIST}/numkit_ide.wasm" ]; then
        echo "Building WASM..."
        bash "$(dirname "$0")/build.sh" --wasm
    fi
    echo "Copying freshly-built WASM into ide/public/..."
    cp "${WASM_DIST}/numkit_ide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_ide.wasm" "${IDE_DIR}/public/"
elif [ -f "${WASM_DIST}/numkit_ide.wasm" ]; then
    echo "emcc not on PATH but ${WASM_DIST}/numkit_ide.wasm exists — copying it."
    cp "${WASM_DIST}/numkit_ide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_ide.wasm" "${IDE_DIR}/public/"
else
    echo "emcc not found and no pre-built WASM in ${WASM_DIST} — falling back."
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
