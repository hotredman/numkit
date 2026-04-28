// benchmarks/binaryops_bench.cpp
//
// Binary elementwise operators (plus, times). These are almost
// purely memory-bound at large N — every element requires 2 reads +
// 1 write, a single add/mul in between. They're worth benching
// separately from the unary math family because the arithmetic
// intensity is lower (no transcendental), so the DRAM plateau shows
// up sooner.

#include <numkit/builtin/lang/operators/binary_ops.hpp>
#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

numkit::Value makeReal(size_t n, uint32_t seed)
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

template <typename Fn>
void runBinaryBench(benchmark::State &state, Fn fn)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Value a = makeReal(n, 1);
    Value b = makeReal(n, 2);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        Value c = fn(alloc, a, b);
        benchmark::DoNotOptimize(c);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    // 2 reads + 1 write = 3 * sizeof(double) per element.
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            3 * static_cast<int64_t>(sizeof(double)));
}

} // namespace

static void BM_Plus(benchmark::State &s)  { runBinaryBench(s, numkit::builtin::plus);  }
static void BM_Times(benchmark::State &s) { runBinaryBench(s, numkit::builtin::times); }

BENCHMARK(BM_Plus)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);
BENCHMARK(BM_Times)->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN);

// ── Decomposed micro-benches ─────────────────────────────────
// `z = x + y` from a .m script does three things:
//   1. allocate a fresh N-element Value for the result
//   2. run the SIMD plusLoop into its buffer
//   3. wrap up + return through the VM
// To know which step matters, time them in isolation. The full
// path is BM_Plus above; subtract these two from it to see where
// the rest goes.

namespace numkit::builtin::detail {
// Forward-declare from libs/builtin/src/backends/binary_ops_loops.hpp.
// Linked from the same numkit library; bypasses the public
// plus()/elementwiseDouble() wrapping so we measure only the kernel.
void plusLoop(const double *a, const double *b, double *out, std::size_t n);
} // namespace numkit::builtin::detail

// Pure alloc: how long does it take to ask the engine allocator
// for a fresh N-element heap double? No data writes.
static void BM_PlusAlloc(benchmark::State &state)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : state) {
        Value c = Value::matrix(n, 1, ValueType::DOUBLE, &alloc);
        benchmark::DoNotOptimize(c);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) * sizeof(double));
}

// Pure kernel: pre-allocated output, no Value plumbing —
// just plusLoop on raw double buffers. Whatever Highway can pull
// off in steady state.
static void BM_PlusKernel(benchmark::State &state)
{
    using namespace numkit;
    const size_t n = static_cast<size_t>(state.range(0));
    Value a = makeReal(n, 1);
    Value b = makeReal(n, 2);
    Value c = Value::matrix(n, 1, ValueType::DOUBLE, nullptr);
    const double *ad = a.doubleData();
    const double *bd = b.doubleData();
    double       *cd = c.doubleDataMut();
    for (auto _ : state) {
        builtin::detail::plusLoop(ad, bd, cd, n);
        benchmark::DoNotOptimize(cd);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            3 * static_cast<int64_t>(sizeof(double)));
}

BENCHMARK(BM_PlusAlloc) ->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
BENCHMARK(BM_PlusKernel)->RangeMultiplier(4)->Range(1 << 10, 1 << 22);
