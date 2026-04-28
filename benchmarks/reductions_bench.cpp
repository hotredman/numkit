// benchmarks/reductions_bench.cpp
//
// Phase-10 sweep covering Phase 1-3 reductions:
//   * Existing-but-previously-unbenched: sum/mean/min/max/prod/cumsum
//     and the dim-overload variants.
//   * Phase 1 stats: var/std/median/quantile/mode.
//   * Phase 2 nan-aware: nansum/nanmean/nanmedian.
//   * Phase 3 cumulative + logical: cumprod/cummax/cummin/any/all.
//
// All bench inputs are real double vectors (1D) or matrices (2D dim
// path). Sizes span L1 (1K) through DRAM (4M). The kernels are all
// O(N) or O(N log N) for the median family.

#include <numkit/m/builtin/lang/arrays/matrix.hpp>
#include <numkit/m/builtin/math/elementary/reductions.hpp>
#include <numkit/m/builtin/data_analysis/descriptive_statistics/stats.hpp>
#include <numkit/m/stats/nan_aware/nan_aware.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

using namespace numkit::m;

MValue makeVec(size_t n, uint32_t seed = 7)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = d(rng);
    return v;
}

MValue makeMat(size_t r, size_t c, uint32_t seed = 11)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    MValue m = MValue::matrix(r, c, MType::DOUBLE, nullptr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < r * c; ++i) p[i] = d(rng);
    return m;
}

MValue makeVecWithNaN(size_t n, double nanFraction, uint32_t seed = 13)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    std::bernoulli_distribution nanFlip(nanFraction);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = nanFlip(rng) ? std::nan("") : d(rng);
    return v;
}

template <typename Fn>
void runVecBench(benchmark::State &s, Fn fn)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeVec(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = fn(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}

template <typename Fn>
void runMatDimBench(benchmark::State &s, Fn fn, int dim)
{
    const size_t n = static_cast<size_t>(s.range(0));
    // Square-ish 2D so dim=1 vs dim=2 both touch realistic strides.
    const size_t side = static_cast<size_t>(std::sqrt(static_cast<double>(n)));
    auto m = makeMat(side, side);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = fn(alloc, m, dim);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(side * side));
}

} // namespace

// ── Existing reductions (vector form) ───────────────────────

static void BM_Sum   (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::sum(a, x); }); }
static void BM_Mean  (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::mean(a, x); }); }
static void BM_Prod  (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::prod(a, x); }); }
static void BM_Max   (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return std::get<0>(builtin::max(a, x)); }); }
static void BM_Min   (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return std::get<0>(builtin::min(a, x)); }); }
static void BM_Cumsum(benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::cumsum(a, x); }); }

BENCHMARK(BM_Sum)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Mean)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Prod)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Max)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Min)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Cumsum) ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);

// ── Dim-overload variants on matrices ──────────────────────

static void BM_SumDim1 (benchmark::State &s) { runMatDimBench(s, [](auto &a, auto &x, int d){ return builtin::sum (a, x, d); }, 1); }
static void BM_SumDim2 (benchmark::State &s) { runMatDimBench(s, [](auto &a, auto &x, int d){ return builtin::sum (a, x, d); }, 2); }
static void BM_MeanDim2(benchmark::State &s) { runMatDimBench(s, [](auto &a, auto &x, int d){ return builtin::mean(a, x, d); }, 2); }

BENCHMARK(BM_SumDim1) ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_SumDim2) ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_MeanDim2)->RangeMultiplier(4)->Range(1 << 10, 1 << 22);

// ── Phase 1 stats ───────────────────────────────────────────

static void BM_Var   (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::var(a, x); }); }
static void BM_Std   (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::stdev(a, x); }); }
static void BM_Median(benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::median(a, x); }); }
static void BM_Mode  (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return std::get<0>(builtin::mode(a, x)); }); }

BENCHMARK(BM_Var)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_Std)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_Median)->RangeMultiplier(4)->Range(1 << 10, 1 << 20);
BENCHMARK(BM_Mode)  ->RangeMultiplier(4)->Range(1 << 10, 1 << 18);  // sort+RLE — cap earlier

// ── Phase 2 nan-aware (10% NaN) ─────────────────────────────

template <typename Fn>
static void runNanBench(benchmark::State &s, Fn fn)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeVecWithNaN(n, 0.10);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = fn(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}

static void BM_Nansum   (benchmark::State &s) { runNanBench(s, [](auto &a, auto &x){ return stats::nansum   (a, x); }); }
static void BM_Nanmean  (benchmark::State &s) { runNanBench(s, [](auto &a, auto &x){ return stats::nanmean  (a, x); }); }
static void BM_Nanmedian(benchmark::State &s) { runNanBench(s, [](auto &a, auto &x){ return stats::nanmedian(a, x); }); }

BENCHMARK(BM_Nansum)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Nanmean)  ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Nanmedian)->RangeMultiplier(4)->Range(1 << 10, 1 << 18);

// ── Phase 3 cumulative + logical ───────────────────────────

static void BM_Cumprod(benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::cumprod(a, x); }); }
static void BM_Cummax (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::cummax (a, x); }); }
static void BM_Any    (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::anyOf  (a, x); }); }
static void BM_All    (benchmark::State &s) { runVecBench(s, [](auto &a, auto &x){ return builtin::allOf  (a, x); }); }

BENCHMARK(BM_Cumprod)->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Cummax) ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_Any)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_All)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
