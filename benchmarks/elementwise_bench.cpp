// benchmarks/elementwise_bench.cpp
//
// Elementwise math on large arrays — the primary SIMD target. sin()
// here is a stand-in for the whole family (sin / cos / exp / log)
// since they share the same shape: one math-lib call per element.

#include <numkit/m/builtin/MStdMath.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

numkit::m::MValue makeReal(size_t n, uint32_t seed)
{
    using namespace numkit::m;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-10.0, 10.0);

    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);
    return v;
}

} // namespace

static void BM_Sin_LargeArray(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue x = makeReal(n, 7);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        MValue y = builtin::sin(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            static_cast<int64_t>(sizeof(double)));
}

// 2^10 fits L1, 2^22 is ~32 MiB and overflows L3 on most boxes.
// The memory-bound region (post-L3) is where SIMD buys the least —
// useful to see the plateau.
BENCHMARK(BM_Sin_LargeArray)
    ->RangeMultiplier(4)
    ->Range(1 << 10, 1 << 22)
    ->Complexity(benchmark::oN);
