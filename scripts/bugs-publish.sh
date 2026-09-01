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

echo "=== NumKit Bugs & Parity - Deploy to GitHub Pages Repository ==="
echo "Source: ${PROJECT_DIR}"
echo "Target: ${BUGS_REPO_DIR}"
echo ""

if [[ "${SKIP_BUILD}" -eq 0 || ! -f "${BUGS_HTML_DIR}/index.html" ]]; then
    echo "Generating NumKit Bugs & Parity site..."
    python3 "${PROJECT_DIR}/tools/build_bugs_site.py" --output "${BUGS_HTML_DIR}"
fi

SRC_REV=$(git -C "${PROJECT_DIR}" rev-parse --short HEAD 2>/dev/null || echo "manual")

echo "Syncing bugs catalog files to ${BUGS_REPO_DIR}..."
cp -r "${BUGS_HTML_DIR}/." "${BUGS_REPO_DIR}/"
touch "${BUGS_REPO_DIR}/.nojekyll"

cd "${BUGS_REPO_DIR}"
if [[ -z $(git status --porcelain) ]]; then
    echo "No changes detected in target repository. Target is already up to date."
    exit 0
fi

echo "Committing changes in Bugs repository..."
git add -A
git commit -m "docs(bugs): update parity and bug catalog (numkit@${SRC_REV})"

if [[ "${DO_PUSH}" -eq 1 ]]; then
    echo "Pushing to GitHub origin main..."
    git push origin main
    echo "Successfully deployed and pushed Bugs catalog to GitHub Pages!"
fi
