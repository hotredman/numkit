#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

OUT_DIR="$PROJECT_DIR/build/pvs"
COMPILE_COMMANDS="$PROJECT_DIR/build/linux/release/compile_commands.json"
LOG_PATH="$OUT_DIR/nk.log"
JSON_PATH="$OUT_DIR/nk.log.json"
INPUT_JSON=""
DO_COMMIT=0

show_help() {
    echo "Usage: $(basename "$0") [options]"
    echo
    echo "Runs PVS-Studio static analysis and updates bugs/pvs/ reports."
    echo
    echo "Options:"
    echo "  --commit               Automatically commit the generated report to git (if working tree clean)"
    echo "  --input <file>         Process an existing PVS JSON report instead of running analyzer"
    echo "  --compile-commands <f> Path to compile_commands.json"
    echo "  -h, --help             Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --commit)
            DO_COMMIT=1
            shift
            ;;
        --input)
            INPUT_JSON="$2"
            shift 2
            ;;
        --compile-commands)
            COMPILE_COMMANDS="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "Unknown argument: $1" >&2
            show_help
            ;;
    esac
done

if [[ -n "$INPUT_JSON" ]]; then
    echo "Using provided input report: $INPUT_JSON"
else
    if ! command -v pvs-studio-analyzer &>/dev/null; then
        echo "ERROR: pvs-studio-analyzer not found on PATH." >&2
        echo "       Install PVS-Studio or pass --input <report.json>." >&2
        exit 1
    fi
    if ! command -v plog-converter &>/dev/null; then
        echo "ERROR: plog-converter not found on PATH." >&2
        exit 1
    fi

    if [[ ! -f "$COMPILE_COMMANDS" ]]; then
        echo "Configuring linux-release preset..."
        cmake --preset linux-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    fi

    mkdir -p "$OUT_DIR"
    rm -f "$LOG_PATH" "$JSON_PATH"

    echo "======================================================="
    echo "  Running PVS-Studio Analysis"
    echo "======================================================="
    pvs-studio-analyzer analyze \
        -f "$COMPILE_COMMANDS" \
        -o "$LOG_PATH" \
        --cfg "$PROJECT_DIR/.pvsconfig" \
        --intermodular || true

    if [[ ! -f "$LOG_PATH" ]]; then
        echo "ERROR: Analysis log $LOG_PATH was not created!" >&2
        exit 1
    fi

    echo "Converting log to JSON..."
    plog-converter -t json -m cwe -o "$OUT_DIR" -r "$PROJECT_DIR" "$LOG_PATH"
    INPUT_JSON="$JSON_PATH"
fi

COMMIT_FLAG=""
if [[ "$DO_COMMIT" -eq 1 ]]; then
    COMMIT_FLAG="--commit"
fi

python3 tools/pvs_report_summary.py --input "$INPUT_JSON" $COMMIT_FLAG
