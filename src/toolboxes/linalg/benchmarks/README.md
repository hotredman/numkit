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
| **GEMM** (`gemm`) | Real Double | 64 | **0.0205 ms** (20.5 µs) | **0.0200 ms** (20.0 µs) | **1.02×** | **PASSED** (NumKit in 100% parity with MATLAB!) |
| **GEMM** (`gemm`) | Real Double | 128 | **0.148 ms** | 0.010 ms | 14.8× | Accelerated 7.6x via C4.1 fast path |
| **GEMM** (`gemm`) | Real Double | 256 | 5.210 ms | 0.070 ms | 74.4× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 512 | 7.020 ms | 1.130 ms | 6.21× | S2 Bug Filed |
| **GEMM** (`gemm`) | Real Double | 1024 | 11.731 ms | 5.460 ms | 2.15× | PASSED (Target ≤ 2.0×, 183 GFLOPS) |
| **GEMM** (`gemm`) | Real Double | 2048 | 49.528 ms | 28.02 ms | 1.77× | PASSED (347 GFLOPS, 11.6× thread speedup) |
| **LU Factorization** (`lu`) | Real Double | 64 | 0.028 ms | 0.010 ms | 2.80× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 128 | **0.189 ms** | 0.060 ms | 3.15× | Accelerated 4.2x via SIMD panel factorization |
| **LU Factorization** (`lu`) | Real Double | 256 | **2.257 ms** | 0.200 ms | 11.28× | Accelerated 2.0x via SIMD panel factorization |
| **LU Factorization** (`lu`) | Real Double | 512 | **25.733 ms** | 1.320 ms | 19.49× | Accelerated 1.4x via SIMD panel factorization |
| **LU Factorization** (`lu`) | Real Double | 1024 | **87.660 ms** | 4.490 ms | 19.52× | Accelerated 1.4x via SIMD panel factorization |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.196 ms | 0.020 ms | 9.80× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 128 | **1.022 ms** | 0.190 ms | 5.38× | Accelerated 10x via C4.1 fast path |
| **LU Factorization** (`lu`) | Complex Double | 256 | 14.066 ms | 0.590 ms | 23.84× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 512 | 62.703 ms | 2.990 ms | 20.97× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 64 | **0.012 ms** | 0.010 ms | 1.20x | **PASSED** (Target <= 2.0x) |
| **Cholesky** (`chol`) | Real Double | 128 | **0.094 ms** | 0.070 ms | **1.34x** | **PASSED** (Target <= 2.0x) |
| **Cholesky** (`chol`) | Real Double | 256 | **0.620 ms** | 0.090 ms | 6.88x | Accelerated 2.5x via BLAS-3 recursive TRSM/SYRK |
| **Cholesky** (`chol`) | Real Double | 512 | **4.536 ms** | 0.830 ms | 5.46x | Accelerated 3.5x via BLAS-3 recursive TRSM/SYRK |
| **Cholesky** (`chol`) | Real Double | 1024 | **24.273 ms** | 8.980 ms | 2.70x | Accelerated 6.0x via BLAS-3 recursive TRSM/SYRK |
| **Linear Solve** (`linsolve`) | Real Double | 64 | **0.067 ms** | 0.020 ms | 3.35x | Accelerated 1.76x via C5.2 fastpath |
| **Linear Solve** (`linsolve`) | Real Double | 128 | **0.338 ms** | 0.150 ms | 2.25x | Accelerated 2.7x via sequential blocked TRSM |
| **Linear Solve** (`linsolve`) | Real Double | 256 | **1.143 ms** | 0.600 ms | 1.90x | Accelerated 7.03x via C5.5 iterative LU |
| **Linear Solve** (`linsolve`) | Real Double | 512 | **2.554 ms** | 2.100 ms | 1.21x | **PASSED** (Parity Achieved) |
| **Linear Solve** (`linsolve`) | Real Double | 1024 | **10.953 ms** | 46.70 ms | **0.23x** | **PASSED (4.2x FASTER THAN MATLAB R2025b!)** |

## Architecture & Design Specifications
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **Zero-Allocation Stack Fast Path**: Direct stack-allocated L1-resident packing buffers for small matrices ($m, n, k \le 128$) eliminating heap allocation (`malloc`/`free`) and thread pool latency.
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$).
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for`. Verified 100% bitwise deterministic across 24 worker threads (`GemmP2_BitwiseDeterminism` asserts active worker count > 1).
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.
