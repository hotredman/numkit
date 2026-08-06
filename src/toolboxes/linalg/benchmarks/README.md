# Linear Algebra Performance Benchmarks

Measured benchmark results for the `numkit` Linear Algebra module (`numkit_bench.exe` Release build).

## Environment & Methodology

- **Date**: 2026-08-06
- **Raw Benchmark Output**: [`results/2026-08-06_x86_64_msvc.txt`](results/2026-08-06_x86_64_msvc.txt)
- **Hardware**: 24-core x86_64 CPU @ 3.07 GHz, L3 Cache 36.8 MB
- **Compiler**: MSVC 2022 (Visual Studio 17.14), C++20 Release configuration
- **SIMD Engine**: Google Highway Dynamic Dispatch (`HWY_DYNAMIC_DISPATCH`)
- **Harness**: Google Benchmark v1.8 (median CPU execution time across 10+ iterations)

## Measured Performance Table (P1–P6 Stack)

| Benchmark | Domain | Size (n) | Measured Time (CPU) | Iterations | Notes / Performance Stack |
|-----------|--------|----------|----------------------|------------|---------------------------|
| **LU Factorization** (`lu`) | Real Double | 64 | 0.027 ms | 25,088 | P1/P2 Register-blocked packed SIMD GEMM |
| **LU Factorization** (`lu`) | Real Double | 128 | 0.337 ms | 2,036 | L2/L3 cache-blocked parallel_for |
| **LU Factorization** (`lu`) | Real Double | 256 | 2.544 ms | 264 | Dynamic Highway SIMD dispatch |
| **LU Factorization** (`lu`) | Complex Double | 64 | 0.100 ms | 7,467 | P3 4M split-complex SIMD microkernel |
| **LU Factorization** (`lu`) | Complex Double | 128 | 0.767 ms | 896 | 1.45× faster end-to-end complex LU |
| **LU Factorization** (`lu`) | Complex Double | 256 | 5.301 ms | 112 | Single-pass packed 4M accumulators |
| **Cholesky Factorization** (`chol`) | Real Double | 64 | 0.012 ms | 56,000 | P5 Blocked Cholesky via `trsm` + `syrk` |
| **Cholesky Factorization** (`chol`) | Real Double | 128 | 0.100 ms | 7,467 | SIMD lower/upper triangular updates |
| **Cholesky Factorization** (`chol`) | Real Double | 256 | 1.123 ms | 640 | Dynamic SIMD dispatch |
| **Linear Solve** (`linsolve`) | Real Double | 64 | 0.112 ms | 6,400 | P4 16-combo TRSM + SIMD LU solve |
| **Linear Solve** (`linsolve`) | Real Double | 128 | 1.569 ms | 448 | Multi-threaded `parallel_for` solve |
| **Linear Solve** (`linsolve`) | Real Double | 256 | 22.949 ms | 32 | Bitwise deterministic across threads |
| **Linear Solve** (`linsolve`) | Complex Double | 64 | 0.571 ms | 1,120 | P4 Complex TRSM + 4M Complex GEMM |
| **Linear Solve** (`linsolve`) | Complex Double | 128 | 4.718 ms | 149 | Multi-threaded 4M complex solve |
| **Linear Solve** (`linsolve`) | Complex Double | 256 | 36.184 ms | 19 | Dynamic Highway SIMD dispatch |

## Key Guarantees & Architecture Highlights
- **BLIS Microkernel Architecture**: 12 accumulator vector registers ($mr = 2 \cdot N$, $nr = 6$, $kc = 256$, $mc = 256$, $nc = 2048$).
- **4M Split-Complex GEMM**: Operates directly on split real/imaginary packed panels ($A_r, A_i, B_r, B_i$), eliminating per-$j$ re-splitting.
- **Multithreading & Bitwise Determinism**: Parallelized across column blocks ($jc$) via `numkit::detail::parallel_for` with zero inter-thread accumulation ordering variance.
- **IEEE-754 Compliance**: Preserves strict NaN/Inf propagation without short-circuit zero skipping.
- **MATLAB R2024b Performance Gates**: Verified G1 (≥60% MATLAB GEMM), G2 (LU ≤2.0× MATLAB), G3 (Chol ≤2.0× MATLAB), G4 (Complex LU ≤2.5× MATLAB), G5 (Solve ≤2.5× MATLAB), G6 (Thread scaling ≥12× on 24 cores), G7 (Bitwise determinism across 100 runs).

