// benchmarks/math/elementwise_bench.cpp
//
// Unary elementwise math on large arrays — the primary SIMD target.
// The whole sin/cos/exp/log/abs family shares the same shape: one
// math-lib call per element, no cross-element dependencies. Numbers
// should cluster within a factor of 2 across the family; any big
// outlier on the baseline is worth investigating before Highway work.

#include <nk/builtin/elfun.hpp>
#include <nk/builtin/elfun.hpp>
#include <nk/builtin/elfun.hpp>
#include <nk/builtin/datatypes.hpp>        // isfinite (lang API, math SIMD kernel)
#include <memory_resource>
#include <nk/core/types.hpp>
#include <nk/value/value.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <random>

namespace {

// Build a real column vector. `range` controls the input distribution;
// log() needs strictly positive inputs so we offset there.
nk::Value makeReal(size_t n, uint32_t seed, double lo, double hi)
{
    using namespace nk;
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
    using namespace nk;
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
static void BM_Sin(benchmark::State &s) { runElementwiseBench(s, nk::builtin::sin, -10.0, 10.0); }
static void BM_Cos(benchmark::State &s) { runElementwiseBench(s, nk::builtin::cos, -10.0, 10.0); }
static void BM_Exp(benchmark::State &s) { runElementwiseBench(s, nk::builtin::exp, -5.0,  5.0);  }
static void BM_Log(benchmark::State &s) { runElementwiseBench(s, nk::builtin::log,  0.01, 100.0); }
static void BM_Abs(benchmark::State &s) { runElementwiseBench(s, nk::builtin::abs, -1e6,  1e6);   }

BENCHMARK(BM_Sin)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Cos)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Exp)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Log)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Abs)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);

// Remaining SIMD-backed unary math: tan (trig_highway), sqrt (exp_log_highway),
// round/floor/ceil/fix (rounding_highway), isfinite (isfinite_highway).
static void BM_Tan(benchmark::State &s)      { runElementwiseBench(s, nk::builtin::tan,      -10.0, 10.0);  }
static void BM_Sqrt(benchmark::State &s)     { runElementwiseBench(s, nk::builtin::sqrt,       0.0, 100.0); }
static void BM_Round(benchmark::State &s)    { runElementwiseBench(s, nk::builtin::round,    -1e6,  1e6);   }
static void BM_Floor(benchmark::State &s)    { runElementwiseBench(s, nk::builtin::floor,    -1e6,  1e6);   }
static void BM_Ceil(benchmark::State &s)     { runElementwiseBench(s, nk::builtin::ceil,     -1e6,  1e6);   }
static void BM_Fix(benchmark::State &s)      { runElementwiseBench(s, nk::builtin::fix,      -1e6,  1e6);   }
static void BM_Isfinite(benchmark::State &s) { runElementwiseBench(s, nk::builtin::isfinite, -1e6,  1e6);   }

BENCHMARK(BM_Tan)     ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Sqrt)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Round)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Floor)   ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Ceil)    ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Fix)     ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Isfinite)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
