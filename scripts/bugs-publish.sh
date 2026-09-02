#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}/.."
BUGS_HTML_DIR="${PROJECT_DIR}/build/bugs"
DEPLOY_GIT_DIR="${PROJECT_DIR}/build/deploy-bugs"

REPO_URL="${NUMKIT_BUGS_REPO:-git@github.com:hotredman/numkit-bugs.git}"
DO_PUSH=1
SKIP_BUILD=0
DEST_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push) DO_PUSH=1; shift ;;
        --no-push) DO_PUSH=0; shift ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --repo) REPO_URL="$2"; shift 2 ;;
        --dest) DEST_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--push|--no-push] [--repo <url>] [--dest <dir>] [--skip-build]"
            exit 0
            ;;
        *) DEST_DIR="$1"; shift ;;
    esac
done

echo "=== NumKit Bugs & Parity - Deploy to GitHub Pages ==="
echo "Source: ${PROJECT_DIR}"
if [[ -n "${DEST_DIR}" ]]; then
    echo "Target Local: ${DEST_DIR}"
    TARGET_DIR="${DEST_DIR}"
else
    echo "Target Remote: ${REPO_URL}"
    echo "Workspace: ${DEPLOY_GIT_DIR}"
    TARGET_DIR="${DEPLOY_GIT_DIR}"
fi
echo ""

if [[ "${SKIP_BUILD}" -eq 0 || ! -f "${BUGS_HTML_DIR}/index.html" ]]; then
    echo "Generating NumKit Bugs & Parity site..."
    python3 "${PROJECT_DIR}/tools/build_bugs_site.py" --output "${BUGS_HTML_DIR}"
fi

SRC_REV=$(git -C "${PROJECT_DIR}" rev-parse --short HEAD 2>/dev/null || echo "manual")

if [[ -z "${DEST_DIR}" ]]; then
    mkdir -p "${DEPLOY_GIT_DIR}"
    cd "${DEPLOY_GIT_DIR}"
    if [[ ! -d ".git" ]]; then
        git init -b main >/dev/null 2>&1
        git remote add origin "${REPO_URL}" >/dev/null 2>&1
    else
        git remote set-url origin "${REPO_URL}" >/dev/null 2>&1
    fi
fi

echo "Syncing bugs catalog files to deploy workspace..."
find "${TARGET_DIR}" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +
cp -r "${BUGS_HTML_DIR}/." "${TARGET_DIR}/"
touch "${TARGET_DIR}/.nojekyll"

cd "${TARGET_DIR}"
echo ""
echo "Creating clean 1-commit state..."
git checkout --orphan temp_deploy >/dev/null 2>&1 || git checkout -b temp_deploy >/dev/null 2>&1
git add -A
git commit -m "docs(bugs): NumKit Defect & Parity Catalog (numkit@${SRC_REV})" >/dev/null 2>&1
git branch -D main >/dev/null 2>&1 || true
git branch -m main >/dev/null 2>&1 || true

if [[ "${DO_PUSH}" -eq 1 ]]; then
    echo ""
    echo "Force-pushing single clean commit to ${REPO_URL}..."
    git push -f origin main
    echo ""
    echo "Successfully published clean 1-commit mirror to GitHub Pages!"
else
    echo ""
    echo "Clean commit created locally in ${TARGET_DIR} (push skipped due to --no-push)."
fi
