# Perf Bug: GEMM and Solve scaling gap vs Multithreaded MKL (S2)

- **Date**: 2026-08-06
- **Status**: OPEN (Deferred to Phase 2 threading optimization)
- **Severity**: S2 (Performance gap > 3x vs multithreaded MKL baseline)

## Problem Description

At large matrix dimensions ($N \ge 512$), NumKit's Highway SIMD GEMM achieves ~36 GFLOPS single/multi-threaded on AVX2, whereas MATLAB R2025b (linked against Intel MKL) reaches up to 609 GFLOPS on 24 cores (AVX-512 FMA multi-threaded GEMM).

## Measured Benchmarks (2026-08-06, 24-core x86_64)

- **GEMM Real N=2048**: NumKit 479.1 ms vs MATLAB 28.18 ms (17.0x gap)
- **LU Real N=1024**: NumKit 89.49 ms vs MATLAB 7.97 ms (11.2x gap)
- **Chol Real N=1024**: NumKit 285.1 ms vs MATLAB 1.91 ms (149x gap - unblocked scalar loop vs MKL potrf)
- **Solve Real N=1024**: NumKit 3877.6 ms vs MATLAB 9.59 ms (404x gap)

## Root Cause & Action Plan

1. **GEMM Microkernel**: Highway microkernel uses 2xN double lanes (AVX2 4 doubles per vector). Next iteration target: AVX-512 / FMA 32-register packing + OpenMP/tbb thread pool.
2. **Blocked Cholesky**: `chol` uses $nb=64$ right-looking factorization. Needs multi-threaded `syrk` and AVX-512 `trsm` acceleration.
