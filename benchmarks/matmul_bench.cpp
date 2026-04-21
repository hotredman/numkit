// benchmarks/matmul_bench.cpp
//
// Square-matrix multiply via mtimes. Only the mtimes call is timed;
// input matrices are built once per benchmark-size and re-used across
// iterations.

#include <numkit/m/builtin/MStdBinaryOps.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

numkit::m::MValue makeSquare(size_t n)
{
    using namespace numkit::m;
    std::mt19937 rng(17);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    MValue M = MValue::matrix(n, n, MType::DOUBLE, nullptr);
    double *data = M.doubleDataMut();
    for (size_t i = 0; i < n * n; ++i)
        data[i] = dist(rng);
    return M;
}

} // namespace

static void BM_Mtimes_Square(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue A = makeSquare(n);
    MValue B = makeSquare(n);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        MValue C = builtin::mtimes(alloc, A, B);
        benchmark::DoNotOptimize(C);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    // Each matmul does 2 * n^3 flops (n^2 dots of length n).
    state.counters["flops/iter"] =
        benchmark::Counter(2.0 * double(n) * double(n) * double(n),
                           benchmark::Counter::kDefaults,
                           benchmark::Counter::OneK::kIs1000);
}

// 64 (fits L1) … 512 (spills to L3/DRAM). Larger sizes (1024+) make
// the loop impractical for the default min_time — use --benchmark_filter
// with a custom size range when doing deep profiling.
BENCHMARK(BM_Mtimes_Square)
    ->RangeMultiplier(2)
    ->Range(64, 512)
    ->Complexity(benchmark::oNCubed);
