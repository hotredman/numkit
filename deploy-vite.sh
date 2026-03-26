#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-wasm"
WASM_DIST="${BUILD_DIR}/repl/dist"
VITE_DIR="${PROJECT_DIR}/repl-vite"
PAGES_DIR="${PROJECT_DIR}/docs"

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
    err "Vite project not found at ${VITE_DIR}"
    echo "  Extract mlab-repl-vite.tar.gz into repl-vite/ first."
    exit 1
fi

if ! command -v node &>/dev/null; then
    err "node not found. Install Node.js 18+ first."
    exit 1
fi

if ! command -v npm &>/dev/null; then
    err "npm not found."
    exit 1
fi

# ── 1. Собираем WASM (если есть emcc) ──

if command -v emcc &>/dev/null; then
    if [ ! -f "${WASM_DIST}/mlab_repl.wasm" ]; then
        info "Building WASM..."
        bash "${PROJECT_DIR}/build.sh"
    else
        ok "WASM already built"
    fi

    info "Copying WASM files into Vite public/..."
    cp "${WASM_DIST}/mlab_repl.js"   "${VITE_DIR}/public/"
    cp "${WASM_DIST}/mlab_repl.wasm" "${VITE_DIR}/public/"
    ok "WASM files copied"
else
    warn "emcc not found — building without WASM (fallback mode only)"
    warn "To include WASM engine: source ~/emsdk/emsdk_env.sh && re-run"
fi

# ── 2. Устанавливаем зависимости ──

info "Installing npm dependencies..."
cd "${VITE_DIR}"

if [ ! -d "node_modules" ]; then
    npm install
else
    ok "node_modules exists, skipping install"
fi

# ── 3. Собираем Vite production build ──

info "Building Vite production bundle..."
npm run build
ok "Vite build complete"

# ── 4. Копируем в docs/ для GitHub Pages ──

info "Copying to docs/..."
rm -rf "${PAGES_DIR}"
mkdir -p "${PAGES_DIR}"
cp -r "${VITE_DIR}/dist/"* "${PAGES_DIR}/"

# GitHub Pages нуждается в .nojekyll чтобы не игнорировать _-файлы
touch "${PAGES_DIR}/.nojekyll"

ok "Deployed to docs/"

# ── Готово ──

echo ""
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo -e "${GREEN}  Deploy complete!${NC}"
echo -e "${GREEN}════════════════════════════════════════${NC}"
echo ""
echo -e "  Files are in ${CYAN}docs/${NC}"
echo ""
echo -e "  Next steps:"
echo ""
echo -e "    ${CYAN}git add docs/${NC}"
echo -e "    ${CYAN}git commit -m 'Deploy MLab REPL (Vite) to GitHub Pages'${NC}"
echo -e "    ${CYAN}git push${NC}"
echo ""
echo -e "  Then in GitHub:"
echo -e "    Settings → Pages → Source: Deploy from branch"
echo -e "    Branch: main, Folder: ${CYAN}/docs${NC}"
echo ""
echo -e "  Your REPL will be at:"
echo -e "    ${CYAN}https://<username>.github.io/<repo>/${NC}"
echo ""