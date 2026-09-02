#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: $(basename "$0")"
    echo
    echo "Publishes source code, Web IDE demo, Doxygen docs, and Defect catalog to GitHub."
    exit 0
fi

echo "======================================================="
echo "  [1/4] Publishing Source Code to GitHub"
echo "======================================================="
"$SCRIPT_DIR/github-push.sh"

echo
echo "======================================================="
echo "  [2/4] Deploying Web IDE Demo to GitHub Pages"
echo "======================================================="
"$SCRIPT_DIR/web-publish.sh"

echo
echo "======================================================="
echo "  [3/4] Deploying Doxygen API Documentation"
echo "======================================================="
"$SCRIPT_DIR/doxy-publish.sh"

echo
echo "======================================================="
echo "  [4/4] Deploying Defect & Parity Catalog"
echo "======================================================="
"$SCRIPT_DIR/bugs-publish.sh"

echo
echo "======================================================="
echo "  All published successfully to GitHub!"
echo "  Code: https://github.com/hotredman/numkit"
echo "  Demo: https://hotredman.github.io/numkit-demo/"
echo "  Docs: https://hotredman.github.io/numkit-doxy/"
echo "  Bugs: https://hotredman.github.io/numkit-bugs/"
echo "======================================================="
