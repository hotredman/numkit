# Perf Bug: GEMM and Solve scaling gap vs Multithreaded MKL (S2)

- **Date**: 2026-08-06
- **Status**: OPEN (Deferred to Phase 2 threading optimization)
- **Severity**: S2 (Performance gap > 3x vs multithreaded MKL baseline)

## Problem Description

At large matrix dimensions ($N \ge 512$), NumKit's Highway SIMD GEMM achieves ~36 GFLOPS single/multi-threaded on AVX2, whereas MATLAB R2025b (linked against Intel MKL) reaches up to 609 GFLOPS on 24 cores (AVX-512 FMA multi-threaded GEMM).

## Measured Benchmarks (2026-08-06 Cycle 2, 24-core x86_64)

- **GEMM Real N=2048**: NumKit **41.26 ms** (416.4 GFLOPS, 11.6x thread speedup) vs MATLAB **28.02 ms** (613 GFLOPS) — **Gap closed to 1.47x!**
- **LU Real N=1024**: NumKit **83.61 ms** vs MATLAB **4.49 ms** (18.6x gap)
- **Chol Real N=1024**: NumKit **242.6 ms** vs MATLAB **8.98 ms** (27.0x gap)
- **Solve Real N=1024**: NumKit **3577.5 ms** vs MATLAB **46.70 ms** (76.6x gap)

## C5 Triage & Root Cause Analysis (2026-08-06 Profile)

1. **`chol` ($N=1024$) Triage**:
   - `chol` uses blocked Cholesky ($nb=64$).
   - Profiling shows 92% of runtime is spent in `syrk` trailing updates (`syrk_generic`).
   - `syrk_generic` in `gemm_highway.cpp` is currently **single-threaded** (not parallelized via `parallel_for`).
   - Action item for Cycle 3: Parallelize `syrk` across $jc$ blocks using `parallel_for`. Expected win: `chol` $N=1024$ time drops from 242 ms to ~15 ms (16x speedup).

2. **`solve` / `linsolve` ($N=1024$) Triage**:
   - `linsolve` calls `numkit::ops::la_solve` (`src/ops/src/la_solve.cpp`).
   - `la_solve.cpp` implements `lu_partial_pivot` using a **scalar $O(N^3)$ unblocked loop** (`for (i=k+1..n) for (j=k+1..n) A[i+j*n] -= factor * A[k+j*n]`), completely bypassing `lu_decompose` (which uses Highway SIMD GEMM).
   - Action item for Cycle 3: Route `la_solve` through `lu_decompose` + `ops::trsm`. Expected win: `linsolve` $N=1024$ time drops from 3577 ms to ~85 ms (42x speedup).
