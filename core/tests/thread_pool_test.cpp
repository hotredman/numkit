// core/tests/thread_pool_test.cpp
//
// Unit tests for the std::thread-based pool used by SIMD kernels above
// their parallel thresholds. Compiled in every build but only registers
// gtest cases when NUMKIT_WITH_THREADS is defined; on non-threaded
// builds this file produces a translation unit with zero test bodies
// (the unused-helper trick keeps the linker happy without #if'ing the
// whole file).

#include <gtest/gtest.h>

#if defined(NUMKIT_WITH_THREADS)

#include <numkit/core/parallel_for.hpp>
#include <numkit/core/thread_pool.hpp>

#include <atomic>
#include <cstddef>
#include <numeric>
#include <thread>
#include <vector>

using numkit::detail::parallel_for;
using numkit::detail::ThreadPool;

namespace {

// Confirm the global pool was sized to something sensible. We don't
// pin to a specific number — CI hosts vary — but it must be > 0.
TEST(ThreadPoolTest, GlobalPoolHasWorkers)
{
    auto &pool = ThreadPool::global();
    EXPECT_GT(pool.workers(), 0);
}

// Each chunk should be visited exactly once and the chunks must
// partition [0, n) with no gaps and no overlap.
TEST(ThreadPoolTest, ChunksPartitionRange)
{
    constexpr std::size_t n = 1'000'000;
    std::vector<std::atomic<int>> hits(n);
    for (auto &h : hits)
        h.store(0);

    ThreadPool::global().run(n, [&](std::size_t s, std::size_t e) {
        for (std::size_t i = s; i < e; ++i)
            hits[i].fetch_add(1, std::memory_order_relaxed);
    });

    std::size_t bad = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (hits[i].load(std::memory_order_relaxed) != 1)
            ++bad;
    EXPECT_EQ(bad, 0u);
}

TEST(ThreadPoolTest, RunWithZeroLengthIsNoOp)
{
    bool called = false;
    ThreadPool::global().run(0, [&](std::size_t, std::size_t) { called = true; });
    EXPECT_FALSE(called);
}

// n smaller than the worker count — most workers see an empty
// [start, end) and the callback isn't invoked for them.
TEST(ThreadPoolTest, SmallNSomeChunksEmpty)
{
    constexpr std::size_t n = 3;
    std::atomic<int> total_calls{0};
    std::atomic<std::size_t> total_elems{0};
    ThreadPool::global().run(n, [&](std::size_t s, std::size_t e) {
        total_calls.fetch_add(1, std::memory_order_relaxed);
        total_elems.fetch_add(e - s, std::memory_order_relaxed);
    });
    EXPECT_EQ(total_elems.load(), n);
    EXPECT_GE(total_calls.load(), 1);
    EXPECT_LE(total_calls.load(), ThreadPool::global().workers());
}

// Reduce a known sum across a parallel split — checks that the run()
// barrier really blocks until the last worker is done.
TEST(ThreadPoolTest, ParallelSumMatchesSerial)
{
    constexpr std::size_t n = 100'000;
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 1);  // 1..n

    std::atomic<long long> sum{0};
    ThreadPool::global().run(n, [&](std::size_t s, std::size_t e) {
        long long part = 0;
        for (std::size_t i = s; i < e; ++i)
            part += v[i];
        sum.fetch_add(part, std::memory_order_relaxed);
    });

    const long long expected = static_cast<long long>(n) * (n + 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

// Pool must service consecutive run() calls correctly — workers stay
// alive across calls and pick up the next epoch.
TEST(ThreadPoolTest, ConsecutiveRunsReuseWorkers)
{
    constexpr int    iters = 100;
    constexpr std::size_t n     = 50'000;
    for (int it = 0; it < iters; ++it) {
        std::atomic<std::size_t> total{0};
        ThreadPool::global().run(n, [&](std::size_t s, std::size_t e) {
            total.fetch_add(e - s, std::memory_order_relaxed);
        });
        ASSERT_EQ(total.load(), n) << "iteration " << it;
    }
}

// parallel_for: below threshold should call once with [0, n) on the
// caller thread (no pool dispatch).
TEST(ParallelForTest, BelowThresholdRunsSequential)
{
    constexpr std::size_t n         = 100;
    constexpr std::size_t threshold = 1'000;
    int call_count = 0;
    std::size_t saw_start = 999, saw_end = 999;
    parallel_for(n, threshold, [&](std::size_t s, std::size_t e) {
        ++call_count;
        saw_start = s;
        saw_end   = e;
    });
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(saw_start, 0u);
    EXPECT_EQ(saw_end, n);
}

// At/above threshold the work should be spread (call_count > 1)
// when the pool has multiple workers, otherwise still a single call.
TEST(ParallelForTest, AboveThresholdDispatches)
{
    constexpr std::size_t n         = 100'000;
    constexpr std::size_t threshold = 1'000;
    std::atomic<int> call_count{0};
    std::atomic<std::size_t> total{0};
    parallel_for(n, threshold, [&](std::size_t s, std::size_t e) {
        call_count.fetch_add(1, std::memory_order_relaxed);
        total.fetch_add(e - s, std::memory_order_relaxed);
    });
    EXPECT_EQ(total.load(), n);
    if (ThreadPool::global().workers() > 1)
        EXPECT_GT(call_count.load(), 1);
    else
        EXPECT_EQ(call_count.load(), 1);
}

// n == 0 must not invoke the callback in either path.
TEST(ParallelForTest, ZeroNIsNoOp)
{
    int call_count = 0;
    parallel_for(0, 0, [&](std::size_t, std::size_t) { ++call_count; });
    EXPECT_EQ(call_count, 0);
}

} // namespace

#endif // NUMKIT_WITH_THREADS
