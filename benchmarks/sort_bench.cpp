// benchmarks/sort_bench.cpp
//
// sort(x) is a small but common hot path. Three data patterns —
// random, already sorted, reverse sorted — since real sort
// implementations behave very differently on each.

#include <numkit/m/builtin/MStdMatrix.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <random>
#include <tuple>

namespace {

enum class Pattern { Random, Sorted, Reverse };

numkit::m::MValue makeVector(size_t n, Pattern p)
{
    using namespace numkit::m;
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);

    if (p == Pattern::Sorted)
        std::sort(data, data + n);
    else if (p == Pattern::Reverse)
        std::sort(data, data + n, std::greater<double>{});
    return v;
}

void runSortBench(benchmark::State &state, Pattern p)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue x = makeVector(n, p);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        auto [sorted, idx] = builtin::sort(alloc, x);
        benchmark::DoNotOptimize(sorted);
        benchmark::DoNotOptimize(idx);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

} // namespace

static void BM_Sort_Random(benchmark::State &s)  { runSortBench(s, Pattern::Random); }
static void BM_Sort_Sorted(benchmark::State &s)  { runSortBench(s, Pattern::Sorted); }
static void BM_Sort_Reverse(benchmark::State &s) { runSortBench(s, Pattern::Reverse); }

BENCHMARK(BM_Sort_Random)
    ->RangeMultiplier(4)->Range(1 << 10, 1 << 20)
    ->Complexity(benchmark::oNLogN);
BENCHMARK(BM_Sort_Sorted)
    ->RangeMultiplier(4)->Range(1 << 10, 1 << 20)
    ->Complexity(benchmark::oNLogN);
BENCHMARK(BM_Sort_Reverse)
    ->RangeMultiplier(4)->Range(1 << 10, 1 << 20)
    ->Complexity(benchmark::oNLogN);
