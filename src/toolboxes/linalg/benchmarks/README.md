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
| **GEMM** (`gemm`) | Real Double | 64 | 0.931 ms | 0.020 ms | 46.5× | S2 Bug Filed (Small-N pool overhead) |
| **GEMM** (`gemm`) | Real Double | 128 | 0.320 ms | 0.010 ms | 32.0× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 256 | 0.759 ms | 0.070 ms | 10.8× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 512 | 1.979 ms | 1.130 ms | 1.75× | PASSED (Target ≤ 2.0×, 135 GFLOPS) |
| **GEMM** (`gemm`) | Real Double | 1024 | 6.966 ms | 5.460 ms | 1.28× | PASSED (Target ≤ 1.3×, 308 GFLOPS) |
| **GEMM** (`gemm`) | Real Double | 2048 | 41.26 ms | 28.02 ms | 1.47× | PASSED (416 GFLOPS, 11.6× thread speedup) |
| **LU Factorization** (`lu`) | Real Double | 64 | 0.042 ms | 0.010 ms | 4.20× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 128 | 1.048 ms | 0.060 ms | 17.5× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 256 | 4.773 ms | 0.200 ms | 23.9× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 512 | 20.78 ms | 1.320 ms | 15.7× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 1024 | 83.61 ms | 4.490 ms | 18.6× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.152 ms | 0.020 ms | 7.60× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 128 | 1.594 ms | 0.190 ms | 8.39× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 256 | 6.399 ms | 0.590 ms | 10.8× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 512 | 26.49 ms | 2.990 ms | 8.86× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 64 | 0.015 ms | 0.010 ms | 1.50× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 128 | 0.131 ms | 0.070 ms | 1.87× | PASSED (Target ≤ 2.0×) |
| **Cholesky** (`chol`) | Real Double | 256 | 1.385 ms | 0.090 ms | 15.4× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 512 | 13.65 ms | 0.830 ms | 16.4× | S2 Bug Filed |

## Architecture & Design Specifications
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$).
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for`. Verified 100% bitwise deterministic across 24 worker threads (`GemmP2_BitwiseDeterminism` asserts active worker count > 1).
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.


