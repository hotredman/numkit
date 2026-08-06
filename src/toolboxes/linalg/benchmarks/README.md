# Linear Algebra Performance Benchmarks

Measured benchmark comparison between `numkit` Linear Algebra module (`numkit_bench.exe` Release build) and MATLAB R2025b executed headlessly on the same machine.

## Environment & Methodology

- **Date**: 2026-08-06
- **Hardware**: 24-core x86_64 CPU @ 3.07 GHz, L3 Cache 36.8 MB
- **NumKit Build**: MSVC 2022 (Visual Studio 17.14), C++20 Release (`build/desktop-fast`)
- **NumKit Raw Output**: [`results/2026-08-06_p7_numkit_bench.txt`](results/2026-08-06_p7_numkit_bench.txt)
- **MATLAB Version**: MATLAB R2025b (25.2.0.2998904), 24 threads
- **MATLAB Raw Output**: [`results/2026-08-06_matlab_r2025b_x86_64.txt`](results/2026-08-06_matlab_r2025b_x86_64.txt)

## Measured Comparison Table (NumKit vs MATLAB R2025b)

| Op / Benchmark | Domain | Size (n) | NumKit Time | MATLAB R2025b Time | Ratio (NumKit/MATLAB) | Gate / Status |
|----------------|--------|----------|-------------|--------------------|-----------------------|---------------|
| **GEMM** (`gemm`) | Real Double | 64 | 0.028 ms | 0.100 ms | 0.28× | PASSED (NumKit 3.6× faster) |
| **GEMM** (`gemm`) | Real Double | 128 | 0.134 ms | 0.010 ms | 13.4× | S2 Bug Filed (MKL AVX-512) |
| **GEMM** (`gemm`) | Real Double | 256 | 0.996 ms | 0.090 ms | 11.1× | S2 Bug Filed (MKL AVX-512) |
| **GEMM** (`gemm`) | Real Double | 512 | 7.445 ms | 0.930 ms | 8.0× | S2 Bug Filed (MKL AVX-512) |
| **GEMM** (`gemm`) | Real Double | 1024 | 60.02 ms | 4.650 ms | 12.9× | S2 Bug Filed (MKL AVX-512) |
| **GEMM** (`gemm`) | Real Double | 2048 | 479.1 ms | 28.18 ms | 17.0× | S2 Bug Filed (MKL AVX-512) |
| **LU Factorization** (`lu`) | Real Double | 64 | 0.031 ms | 0.030 ms | 1.03× | PASSED (Target ≤ 1.5×) |
| **LU Factorization** (`lu`) | Real Double | 128 | 0.342 ms | 0.060 ms | 5.7× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 256 | 2.776 ms | 0.270 ms | 10.3× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 512 | 16.67 ms | 2.710 ms | 6.2× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Real Double | 1024 | 89.49 ms | 7.970 ms | 11.2× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.100 ms | 0.040 ms | 2.50× | PASSED (Target ≤ 2.5×) |
| **LU Factorization** (`lu`) | Complex Double | 128 | 1.051 ms | 0.140 ms | 7.5× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 256 | 5.452 ms | 0.930 ms | 5.9× | S2 Bug Filed |
| **LU Factorization** (`lu`) | Complex Double | 512 | 31.32 ms | 4.640 ms | 6.75× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 64 | 0.012 ms | 0.020 ms | 0.60× | PASSED (NumKit 1.6× faster) |
| **Cholesky** (`chol`) | Real Double | 128 | 0.102 ms | 0.040 ms | 2.55× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 256 | 1.145 ms | 0.100 ms | 11.5× | S2 Bug Filed |
| **Cholesky** (`chol`) | Real Double | 512 | 15.13 ms | 0.780 ms | 19.4× | S2 Bug Filed |

## Architecture & Design Specifications
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$).
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for`. Verified 100% bitwise deterministic.
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.


