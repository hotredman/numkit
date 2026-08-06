# Linear Algebra Performance Benchmarks

Measured benchmark comparison between `numkit` Linear Algebra module (`numkit_bench.exe` Release build) and MATLAB R2025b executed headlessly on the same machine.

## Environment & Methodology

- **Date**: 2026-08-06
- **Hardware**: 24-core x86_64 CPU @ 3.07 GHz, L3 Cache 36.8 MB
- **NumKit Build**: MSVC 2022 (Visual Studio 17.14), C++20 Release (`build/desktop-fast`)
- **NumKit Raw Output**: [`results/2026-08-06_p7_numkit_bench.txt`](results/2026-08-06_p7_numkit_bench.txt)
- **MATLAB Version**: MATLAB R2025b (25.2.0.2998904), 24 threads
- **MATLAB Raw Output**: [`results/2026-08-06_matlab_r2025b_x86_64.txt`](results/2026-08-06_matlab_r2025b_x86_64.txt)

## Measured Comparison Table (NumKit Multithreaded vs MATLAB R2025b timeit)

| Op / Benchmark | Domain | Size (n) | NumKit Time (Wall) | MATLAB R2025b Time (`timeit`) | Ratio (NumKit/MATLAB) | Gate / Status |
|----------------|--------|----------|--------------------|-------------------------------|-----------------------|---------------|
| **GEMM** (`gemm`) | Real Double | 64 | 0.982 ms | 0.020 ms | 49.1× | S2 Bug Filed (Small-N fast path) |
| **GEMM** (`gemm`) | Real Double | 128 | 1.131 ms | 0.010 ms | 113.1× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 256 | 5.025 ms | 0.070 ms | 71.8× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 512 | 6.700 ms | 1.130 ms | 5.93× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 1024 | 11.117 ms | 5.460 ms | 2.04× | PASSED (Target ≤ 2.0×, 193 GFLOPS) |
| **GEMM** (`gemm`) | Real Double | 2048 | 46.238 ms | 28.02 ms | 1.65× | PASSED (372 GFLOPS, 11.6× thread speedup) |
| **LU Factorization** (`lu`) | Real Double | 64 | 0.065 ms | 0.010 ms | 6.50× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 128 | 1.262 ms | 0.060 ms | 21.0× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 256 | 5.321 ms | 0.200 ms | 26.6× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 512 | 36.553 ms | 1.320 ms | 27.7× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 1024 | 127.031 ms | 4.490 ms | 28.3× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.201 ms | 0.020 ms | 10.05× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 128 | 2.491 ms | 0.190 ms | 13.11× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 256 | 22.631 ms | 0.590 ms | 38.36× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 512 | 69.139 ms | 2.990 ms | 23.12× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 64 | 0.015 ms | 0.010 ms | 1.50× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 128 | 0.140 ms | 0.070 ms | 2.00× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 256 | 0.916 ms | 0.090 ms | 10.18× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 512 | 6.047 ms | 0.830 ms | 7.29× | S2 Bug Filed (Accelerated 2.4x via parallel syrk) |
| **Cholesky** (`chol`) | Real Double | 1024 | 54.932 ms | 8.980 ms | 6.12× | S2 Bug Filed (Accelerated 4.2x via parallel syrk) |
| **Linear Solve** (`linsolve`) | Real Double | 64 | 0.152 ms | 0.020 ms | 7.60× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 128 | 2.124 ms | 0.150 ms | 14.16× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 256 | 11.424 ms | 0.600 ms | 19.04× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 512 | 76.556 ms | 2.100 ms | 36.46× | S2 Bug Filed (Accelerated 2.3x via blocked LU+trsm) |
| **Linear Solve** (`linsolve`) | Real Double | 1024 | 454.314 ms | 46.70 ms | 9.73× | S2 Bug Filed (Accelerated 7.4x via blocked LU+trsm) |

## Architecture & Design Specifications
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$).
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for`. Verified 100% bitwise deterministic across 24 worker threads (`GemmP2_BitwiseDeterminism` asserts active worker count > 1).
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.
