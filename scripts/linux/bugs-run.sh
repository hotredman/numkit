#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUGS_HTML_DIR="${PROJECT_DIR}/build/bugs"
PORT="${PORT:-8081}"
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1; shift ;;
        --port) PORT="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--skip-build] [--port <number>]"
            exit 0
            ;;
        *) shift ;;
    esac
done

if [[ ! -f "${BUGS_HTML_DIR}/index.html" || "${SKIP_BUILD}" -eq 0 ]]; then
    echo "Building NumKit Bugs & Parity site..."
    python3 "${PROJECT_DIR}/tools/build_bugs_site.py" --output "${BUGS_HTML_DIR}"
fi

echo
echo "========================================================"
echo "  NumKit Bugs & Parity Documentation Local Server"
echo "  URL:     http://localhost:${PORT}/"
echo "  Serving: ${BUGS_HTML_DIR}"
echo "========================================================"
echo "Press Ctrl+C to stop the server."
echo

if command -v xdg-open &>/dev/null; then
    xdg-open "http://localhost:$PORT/" &>/dev/null &
fi

python3 -m http.server "${PORT}" --directory "${BUGS_HTML_DIR}"
