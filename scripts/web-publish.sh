#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPLOY_DIR="$PROJECT_DIR/deploy"

PAGES_DIR="${NUMKIT_PAGES_DIR:-}"
if [[ -z "$PAGES_DIR" ]]; then
    if [[ -d "$PROJECT_DIR/../../hotredman/numkit-demo/.git" ]]; then
        PAGES_DIR="$PROJECT_DIR/../../hotredman/numkit-demo"
    elif [[ -d "$PROJECT_DIR/../numkit-demo/.git" ]]; then
        PAGES_DIR="$PROJECT_DIR/../numkit-demo"
    elif [[ -d "$PROJECT_DIR/../numkit-pages/.git" ]]; then
        PAGES_DIR="$PROJECT_DIR/../numkit-pages"
    elif [[ -d "$PROJECT_DIR/../numkit-web/.git" ]]; then
        PAGES_DIR="$PROJECT_DIR/../numkit-web"
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
            PAGES_DIR="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $(basename "$0") [--push | --no-push] [--skip-build] [--dest <path>] [<path>]"
            exit 0
            ;;
        *)
            PAGES_DIR="$1"
            shift
            ;;
    esac
done

if [[ -z "$PAGES_DIR" || ! -d "$PAGES_DIR/.git" ]]; then
    echo "ERROR: Destination numkit-demo git repository not found: $PAGES_DIR" >&2
    exit 1
fi

echo "=== NumKit Web IDE Demo - Deploy Clean Mirror to GitHub Pages ==="
echo "Source: $PROJECT_DIR"
echo "Target: $PAGES_DIR"
echo

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "Building latest Web IDE static bundle..."
    "$SCRIPT_DIR/web-build.sh"
fi

if [[ ! -f "$DEPLOY_DIR/index.html" ]]; then
    echo "ERROR: deploy/index.html not found!" >&2
    exit 1
fi

SRC_REV="$(git -C "$PROJECT_DIR" rev-parse --short HEAD 2>/dev/null || echo "manual")"

echo "Syncing deploy artifacts to $PAGES_DIR..."

# Clean old files (except .git)
find "$PAGES_DIR" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +

# Copy fresh files
cp -r "$DEPLOY_DIR/"* "$PAGES_DIR/"
touch "$PAGES_DIR/.nojekyll"

cd "$PAGES_DIR"
echo
echo "Creating clean 1-commit state in Demo repository..."
git checkout --orphan temp_deploy >/dev/null 2>&1 || git checkout -b temp_deploy
git add -A
git commit -m "deploy(demo): NumKit Web IDE Demo (numkit@$SRC_REV)" >/dev/null 2>&1
git branch -D main >/dev/null 2>&1 || true
git branch -m main >/dev/null 2>&1 || true

if [[ "$DO_PUSH" -eq 1 ]]; then
    echo
    echo "Force-pushing single clean commit to GitHub origin main..."
    git push -f origin main
    echo
    echo "Successfully published clean 1-commit Web IDE Demo to GitHub Pages!"
else
    echo
    echo "Clean commit created locally (push skipped due to --no-push)."
fi
