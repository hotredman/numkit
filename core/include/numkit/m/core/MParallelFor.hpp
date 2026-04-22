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
// overhead of the pool dispatch (~ a few µs of mutex + condvar) is
// well under the kernel's expected wall time at that N. Tune later
// if profiling tells us otherwise.
inline constexpr std::size_t kElementwiseThreshold    = 16 * 1024;
inline constexpr std::size_t kTranscendentalThreshold =  4 * 1024;

template <typename F>
inline void parallel_for(std::size_t n, std::size_t threshold, F &&fn)
{
#if defined(NUMKIT_WITH_THREADS)
    auto &pool = ThreadPool::global();
    if (n >= threshold && pool.workers() > 1) {
        pool.run(n, std::forward<F>(fn));
        return;
    }
#else
    (void)threshold;
#endif
    if (n > 0)
        fn(static_cast<std::size_t>(0), n);
}

} // namespace numkit::m::detail
