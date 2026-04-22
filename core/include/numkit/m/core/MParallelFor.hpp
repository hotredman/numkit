// core/include/numkit/m/core/MParallelFor.hpp
//
// `parallel_for(n, threshold, fn)` — call `fn(start, end)` over a
// non-overlapping cover of [0, n).
//
//   * NUMKIT_WITH_THREADS undefined            → always sequential:
//     `fn(0, n)` on the caller's thread.
//   * NUMKIT_WITH_THREADS defined AND n ≥ threshold AND the global
//     thread pool has more than one worker → split [0, n) evenly
//     across workers, block until all chunks finish.
//   * Otherwise (n < threshold or single-worker pool) → still
//     `fn(0, n)` sequentially. Threshold lets per-kernel callers
//     opt out of threading on small inputs where the hand-off
//     would dominate.
//
// SIMD kernels in libs/*/src/backends/*_simd.cpp use this exclusively
// for parallelism. Output buffers must be disjoint per chunk and
// inputs read-only — both already true for our element-wise SIMD
// kernels.

#pragma once

#include <cstddef>

#if defined(NUMKIT_WITH_THREADS)
#include <numkit/m/core/MThreadPool.hpp>
#endif

namespace numkit::m::detail {

// Suggested per-kernel parallel thresholds.  Chosen so the per-call
// overhead of the pool dispatch (~ a few µs of mutex + condvar on
// native, ~30-100 µs per Web Worker dispatch on WASM) is well under
// the kernel's expected wall time at that N. Tune later if profiling
// tells us otherwise.
//
// `kCheapElementwiseThreshold` is for the cheapest memory-bound ops
// (abs / + / - / .* / ./). Each element costs essentially one SIMD
// instruction; the kernel saturates DRAM bandwidth at modest N and
// parallelism doesn't help past ~6 workers.
//
// The threshold is platform-dependent because pool dispatch latency
// is wildly different:
//   * native std::thread + condvar: a few µs per dispatch
//   * Emscripten pthread (Web Workers + SharedArrayBuffer atomics):
//     ~30-100 µs per Worker signal × 6 Workers ≈ 200-600 µs per
//     parallel_for call.
// At 200+ µs of dispatch overhead per call, the abs kernel at N=1M
// (~0.5 ms single-thread) gets slower with threading. Raising the
// WASM threshold to 4 M keeps practical-size calls on the caller's
// thread there.
//
// `kTranscendentalThreshold` is much smaller because sin/cos/exp/log
// have high arithmetic intensity per element (Highway's polynomial
// approximations) — the kernel pays for the dispatch even on small N.
inline constexpr std::size_t kElementwiseThreshold    =  16 * 1024;
#if defined(__EMSCRIPTEN__)
inline constexpr std::size_t kCheapElementwiseThreshold = 4 * 1024 * 1024;
#else
inline constexpr std::size_t kCheapElementwiseThreshold =     256 * 1024;
#endif
inline constexpr std::size_t kTranscendentalThreshold =   4 * 1024;

// Worker caps for memory-bandwidth-bound kernels. On dual-channel
// DDR5-6400 (≈ 100 GB/s aggregate), 4-6 workers already saturate
// the bus on `z = x op y` and friends; more workers add signaling
// noise without helping throughput. Compute-bound kernels (mtimes,
// transcendentals) don't need a cap and benefit from every core.
inline constexpr int         kElementwiseMaxWorkers   = 6;

template <typename F>
inline void parallel_for(std::size_t n, std::size_t threshold, F &&fn,
                         int max_workers = 0)
{
#if defined(NUMKIT_WITH_THREADS)
    auto &pool = ThreadPool::global();
    if (n >= threshold && pool.workers() > 1) {
        pool.run(n, std::forward<F>(fn), max_workers);
        return;
    }
#else
    (void)threshold;
    (void)max_workers;
#endif
    if (n > 0)
        fn(static_cast<std::size_t>(0), n);
}

} // namespace numkit::m::detail
