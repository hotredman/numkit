# signal.fft — slower than MATLAB (single-threaded vs FFTW)

- **Status:** 🔴 OPEN
- **Kind:** perf
- **Slowdown:** 1.2×–4.3× vs MATLAB R2025b (worst: complex N=262144)
- **Found:** 2026-06 via DEEP-PROBE (FFT speed review)
- **Note:** results are CORRECT (FFT family is bit-identical to MATLAB — see
  the FFT correctness review). This is a pure performance gap.

## Measurement (µs per call, Arrow Lake / AVX2 / MSVC)

| N | numkit real | MATLAB real | numkit cplx | MATLAB cplx |
|---|---|---|---|---|
| 4096 | 15.5 | 9.1 (1.7×) | 29.7 | 9.6 (3.1×) |
| 16384 | 99.7 | 25.1 (4.0×) | 182 | 49.9 (3.6×) |
| 65536 | 650 | 539 (1.2×) | 977 | 442 (2.2×) |
| 262144 | 3100 | 1266 (2.4×) | 5309 | 1239 (4.3×) |

## Root cause (NOT lack of SIMD)
A Highway/SIMD FFT already exists (`backends/fft_r2_soa_simd.cpp`,
`fft_r4_soa_simd.cpp`, AoS r2/r4, + a Highway twist in the rfft path). The
gap is structural:
1. **Single-threaded** — MATLAB uses multithreaded FFTW (the dominant factor
   at large N). numkit's fft has no `parallel_for`/threads.
2. **MSVC codegen cliff** on odd-log2 sizes (N=16384 → 4×). Under clang-cl
   the same code is ~2.4× faster at N=32768 and matches/beats MSVC on 8/9
   sizes (measured, see memory).
3. **Wrapper/alloc** ≈55% of per-call cost at large N (complex-result
   page-commit + framework bookkeeping).

## Already tried & reverted (do NOT redo)
mixed-radix, radix-8, AoS r4, Stockham, SIMD-twist — each regresses MSVC
codegen for other sizes or loses (see `feedback_fft_msvc_limits.md`,
`feedback_fft_optimization_dead_ends.md`).

## Suggested fix (in leverage order)
1. **Multithread large-N FFT** (≥64k) — biggest untried lever; risky on
   Arrow Lake (threading dispatch hurt other ops there — measure first).
2. **TU-split the backends** to dodge the MSVC inliner cliff (helps the
   odd-log2 sizes).
3. **clang-cl Windows build** — matches/beats MSVC nearly everywhere.
External FFTW/pocketfft and output-MValue caching were rejected by the user.

## Benchmark (the "test" for a perf entry — not a DISABLED gtest)
`libs/signal/benchmarks/fft_bench.cpp` — `BM_Fft_KernelOnly_*`,
`BM_Fft_RfftPackOnly`, `BM_Fft_RfftTwistOnlyScalar`. Re-measure with these
before any change; the bottleneck split may have moved.

## References
- `libs/signal/src/transforms/fft.cpp` + `backends/`
- memory: `feedback_fft_optimization_dead_ends.md`,
  `feedback_fft_msvc_limits.md`, `project_fft_native_cliff.md`
- MATLAB `doc fft` (FFTW)
