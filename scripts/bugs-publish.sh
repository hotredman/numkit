#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}/.."
BUGS_HTML_DIR="${PROJECT_DIR}/build/bugs"

BUGS_REPO_DIR="${NUMKIT_BUGS_DIR:-}"
if [[ -z "${BUGS_REPO_DIR}" ]]; then
    if [[ -d "${PROJECT_DIR}/../../hotredman/numkit-bugs/.git" ]]; then
        BUGS_REPO_DIR="${PROJECT_DIR}/../../hotredman/numkit-bugs"
    elif [[ -d "${PROJECT_DIR}/../numkit-bugs/.git" ]]; then
        BUGS_REPO_DIR="${PROJECT_DIR}/../numkit-bugs"
    fi
fi

DO_PUSH=1
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push) DO_PUSH=1; shift ;;
        --no-push) DO_PUSH=0; shift ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --dest) BUGS_REPO_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--dest <dir>] [--push|--no-push] [--skip-build]"
            exit 0
            ;;
        *) BUGS_REPO_DIR="$1"; shift ;;
    esac
done

if [[ -z "${BUGS_REPO_DIR}" || ! -d "${BUGS_REPO_DIR}/.git" ]]; then
    echo "ERROR: Destination numkit-bugs git repository not found."
    exit 1
fi

echo "=== NumKit Bugs & Parity - Deploy Clean Mirror to GitHub Pages ==="
echo "Source: ${PROJECT_DIR}"
echo "Target: ${BUGS_REPO_DIR}"
echo ""

if [[ "${SKIP_BUILD}" -eq 0 || ! -f "${BUGS_HTML_DIR}/index.html" ]]; then
    echo "Generating NumKit Bugs & Parity site..."
    python3 "${PROJECT_DIR}/tools/build_bugs_site.py" --output "${BUGS_HTML_DIR}"
fi

SRC_REV=$(git -C "${PROJECT_DIR}" rev-parse --short HEAD 2>/dev/null || echo "manual")

echo "Syncing bugs catalog files to ${BUGS_REPO_DIR}..."

# Clean old files (except .git)
find "${BUGS_REPO_DIR}" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +

# Copy fresh files
cp -r "${BUGS_HTML_DIR}/." "${BUGS_REPO_DIR}/"
touch "${BUGS_REPO_DIR}/.nojekyll"

cd "${BUGS_REPO_DIR}"
echo ""
echo "Creating clean 1-commit state in Bugs repository..."
git checkout --orphan temp_deploy >/dev/null 2>&1 || git checkout -b temp_deploy
git add -A
git commit -m "docs(bugs): NumKit Defect & Parity Catalog (numkit@${SRC_REV})" >/dev/null 2>&1
git branch -D main >/dev/null 2>&1 || true
git branch -m main >/dev/null 2>&1 || true

if [[ "${DO_PUSH}" -eq 1 ]]; then
    echo ""
    echo "Force-pushing single clean commit to GitHub origin main..."
    git push -f origin main
    echo ""
    echo "Successfully published clean 1-commit mirror to GitHub Pages!"
else
    echo ""
    echo "Clean commit created locally (push skipped due to --no-push)."
fi
