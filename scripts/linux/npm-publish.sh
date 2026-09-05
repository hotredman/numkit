#!/usr/bin/env bash
# Publish packages/numkit (the WASM CLI) to npm — manual flow.
#
#   scripts/linux/npm-publish.sh [--skip-build] [--dry-run]
#
# Steps: rebuild WASM (web-build.sh) → refresh dist/ → npm test → npm publish.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PKG_DIR="$PROJECT_DIR/packages/numkit"

SKIP_BUILD=0
DRY_RUN=0
while [ $# -gt 0 ]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1 ;;
        --dry-run)    DRY_RUN=1 ;;
        -h|--help)
            sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "Unknown option: $1 (use -h for help)" >&2; exit 1 ;;
    esac
    shift
done

echo "=== Numkit - Publish npm package (manual) ==="
echo "Package: $PKG_DIR"
[ "$DRY_RUN" = 1 ] && echo "Mode: DRY RUN (no upload to the registry)"
echo

command -v node >/dev/null 2>&1 || { echo "ERROR: Node.js not found." >&2; exit 1; }
command -v npm  >/dev/null 2>&1 || { echo "ERROR: npm not found on PATH." >&2; exit 1; }

if [ "$SKIP_BUILD" = 0 ]; then
    echo "=== Step 1/4: Rebuilding the WASM engine ==="
    "$SCRIPT_DIR/web-build.sh"
else
    echo "=== Step 1/4: Skipping WASM build (--skip-build)"
fi

echo
echo "=== Step 2/4: Refreshing package dist/ from build output ==="
cd "$PKG_DIR"
node scripts/refresh-dist.js

echo
echo "=== Step 3/4: Smoke test (inline MATLAB eval through the CLI) ==="
npm test

echo
echo "=== Step 4/4: Pack preview ==="
npm pack --dry-run

if [ "$DRY_RUN" = 1 ]; then
    echo
    echo "=== Dry run complete - nothing was published. ==="
    echo "Re-run without --dry-run to upload to the npm registry."
    exit 0
fi

echo
npm whoami >/dev/null 2>&1 || {
    echo "ERROR: Not logged in to npm. Run:  npm login" >&2
    exit 1
}

echo "Publishing to the npm registry..."
npm publish --access public

echo
PKG_VERSION=$(node -p "require('./package.json').version")
echo "=== Published numkit@${PKG_VERSION} ==="
echo "Verify: https://www.npmjs.com/package/numkit"
echo "Test:   npx numkit -e \"disp(1+1)\""
