// benchmarks/interp_bench.cpp
//
// Phase-10 sweep covering libs/fit (interp1, polyval, trapz). These
// existed before the parity expansion but have never been benched.

#include <numkit/m/fit/MFitInterp.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <random>

namespace {

using namespace numkit::m;

MValue makeSorted(size_t n)
{
    // Strictly increasing x — required by interp1.
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        p[i] = static_cast<double>(i);
    return v;
}

MValue makeY(size_t n, uint32_t seed = 7)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> d(0.0, 1.0);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = d(rng);
    return v;
}

MValue makeQuery(size_t nx, size_t nq, uint32_t seed = 11)
{
    // Query points uniformly in [0, nx-1].
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(0.0, static_cast<double>(nx - 1));
    MValue v = MValue::matrix(nq, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < nq; ++i) p[i] = d(rng);
    return v;
}

} // namespace

static void BM_Interp1Linear(benchmark::State &s)
{
    const size_t nx = static_cast<size_t>(s.range(0));
    const size_t nq = nx;
    auto x  = makeSorted(nx);
    auto y  = makeY(nx);
    auto xq = makeQuery(nx, nq);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto out = fit::interp1(alloc, x, y, xq, "linear");
        benchmark::DoNotOptimize(out);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(nq));
}
BENCHMARK(BM_Interp1Linear)->RangeMultiplier(4)->Range(1 << 10, 1 << 18);

static void BM_Interp1Spline(benchmark::State &s)
{
    const size_t nx = static_cast<size_t>(s.range(0));
    const size_t nq = nx;
    auto x  = makeSorted(nx);
    auto y  = makeY(nx);
    auto xq = makeQuery(nx, nq);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto out = fit::interp1(alloc, x, y, xq, "spline");
        benchmark::DoNotOptimize(out);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(nq));
}
BENCHMARK(BM_Interp1Spline)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

static void BM_Polyval(benchmark::State &s)
{
    // Degree-15 polynomial (16 coefficients) evaluated at n points.
    const size_t n = static_cast<size_t>(s.range(0));
    auto p  = makeY(16);
    auto xq = makeY(n, 5);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = fit::polyval(alloc, p, xq);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Polyval)->RangeMultiplier(4)->Range(1 << 10, 1 << 22);

static void BM_TrapzUniform(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto y = makeY(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto out = fit::trapz(alloc, y);
        benchmark::DoNotOptimize(out);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_TrapzUniform)->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
