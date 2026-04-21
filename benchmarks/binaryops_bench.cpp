// benchmarks/binaryops_bench.cpp
//
// Binary elementwise operators (plus, times). These are almost
// purely memory-bound at large N — every element requires 2 reads +
// 1 write, a single add/mul in between. They're worth benching
// separately from the unary math family because the arithmetic
// intensity is lower (no transcendental), so the DRAM plateau shows
// up sooner.

#include <numkit/m/builtin/MStdBinaryOps.hpp>
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
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);
    return v;
}

template <typename Fn>
void runBinaryBench(benchmark::State &state, Fn fn)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue a = makeReal(n, 1);
    MValue b = makeReal(n, 2);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        MValue c = fn(alloc, a, b);
        benchmark::DoNotOptimize(c);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    // 2 reads + 1 write = 3 * sizeof(double) per element.
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            3 * static_cast<int64_t>(sizeof(double)));
}

} // namespace

static void BM_Plus(benchmark::State &s)  { runBinaryBench(s, numkit::m::builtin::plus);  }
static void BM_Times(benchmark::State &s) { runBinaryBench(s, numkit::m::builtin::times); }

BENCHMARK(BM_Plus)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Times)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
