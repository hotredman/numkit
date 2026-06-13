// benchmarks/elementwise_bench.cpp
//
// Unary elementwise math on large arrays — the primary SIMD target.
// The whole sin/cos/exp/log/abs family shares the same shape: one
// math-lib call per element, no cross-element dependencies. Numbers
// should cluster within a factor of 2 across the family; any big
// outlier on the baseline is worth investigating before Highway work.

#include <numkit/math/exp_log/exponents.hpp>
#include <numkit/math/arithmetic/rounding.hpp>
#include <numkit/math/trig/trigonometry.hpp>
#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <random>

namespace {

// Build a real column vector. `range` controls the input distribution;
// log() needs strictly positive inputs so we offset there.
numkit::Value makeReal(size_t n, uint32_t seed, double lo, double hi)
{
    using namespace numkit;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(lo, hi);

    Value v = Value::matrix(n, 1, ValueType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);
    return v;
}

template <typename Fn>
void runElementwiseBench(benchmark::State &state, Fn fn, double lo, double hi)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Value x = makeReal(n, 7, lo, hi);
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    for (auto _ : state) {
        Value y = fn(x, mr);   // unary math: (value, mr) — mr last.
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            static_cast<int64_t>(sizeof(double)));
}

} // namespace

// 2^10 fits L1, 2^22 is ~32 MiB and overflows L3 on most boxes.
// The memory-bound region (post-L3) is where SIMD buys the least —
// useful to see the plateau.
static void BM_Sin(benchmark::State &s) { runElementwiseBench(s, numkit::math::sin, -10.0, 10.0); }
static void BM_Cos(benchmark::State &s) { runElementwiseBench(s, numkit::math::cos, -10.0, 10.0); }
static void BM_Exp(benchmark::State &s) { runElementwiseBench(s, numkit::math::exp, -5.0,  5.0);  }
static void BM_Log(benchmark::State &s) { runElementwiseBench(s, numkit::math::log,  0.01, 100.0); }
static void BM_Abs(benchmark::State &s) { runElementwiseBench(s, numkit::math::abs, -1e6,  1e6);   }

BENCHMARK(BM_Sin)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Cos)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Exp)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Log)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Abs)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
