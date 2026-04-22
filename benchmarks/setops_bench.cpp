// benchmarks/setops_bench.cpp
//
// Phase-10 sweep covering Phase 8 set / search ops:
//   unique / ismember / union / intersect / setdiff / histcounts / discretize.
// All algorithms here are O(N log N) (sort-based); inputs are random
// uniform integers in [1, N/4] to give a realistic distribution of
// duplicates.

#include <numkit/m/builtin/MStdSetOps.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

using namespace numkit::m;

MValue makeIntVec(size_t n, int64_t hi, uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int64_t> d(1, hi);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<double>(d(rng));
    return v;
}

MValue makeRealVec(size_t n, uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(0.0, 100.0);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = d(rng);
    return v;
}

MValue makeEdges(size_t n)
{
    MValue v = MValue::matrix(1, n + 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i <= n; ++i)
        p[i] = 100.0 * static_cast<double>(i) / static_cast<double>(n);
    return v;
}

} // namespace

static void BM_Unique(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeIntVec(n, std::max<int64_t>(8, static_cast<int64_t>(n / 4)), 1);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = builtin::unique(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Unique)->RangeMultiplier(4)->Range(1 << 10, 1 << 20);

static void BM_Ismember(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto a = makeIntVec(n, std::max<int64_t>(8, static_cast<int64_t>(n / 4)), 2);
    auto b = makeIntVec(n / 4, std::max<int64_t>(8, static_cast<int64_t>(n / 4)), 3);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = builtin::ismember(alloc, a, b);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Ismember)->RangeMultiplier(4)->Range(1 << 10, 1 << 20);

template <typename Fn>
static void runPairBench(benchmark::State &s, Fn fn)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto a = makeIntVec(n,     std::max<int64_t>(8, static_cast<int64_t>(n / 4)), 4);
    auto b = makeIntVec(n / 2, std::max<int64_t>(8, static_cast<int64_t>(n / 4)), 5);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = fn(alloc, a, b);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}

static void BM_Union    (benchmark::State &s) { runPairBench(s, [](auto &a, auto &x, auto &y){ return builtin::setUnion    (a, x, y); }); }
static void BM_Intersect(benchmark::State &s) { runPairBench(s, [](auto &a, auto &x, auto &y){ return builtin::setIntersect(a, x, y); }); }
static void BM_Setdiff  (benchmark::State &s) { runPairBench(s, [](auto &a, auto &x, auto &y){ return builtin::setDiff     (a, x, y); }); }

BENCHMARK(BM_Union)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_Intersect)->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_Setdiff)  ->RangeMultiplier(4)->Range(1 << 10, 1 << 20);

static void BM_Histcounts(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeRealVec(n, 6);
    auto edges = makeEdges(64);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = builtin::histcounts(alloc, x, edges);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Histcounts)->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
