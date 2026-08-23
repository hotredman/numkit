#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS_HTML_DIR="$PROJECT_DIR/build/docs/html"

# Determine default doxygen repository directory
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

show_help() {
    echo "Usage: $(basename "$0") [--push | --no-push] [--skip-build] [--dest <path>] [<path>]"
    echo
    echo "Generates Doxygen API documentation and synchronizes it into a GitHub Pages repository."
    echo
    echo "Options:"
    echo "  --push        Automatically push commit to origin main (default: on)."
    echo "  --no-push     Commit locally without pushing to remote."
    echo "  --skip-build  Skip re-running doxygen if build/docs/html/ is already fresh."
    echo "  --dest <path> Destination directory (or set NUMKIT_DOXY_DIR environment variable)."
    echo "  -h, --help    Show this help message."
    exit 1
}

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
            show_help
            ;;
        *)
            DOXY_DIR="$1"
            shift
            ;;
    esac
done

if [[ -z "$DOXY_DIR" ]]; then
    echo "ERROR: Doxygen destination directory is not specified." >&2
    echo >&2
    show_help
fi

if [[ ! -d "$DOXY_DIR/.git" ]]; then
    echo "ERROR: Destination directory is not a Git repository: $DOXY_DIR" >&2
    echo >&2
    show_help
fi

echo "=== Numkit Doxygen - Deploy to GitHub Pages Repository ==="
echo "Source: $PROJECT_DIR"
echo "Target: $DOXY_DIR"
echo

# 1. Run Doxygen if needed
if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "Generating Doxygen documentation..."
    cd "$PROJECT_DIR"
    doxygen Doxyfile
fi

if [[ ! -f "$DOCS_HTML_DIR/index.html" ]]; then
    echo "ERROR: build/docs/html/index.html not found after Doxygen build!" >&2
    exit 1
fi

# 2. Get source git commit hash
SRC_REV="$(git -C "$PROJECT_DIR" rev-parse --short HEAD 2>/dev/null || echo "manual")"

echo
echo "Syncing documentation files to $DOXY_DIR..."

# Copy files into target repo using rsync
rsync -a --delete --exclude '.git' "$DOCS_HTML_DIR/" "$DOXY_DIR/"

# Ensure .nojekyll exists
touch "$DOXY_DIR/.nojekyll"

echo "Files synchronized."

# 3. Check git status in target repo
cd "$DOXY_DIR"
if [[ -z "$(git status --porcelain)" ]]; then
    echo
    echo "No changes detected in target repository. Target is already up to date."
    echo
    echo "=== Done ==="
    exit 0
fi

echo
echo "Committing changes in Doxygen repository..."
git add -A
git commit -m "docs: update Doxygen API documentation (numkit@$SRC_REV)"

if [[ "$DO_PUSH" -eq 1 ]]; then
    echo
    echo "Pushing to GitHub origin main..."
    git push origin main
    echo "Successfully deployed and pushed Doxygen docs to GitHub Pages!"
    echo
    echo "=== Done ==="
    exit 0
fi

echo
echo "Changes committed locally in: $DOXY_DIR"
echo "To push to GitHub, run:"
echo "  cd $DOXY_DIR"
echo "  git push origin main"
echo
echo "=== Done ==="
