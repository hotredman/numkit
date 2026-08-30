#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
WASM_DIST="${PROJECT_DIR}/build/browser/wasm/dist"
DEPLOY_DIR="${PROJECT_DIR}/deploy"

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+."
    exit 1
fi

# Source emsdk env if present so emcc is in PATH for sub-shells.
if [ -f "${PROJECT_DIR}/.claude_emsdk_env.sh" ]; then
    # shellcheck source=/dev/null
    source "${PROJECT_DIR}/.claude_emsdk_env.sh"
fi

SKIP_WASM=0
for arg in "$@"; do
    if [ "$arg" == "--skip-wasm" ]; then SKIP_WASM=1; fi
done

# Build the WASM engine. FAIL-CLOSED: a silent fallback to a pre-built
# wasm once shipped a stale engine into a publish run — reusing an
# existing artifact is allowed ONLY with an explicit --skip-wasm.
if command -v emcc &>/dev/null; then
    if [ "$SKIP_WASM" -eq 1 ]; then
        echo "Skipping WASM rebuild (--skip-wasm)..."
    else
        echo "Building WASM..."
        bash "$(dirname "$0")/engine-build.sh" --wasm
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

# Copy the built site into deploy/ (local output dir; gitignored)
rm -rf "${DEPLOY_DIR}"
mkdir -p "${DEPLOY_DIR}"
cp -r "${IDE_DIR}/dist/"* "${DEPLOY_DIR}/"
touch "${DEPLOY_DIR}/.nojekyll"

echo ""
echo "Build complete! Static IDE site in deploy/ (gitignored)."
echo "Serve it from any static host — the base is relative, so it works at the"
echo "web root OR a sub-path. Preview locally with: (cd ide && npm run preview)."
