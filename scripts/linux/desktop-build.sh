#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
IDE_DIR="${PROJECT_DIR}/ide"
DESKTOP_DIR="${IDE_DIR}/desktop"
WASM_DIST="${PROJECT_DIR}/build/wasm/release/wasm/dist"

SKIP_WASM=0
NO_PACKAGE=0
SKIP_NATIVE=0

for arg in "$@"; do
    case "$arg" in
        --skip-wasm)   SKIP_WASM=1 ;;
        --no-package)  NO_PACKAGE=1 ;;
        --skip-native) SKIP_NATIVE=1 ;;
        -h|--help)
            echo "Usage: desktop-build.sh [--skip-wasm] [--skip-native] [--no-package]"
            echo
            echo "Builds the Numkit Desktop application using Electron and CMake."
            echo
            echo "Options:"
            echo "  --skip-wasm    Do not rebuild WASM engine (reuse existing build)"
            echo "  --skip-native  Do not rebuild native C++ binaries"
            echo "  --no-package   Build frontend bundle only, skip electron-builder"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
    esac
done

echo "=== Numkit IDE — Desktop Build (Linux) ==="
echo

if ! command -v node &>/dev/null; then
    echo "node not found. Install Node.js 18+." >&2
    exit 1
fi

if [ "$SKIP_WASM" -eq 1 ]; then
    if [ -f "${WASM_DIST}/numkit_ide.wasm" ]; then
        echo "[1/5] Skipping WASM rebuild (--skip-wasm; reusing existing build)"
    else
        echo "[1/5] WARNING: --skip-wasm but no WASM at ${WASM_DIST} — falling through to rebuild"
        SKIP_WASM=0
    fi
fi

if [ "$SKIP_WASM" -eq 0 ]; then
    echo "[1/5] Rebuilding WASM engine via wasm-release preset..."
    "$SCRIPT_DIR/engine-build.sh" --wasm
fi

if [ "$SKIP_NATIVE" -eq 1 ]; then
    echo "[1b/7] Skipping native C++ build (--skip-native)"
else
    echo "[1b/7] Building native executables (linux-release preset)..."
    cmake --build --preset=linux-release
    echo "[1b/7] Native C++ binaries ready"
fi

if [ -f "${WASM_DIST}/numkit_ide.wasm" ]; then
    cp "${WASM_DIST}/numkit_ide.js"   "${IDE_DIR}/public/"
    cp "${WASM_DIST}/numkit_ide.wasm" "${IDE_DIR}/public/"
    echo "[2/7] WASM engine copied to ide/public/"
else
    echo "[2/7] WARNING: WASM not found at ${WASM_DIST} — app will run in demo mode"
fi

if [ ! -d "${IDE_DIR}/node_modules" ]; then
    echo "[3/7] Installing IDE dependencies..."
    cd "${IDE_DIR}" && npm install
fi

echo "[4/7] Generating examples manifest and building static files..."
cd "${IDE_DIR}"
if [ -f "scripts/generate-manifest.js" ]; then
    node "scripts/generate-manifest.js"
fi
npx vite build --base ./

rm -rf "${DESKTOP_DIR}/dist"
cp -r "${IDE_DIR}/dist" "${DESKTOP_DIR}/dist"
echo "     Static files ready at ${DESKTOP_DIR}/dist"

if [ ! -d "${DESKTOP_DIR}/node_modules" ]; then
    echo "[5/7] Installing desktop dependencies..."
    cd "${DESKTOP_DIR}" && npm install
fi

if [ "$NO_PACKAGE" -eq 1 ]; then
    echo "[5/7] Skipping packaging (--no-package)"
    echo "=== Done (dev build) ==="
    echo "Launch with: ./scripts/linux/desktop-run.sh"
    exit 0
fi

echo "[5/7] Packaging Linux desktop app (electron-builder)..."
cd "${DESKTOP_DIR}"
npx electron-builder --linux AppImage dir

DEPLOY_DIR="${PROJECT_DIR}/deploy/desktop"
rm -rf "${DEPLOY_DIR}"
mkdir -p "${DEPLOY_DIR}"

if ls "${DESKTOP_DIR}/release/"*.AppImage 1> /dev/null 2>&1; then
    cp "${DESKTOP_DIR}/release/"*.AppImage "${DEPLOY_DIR}/"
    echo "[6/7] Copied AppImage package to deploy/desktop/"
fi

NATIVE_REPL="${PROJECT_DIR}/build/linux/release/apps/numkit/numkit_repl"
if [ -f "$NATIVE_REPL" ]; then
    cp "$NATIVE_REPL" "${DEPLOY_DIR}/"
    echo "[6/7] Copied interpreter: numkit_repl"
fi

echo
echo "=== Done! ==="
echo "Packaged output : ${DESKTOP_DIR}/release/"
echo "Deploy bundle   : ${DEPLOY_DIR}/"
