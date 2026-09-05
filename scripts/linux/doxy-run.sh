#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DOCS_HTML_DIR="$PROJECT_DIR/build/docs/html"
PORT=8080
SKIP_BUILD=0

show_help() {
    echo "Usage: $(basename "$0") [--skip-build] [--port <number>]"
    echo
    echo "Builds (if needed) and serves Doxygen documentation locally via HTTP server."
    echo
    echo "Options:"
    echo "  --skip-build  Do not rebuild documentation, serve existing build/docs/html."
    echo "  --port <num>  HTTP port to listen on (default: 8080)."
    echo "  -h, --help    Show this help message."
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            shift
            ;;
    esac
done

if [[ ! -f "$DOCS_HTML_DIR/index.html" ]]; then
    SKIP_BUILD=0
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "Building Doxygen documentation..."
    cd "$PROJECT_DIR"
    doxygen Doxyfile
fi

echo
echo "========================================================"
echo "  Numkit Doxygen Documentation Local Server"
echo "  URL:     http://localhost:$PORT/"
echo "  Serving: $DOCS_HTML_DIR"
echo "========================================================"
echo "Press Ctrl+C to stop the server."
echo

if command -v xdg-open &>/dev/null; then
    xdg-open "http://localhost:$PORT/" &>/dev/null &
fi

python3 -m http.server "$PORT" --directory "$DOCS_HTML_DIR"
