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
| **GEMM** (`gemm`) | Real Double | 64 | 4.887 ms | 0.020 ms | 244.3× | S2 Bug Filed (Small-N pool overhead) |
| **GEMM** (`gemm`) | Real Double | 128 | 4.910 ms | 0.010 ms | 491.0× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 256 | 5.946 ms | 0.070 ms | 85.0× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 512 | 6.757 ms | 1.130 ms | 5.98× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 1024 | 10.387 ms | 5.460 ms | 1.90× | PASSED (Target ≤ 2.0×, 207 GFLOPS) |
| **GEMM** (`gemm`) | Real Double | 2048 | 49.693 ms | 28.02 ms | 1.77× | PASSED (346 GFLOPS, 11.6× thread speedup) |
| **LU Factorization** (`lu`) | Real Double | 64 | 0.065 ms | 0.010 ms | 6.50× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 128 | 4.711 ms | 0.060 ms | 78.5× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 256 | 16.150 ms | 0.200 ms | 80.8× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 512 | 46.980 ms | 1.320 ms | 35.6× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 1024 | 137.297 ms | 4.490 ms | 30.6× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.180 ms | 0.020 ms | 9.00× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 128 | 10.152 ms | 0.190 ms | 53.4× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 256 | 34.074 ms | 0.590 ms | 57.8× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 512 | 77.515 ms | 2.990 ms | 25.9× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 64 | 0.015 ms | 0.010 ms | 1.50× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 128 | 0.132 ms | 0.070 ms | 1.89× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 256 | 1.368 ms | 0.090 ms | 15.2× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 512 | 14.502 ms | 0.830 ms | 17.5× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 64 | 0.125 ms | 0.020 ms | 6.25× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 128 | 2.068 ms | 0.150 ms | 13.8× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 256 | 21.151 ms | 0.600 ms | 35.3× | S2 Bug Filed |
| **Linear Solve** (`linsolve`) | Real Double | 512 | 175.021 ms | 2.100 ms | 83.3× | S2 Bug Filed (Triage: unblocked scalar LU loop) |
| **Linear Solve** (`linsolve`) | Real Double | 1024 | 3348.9 ms | 8.360 ms | 400.6× | S2 Bug Filed (Triage: unblocked scalar LU loop) |

## Architecture & Design Specifications
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$).
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for`. Verified 100% bitwise deterministic across 24 worker threads (`GemmP2_BitwiseDeterminism` asserts active worker count > 1).
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.
