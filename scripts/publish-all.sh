#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: $(basename "$0")"
    echo
    echo "Publishes both the source code (to github.com/hotredman/numkit)"
    echo "and the Web IDE static distribution (to hotredman.github.io/numkit-demo)."
    exit 0
fi

echo "======================================================="
echo "  [1/3] Publishing Source Code to GitHub"
echo "======================================================="
"$SCRIPT_DIR/code-publish.sh"

echo
echo "======================================================="
echo "  [2/3] Deploying Web IDE Bundle to GitHub Pages"
echo "======================================================="
"$SCRIPT_DIR/web-publish.sh" --push

echo
echo "======================================================="
echo "  [3/3] Deploying Doxygen API Documentation"
echo "======================================================="
"$SCRIPT_DIR/doxy-publish.sh" --push

echo
echo "======================================================="
echo "  All published successfully to GitHub!"
echo "  Code: https://github.com/hotredman/numkit"
echo "  Demo: https://hotredman.github.io/numkit-demo/"
echo "  Docs: https://hotredman.github.io/numkit-doxy/"
echo "======================================================="
