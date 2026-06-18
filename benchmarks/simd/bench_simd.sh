#!/usr/bin/env bash
# benchmarks/simd/bench_simd.sh — SIMD speedup runner (POSIX / git-bash)
#
# Same as bench_simd.bat. Runs the SIMD-optimised kernels on the
# scalar-baseline build (preset=bench) and the Highway-SIMD build
# (preset=bench-simd), then side-by-sides the numbers via Google
# Benchmark's compare.py.
#
# Kernels covered — every numkit SIMD family with a portable-vs-Highway split:
#   unary math   abs sin cos tan exp log sqrt round floor ceil fix isfinite
#   binary       plus minus times rdivide mod ; compares eq ne lt gt le ge
#   matmul / fft mtimes ; fft
#   reductions   any all var std cumsum
#   fused        the ops/fused one-pass kernels (BM_Fused*)
#
# Prereq:
#   cmake --preset=bench      && cmake --build --preset=bench
#   cmake --preset=bench-simd && cmake --build --preset=bench-simd

set -eu

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/../.." && pwd)"   # benchmarks/simd/ -> repo root
FILTER='BM_Abs|BM_Sin|BM_Cos|BM_Tan|BM_Exp|BM_Log|BM_Sqrt|BM_Round|BM_Floor|BM_Ceil|BM_Fix|BM_Isfinite|BM_Plus|BM_Minus|BM_Times|BM_Rdivide|BM_Mod/|BM_Lt|BM_Gt|BM_Le|BM_Ge|BM_Eq|BM_Ne|BM_Mtimes|BM_Fft_PowerOfTwo|BM_Cumsum|BM_Any|BM_All|BM_Var|BM_Std|BM_Fused'
MIN_TIME='0.2s'

# On Windows the VS generator nests exes under Release/. On Ninja/Linux
# the exe sits directly in the bench dir. Pick whichever exists.
find_exe() {
    local base="$1"
    for p in "$base/benchmarks/Release/numkit_bench.exe" \
             "$base/benchmarks/numkit_bench.exe" \
             "$base/benchmarks/numkit_bench"; do
        [ -x "$p" ] && { printf '%s' "$p"; return 0; }
    done
    return 1
}

PORTABLE="$(find_exe "$PROJECT_DIR/build/bench")" || {
    echo "Portable bench exe not found under $PROJECT_DIR/build/bench/benchmarks/"
    echo "Build the scalar baseline first:"
    echo "  cmake --preset=bench && cmake --build --preset=bench"
    exit 1
}
SIMD="$(find_exe "$PROJECT_DIR/build/bench-simd")" || {
    echo "SIMD bench exe not found under $PROJECT_DIR/build/bench-simd/benchmarks/"
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
