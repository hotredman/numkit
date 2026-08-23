#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LOCAL_MIRROR=""
if [[ -d "$PROJECT_DIR/../../hotredman/numkit/.git" ]]; then
    LOCAL_MIRROR="$PROJECT_DIR/../../hotredman/numkit"
elif [[ -d "$PROJECT_DIR/../numkit-src/.git" ]]; then
    LOCAL_MIRROR="$PROJECT_DIR/../numkit-src"
fi

CODE_REMOTE="${NUMKIT_GITHUB_REMOTE:-}"
if [[ -z "$CODE_REMOTE" ]]; then
    CODE_REMOTE="$(git -C "$PROJECT_DIR" remote get-url github 2>/dev/null || true)"
fi
if [[ -z "$CODE_REMOTE" && -n "$LOCAL_MIRROR" ]]; then
    CODE_REMOTE="$(git -C "$LOCAL_MIRROR" remote get-url origin 2>/dev/null || true)"
fi
CODE_REMOTE="${CODE_REMOTE:-git@github.com:hotredman/numkit.git}"

show_help() {
    echo "Usage: $(basename "$0") [--remote <git_url>] [<git_url>]"
    echo
    echo "Pushes the main branch and tags from the primary repository to GitHub."
    echo
    echo "Options:"
    echo "  --remote <url>  GitHub repository URL (default: auto-detected or git@github.com:hotredman/numkit.git)."
    echo "  -h, --help      Show this help message."
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --remote)
            CODE_REMOTE="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            CODE_REMOTE="$1"
            shift
            ;;
    esac
done

echo "=== Numkit - Publish Source Code to GitHub ==="
echo "Source: $PROJECT_DIR"
echo "GitHub Remote: $CODE_REMOTE"
echo

cd "$PROJECT_DIR"

if git remote get-url github >/dev/null 2>&1; then
    git remote set-url github "$CODE_REMOTE"
else
    echo "Adding remote \"github\" -> $CODE_REMOTE"
    git remote add github "$CODE_REMOTE"
fi

echo
echo "Pushing main branch to GitHub..."
git push github main

echo
echo "Pushing tags to GitHub..."
git push github --tags 2>/dev/null || true

if [[ -n "$LOCAL_MIRROR" && -d "$LOCAL_MIRROR/.git" ]]; then
    echo
    echo "Syncing local mirror clone ($LOCAL_MIRROR)..."
    git -C "$LOCAL_MIRROR" pull origin main 2>/dev/null || true
fi

echo
echo "=== Source code successfully published to GitHub! ==="
