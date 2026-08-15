# Perf Bug: GEMM and Solve scaling gap vs Multithreaded MKL (S2)

- **Date**: 2026-08-06
- **Status**: OPEN (Deferred to Phase 2 threading optimization)
- **Severity**: S2 (Performance gap > 3x vs multithreaded MKL baseline)

## Problem Description

At large matrix dimensions ($N \ge 512$), NumKit's Highway SIMD GEMM achieves ~36 GFLOPS single/multi-threaded on AVX2, whereas MATLAB R2025b (linked against Intel MKL) reaches up to 609 GFLOPS on 24 cores (AVX-512 FMA multi-threaded GEMM).

## Measured Benchmarks (2026-08-06 Cycle 3, 24-core x86_64)

- **GEMM Real N=2048**: NumKit **46.24 ms** (372 GFLOPS, 11.6x thread speedup) vs MATLAB **28.02 ms** (613 GFLOPS) — Gap **1.65x**
- **LU Real N=1024**: NumKit **127.0 ms** vs MATLAB **4.49 ms** (28.3x gap)
- **Chol Real N=1024**: NumKit **54.93 ms** vs MATLAB **8.98 ms** — **Gap closed from 27.0x to 6.12x!** (Accelerated 4.2x via parallel syrk)
- **Solve Real N=1024**: NumKit **454.3 ms** vs MATLAB **46.70 ms** — **Gap closed from 76.6x to 9.73x!** (Accelerated 7.4x via blocked LU+trsm)

## C5 Triage & Completed Fixes (2026-08-06 Cycle 3)

1. **`chol` ($N=1024$) Fix**:
   - `syrk` (symmetric rank-k update) parallelized across column blocks via `parallel_for`.
   - `chol` time at $N=1024$ dropped from **230.1 ms to 54.93 ms** (**4.2x speedup**!).

2. **`solve` / `linsolve` ($N=1024$) Fix**:
   - Routed `la_solve` through `lu_pivot_inplace` (blocked SIMD LU) + `ops::trsm`.
   - `linsolve` time at $N=1024$ dropped from **3348.9 ms to 454.3 ms** (**7.4x speedup**!).
