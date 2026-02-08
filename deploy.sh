#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-wasm"
DIST_DIR="${BUILD_DIR}/repl/dist"
PAGES_DIR="${PROJECT_DIR}/docs"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()   { echo -e "${GREEN}[OK]${NC}    $*"; }

# Собираем если ещё не собрано
if [ ! -f "${DIST_DIR}/mlab_repl.wasm" ]; then
    info "Building first..."
    bash "${PROJECT_DIR}/build.sh"
fi

# Копируем в docs/ для GitHub Pages
info "Copying to docs/ ..."
rm -rf "${PAGES_DIR}"
mkdir -p "${PAGES_DIR}"
cp "${DIST_DIR}"/* "${PAGES_DIR}/"

ok "Files copied to docs/"

echo ""
echo -e "${GREEN}Done!${NC} Now run:"
echo ""
echo -e "  ${CYAN}git add docs/${NC}"
echo -e "  ${CYAN}git commit -m 'Deploy MLab REPL to GitHub Pages'${NC}"
echo -e "  ${CYAN}git push${NC}"
echo ""
echo "Then go to:"
echo "  GitHub → Settings → Pages → Source: Deploy from branch"
echo "  Branch: main, Folder: /docs"
echo ""
echo "Your REPL will be at:"
echo -e "  ${CYAN}https://<username>.github.io/<repo>/${NC}"
echo ""
