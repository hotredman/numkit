#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPLOY_DIR="$PROJECT_DIR/deploy"

# Determine default pages directory
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
    elif [[ -d "$PROJECT_DIR/../../czssgkavo/numkit/.git" ]]; then
        PAGES_DIR="$PROJECT_DIR/../../czssgkavo/numkit"
    fi
fi

DO_PUSH=0
SKIP_BUILD=0

show_help() {
    echo "Usage: $(basename "$0") [--push] [--skip-build] [--dest <path>] [<path>]"
    echo
    echo "Synchronizes the static Web IDE bundle (deploy/) into a GitHub Pages repository."
    echo
    echo "Options:"
    echo "  --push        Automatically push commit to origin main in the Pages repo."
    echo "  --skip-build  Skip re-running web-build.sh if deploy/ is already fresh."
    echo "  --dest <path> Destination directory (or set NUMKIT_PAGES_DIR environment variable)."
    echo "  -h, --help    Show this help message."
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push)
            DO_PUSH=1
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
            show_help
            ;;
        *)
            PAGES_DIR="$1"
            shift
            ;;
    esac
done

if [[ -z "$PAGES_DIR" ]]; then
    echo "ERROR: GitHub Pages destination directory is not specified."
    echo
    show_help
fi

if [[ ! -d "$PAGES_DIR/.git" ]]; then
    echo "ERROR: Destination directory is not a Git repository: $PAGES_DIR"
    echo
    show_help
fi

echo "=== Numkit Web IDE - Deploy to GitHub Pages Repository ==="
echo "Source: $PROJECT_DIR"
echo "Target: $PAGES_DIR"
echo

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "Building latest Web IDE static bundle..."
    "$SCRIPT_DIR/web-build.sh"
fi

if [[ ! -f "$DEPLOY_DIR/index.html" ]]; then
    echo "ERROR: deploy/index.html not found!"
    exit 1
fi

SRC_REV="$(git -C "$PROJECT_DIR" rev-parse --short HEAD 2>/dev/null || echo "manual")"

echo "Syncing deploy artifacts to $PAGES_DIR..."
rm -rf "$PAGES_DIR/assets" "$PAGES_DIR/examples"
cp -r "$DEPLOY_DIR/"* "$PAGES_DIR/"
touch "$PAGES_DIR/.nojekyll"

cd "$PAGES_DIR"
if [[ -z "$(git status --porcelain)" ]]; then
    echo "No changes detected in target repository. Target is already up to date."
    echo
    echo "=== Done ==="
    exit 0
fi

echo "Committing changes in Pages repository..."
git add -A
git commit -m "Update Web IDE build (numkit@$SRC_REV)"

if [[ "$DO_PUSH" -eq 1 ]]; then
    echo "Pushing to GitHub origin main..."
    git push origin main
    echo "Successfully deployed and pushed to GitHub Pages!"
else
    echo
    echo "Changes committed locally in: $PAGES_DIR"
    echo "To push to GitHub, run: cd '$PAGES_DIR' && git push origin main"
    echo "Or pass --push next time: ./scripts/web-publish.sh --push"
fi

echo
echo "=== Done ==="
