# Linear Algebra Performance Benchmarks

Measured benchmark results for the `numkit` Linear Algebra module (`numkit_bench.exe` Release build).

## Environment & Methodology

- **Date**: 2026-08-06
- **Raw Benchmark Output**: [`results/2026-08-06_x86_64_msvc.txt`](results/2026-08-06_x86_64_msvc.txt)
- **Hardware**: 24-core x86_64 CPU @ 3.07 GHz, L3 Cache 36.8 MB
- **Compiler**: MSVC 2022 (Visual Studio 17.14), C++20 Release configuration
- **SIMD Engine**: Google Highway Dynamic Dispatch (`HWY_DYNAMIC_DISPATCH`)
- **Harness**: Google Benchmark v1.8 (median CPU execution time across 10+ iterations)

## Measured Performance Table

| Benchmark | Domain | Size (n) | Measured Time (CPU) | Iterations |
|-----------|--------|----------|----------------------|------------|
| **LU Factorization** (`lu`) | Real Double | 64 | 0.028 ms | 21,816 |
| **LU Factorization** (`lu`) | Real Double | 128 | 0.329 ms | 1,948 |
| **LU Factorization** (`lu`) | Real Double | 256 | 2.887 ms | 249 |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.100 ms | 6,400 |
| **LU Factorization** (`lu`) | Complex Double | 128 | 0.858 ms | 747 |
| **LU Factorization** (`lu`) | Complex Double | 256 | 7.639 ms | 90 |
| **Cholesky Factorization** (`chol`) | Real Double | 64 | 0.012 ms | 56,000 |
| **Cholesky Factorization** (`chol`) | Real Double | 128 | 0.105 ms | 6,400 |
| **Cholesky Factorization** (`chol`) | Real Double | 256 | 0.872 ms | 896 |
| **Linear Solve** (`linsolve`) | Real Double | 64 | 0.115 ms | 6,400 |
| **Linear Solve** (`linsolve`) | Real Double | 128 | 1.639 ms | 448 |
| **Linear Solve** (`linsolve`) | Real Double | 256 | 22.396 ms | 30 |
| **Linear Solve** (`linsolve`) | Complex Double | 64 | 0.530 ms | 1,120 |
| **Linear Solve** (`linsolve`) | Complex Double | 128 | 4.404 ms | 149 |
| **Linear Solve** (`linsolve`) | Complex Double | 256 | 40.441 ms | 17 |

## Notes
- Blocked LU decomposition (`lu`) routes trailing matrix updates through Highway SIMD `numkit::ops::gemm`.
- QR decomposition (`qr`) and Cholesky factorization (`chol`) use exact scalar factorizations.
- Unmeasured external comparison baselines (e.g. MATLAB R2025b) are omitted until standardized benchmark comparison runs are executed in a unified test environment.
