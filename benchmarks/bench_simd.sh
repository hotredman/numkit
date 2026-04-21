#!/usr/bin/env bash
# benchmarks/bench_simd.sh — SIMD speedup runner (POSIX / git-bash)
#
# Same as bench_simd.bat. Runs the SIMD-optimised kernels on the
# scalar-baseline build (preset=bench) and the Highway-SIMD build
# (preset=bench-simd), then side-by-sides the numbers via Google
# Benchmark's compare.py.
#
# Kernels covered (Phase 8a-e):
#   BM_Abs                    abs                 (8a)
#   BM_Sin / Cos / Exp / Log  sin cos exp log     (8b)
#   BM_Plus / Times           plus times          (8c)
#   BM_Mtimes_Square          mtimes / matmul     (8d)
#   BM_Fft_PowerOfTwo         fft                 (8e)
#
# Prereq:
#   cmake --preset=bench      && cmake --build --preset=bench
#   cmake --preset=bench-simd && cmake --build --preset=bench-simd

set -eu

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
FILTER='BM_Abs|BM_Sin|BM_Cos|BM_Exp|BM_Log|BM_Plus|BM_Times|BM_Mtimes_Square|BM_Fft_PowerOfTwo'
MIN_TIME='0.2s'

# On Windows the VS generator nests exes under Release/. On Ninja/Linux
# the exe sits directly in the bench dir. Pick whichever exists.
find_exe() {
    local base="$1"
    for p in "$base/benchmarks/Release/m_bench.exe" \
             "$base/benchmarks/m_bench.exe" \
             "$base/benchmarks/m_bench"; do
        [ -x "$p" ] && { printf '%s' "$p"; return 0; }
    done
    return 1
}

PORTABLE="$(find_exe "$PROJECT_DIR/build-bench")" || {
    echo "Portable bench exe not found under $PROJECT_DIR/build-bench/benchmarks/"
    echo "Build the scalar baseline first:"
    echo "  cmake --preset=bench && cmake --build --preset=bench"
    exit 1
}
SIMD="$(find_exe "$PROJECT_DIR/build-bench-simd")" || {
    echo "SIMD bench exe not found under $PROJECT_DIR/build-bench-simd/benchmarks/"
    echo "Build the Highway-SIMD variant first:"
    echo "  cmake --preset=bench-simd && cmake --build --preset=bench-simd"
    exit 1
}

COMPARE="$SCRIPT_DIR/compare_simd.py"
BASELINE_JSON="$SCRIPT_DIR/baseline.json"
SIMD_JSON="$SCRIPT_DIR/simd.json"

echo
echo "[1/2] Portable (scalar baseline)..."
"$PORTABLE" --benchmark_filter="$FILTER" --benchmark_min_time=$MIN_TIME \
            --benchmark_out="$BASELINE_JSON" --benchmark_out_format=json \
            --benchmark_format=console

echo
echo "[2/2] Desktop-fast (Highway SIMD)..."
"$SIMD"     --benchmark_filter="$FILTER" --benchmark_min_time=$MIN_TIME \
            --benchmark_out="$SIMD_JSON" --benchmark_out_format=json \
            --benchmark_format=console

echo
if ! command -v python >/dev/null 2>&1 && ! command -v python3 >/dev/null 2>&1; then
    echo "Python not on PATH — skipping compare.py."
    echo "Raw JSON results saved next to this script:"
    echo "  $BASELINE_JSON"
    echo "  $SIMD_JSON"
    exit 0
fi

PY="$(command -v python || command -v python3)"
echo "=== Speedup table (portable vs. desktop-fast) ==="
"$PY" "$COMPARE" "$BASELINE_JSON" "$SIMD_JSON"
