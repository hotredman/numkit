// benchmarks/conv_bench.cpp
//
// 1D real convolution. The interesting axis is the signal length for
// a fixed-size kernel (which matches the typical filtering use case),
// plus a symmetric equal-length case at the low end.

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <string>

namespace {

numkit::Value makeRealVector(size_t n, uint32_t seed)
{
    using namespace numkit;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value v = Value::matrix(n, 1, ValueType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);
    return v;
}

} // namespace

// Variable-length signal against a fixed 64-tap kernel — models the
// typical filter() / conv(sig, kernel) shape seen in DSP pipelines.
static void BM_Conv_FixedKernel64(benchmark::State &state)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Value sig    = makeRealVector(n, 11);
    Value kernel = makeRealVector(64, 22);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        Value y = signal::conv(alloc, sig, kernel, "full");
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
}

BENCHMARK(BM_Conv_FixedKernel64)
    ->RangeMultiplier(4)
    ->Range(1 << 10, 1 << 16)
    ->Complexity(benchmark::oN);

// Equal-length convolution — worst-case quadratic shape when both
// inputs are comparable in size. Useful for measuring the direct-loop
// hot path vs any future FFT-based fallback.
static void BM_Conv_SquareLen(benchmark::State &state)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Value a = makeRealVector(n, 33);
    Value b = makeRealVector(n, 44);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        Value y = signal::conv(alloc, a, b, "full");
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
}

BENCHMARK(BM_Conv_SquareLen)
    ->RangeMultiplier(2)
    ->Range(64, 2048)
    ->Complexity(benchmark::oNSquared);
