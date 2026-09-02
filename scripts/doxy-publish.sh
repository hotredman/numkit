#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS_HTML_DIR="$PROJECT_DIR/build/docs/html"

DOXY_DIR="${NUMKIT_DOXY_DIR:-}"
if [[ -z "$DOXY_DIR" ]]; then
    if [[ -d "$PROJECT_DIR/../../hotredman/numkit-doxy/.git" ]]; then
        DOXY_DIR="$PROJECT_DIR/../../hotredman/numkit-doxy"
    elif [[ -d "$PROJECT_DIR/../numkit-doxy/.git" ]]; then
        DOXY_DIR="$PROJECT_DIR/../numkit-doxy"
    elif [[ -d "/home/user/projects/hotredman/numkit-doxy/.git" ]]; then
        DOXY_DIR="/home/user/projects/hotredman/numkit-doxy"
    fi
fi

DO_PUSH=1
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push)
            DO_PUSH=1
            shift
            ;;
        --no-push)
            DO_PUSH=0
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --dest)
            DOXY_DIR="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $(basename "$0") [--push | --no-push] [--skip-build] [--dest <path>] [<path>]"
            exit 0
            ;;
        *)
            DOXY_DIR="$1"
            shift
            ;;
    esac
done

if [[ -z "$DOXY_DIR" || ! -d "$DOXY_DIR/.git" ]]; then
    echo "ERROR: Destination numkit-doxy git repository not found: $DOXY_DIR" >&2
    exit 1
fi

echo "=== NumKit Doxygen - Deploy Clean Mirror to GitHub Pages ==="
echo "Source: $PROJECT_DIR"
echo "Target: $DOXY_DIR"
echo

if [[ "$SKIP_BUILD" -eq 0 || ! -f "$DOCS_HTML_DIR/index.html" ]]; then
    echo "Generating Doxygen documentation..."
    cd "$PROJECT_DIR"
    doxygen Doxyfile
fi

SRC_REV="$(git -C "$PROJECT_DIR" rev-parse --short HEAD 2>/dev/null || echo "manual")"

echo
echo "Syncing documentation files to $DOXY_DIR..."

# Clean old files (except .git)
find "$DOXY_DIR" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +

# Copy fresh files
cp -r "$DOCS_HTML_DIR/." "$DOXY_DIR/"
touch "$DOXY_DIR/.nojekyll"

cd "$DOXY_DIR"
echo
echo "Creating clean 1-commit state in Doxygen repository..."
git checkout --orphan temp_deploy >/dev/null 2>&1 || git checkout -b temp_deploy
git add -A
git commit -m "docs: NumKit C++ API Documentation (numkit@$SRC_REV)" >/dev/null 2>&1
git branch -D main >/dev/null 2>&1 || true
git branch -m main >/dev/null 2>&1 || true

if [[ "$DO_PUSH" -eq 1 ]]; then
    echo
    echo "Force-pushing single clean commit to GitHub origin main..."
    git push -f origin main
    echo
    echo "Successfully published clean 1-commit Doxygen docs to GitHub Pages!"
else
    echo
    echo "Clean commit created locally (push skipped due to --no-push)."
fi
