#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
VITE_DIR="${PROJECT_DIR}/repl-vite"
WASM_DIST="${PROJECT_DIR}/build-wasm/repl/dist"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()   { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()  { echo -e "${RED}[ERR]${NC}   $*"; }

# ── Проверки ──

if [ ! -d "${VITE_DIR}" ]; then
    err "repl-vite/ not found"
    echo "  Extract mlab-repl-vite.tar.gz first."
    exit 1
fi

if ! command -v node &>/dev/null; then
    err "node not found. Install Node.js 18+."
    exit 1
fi

# ── WASM: подхватываем если собрано ──

if [ -f "${WASM_DIST}/mlab_repl.wasm" ]; then
    cp "${WASM_DIST}/mlab_repl.js"   "${VITE_DIR}/public/"
    cp "${WASM_DIST}/mlab_repl.wasm" "${VITE_DIR}/public/"
    ok "WASM engine found — full mode"
    ENGINE="wasm"
else
    warn "WASM not built — running in demo (fallback) mode"
    echo "     To build WASM: source ~/emsdk/emsdk_env.sh && ./rebuild.sh --wasm"
    ENGINE="fallback"
fi

# ── npm install ──

cd "${VITE_DIR}"

if [ ! -d "node_modules" ]; then
    info "Installing dependencies..."
    npm install
fi

# ── Запуск ──

echo ""
echo -e "${GREEN}════════════════════════════════════════════${NC}"
if [ "${ENGINE}" = "wasm" ]; then
    echo -e "${GREEN}  MLab REPL — WASM engine${NC}"
else
    echo -e "${YELLOW}  MLab REPL — Demo mode (JS fallback)${NC}"
fi
echo -e "${GREEN}════════════════════════════════════════════${NC}"
echo ""
echo -e "  Opening ${CYAN}http://localhost:3000${NC}"
echo -e "  Press ${CYAN}Ctrl+C${NC} to stop"
echo ""

npm run dev
