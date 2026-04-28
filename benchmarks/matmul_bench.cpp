// benchmarks/matmul_bench.cpp
//
// Square-matrix multiply via mtimes. Only the mtimes call is timed;
// input matrices are built once per benchmark-size and re-used across
// iterations.

#include <numkit/m/builtin/lang/operators/binary_ops.hpp>
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

// 64 (fits L1) … 512 (still in L3). Smaller sizes are the common case
// inside MATLAB scripts (small linear algebra in tight loops). Use the
// _Large variant below to see the L2-blocking regime.
BENCHMARK(BM_Mtimes_Square)
    ->RangeMultiplier(2)
    ->Range(64, 512)
    ->Complexity(benchmark::oNCubed);

// Large-size matmul. 1024 spills C past L2 (1024² × 8B = 8 MB), 2048
// pushes A+B+C to ~96 MB total — solidly DRAM-bound territory where
// L2 blocking and B-packing matter. Each call does ~2N³ flops, so a
// 1024 call is 2 GF and 2048 is 16 GF — keep iters small or you'll
// blow the bench wall-time budget. Run with:
//   --benchmark_filter=BM_Mtimes_Large --benchmark_min_time=0.5s
static void BM_Mtimes_Large(benchmark::State &state)
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
    state.counters["flops/iter"] =
        benchmark::Counter(2.0 * double(n) * double(n) * double(n),
                           benchmark::Counter::kDefaults,
                           benchmark::Counter::OneK::kIs1000);
}

BENCHMARK(BM_Mtimes_Large)
    ->Arg(1024)
    ->Arg(2048)
    ->Complexity(benchmark::oNCubed);
