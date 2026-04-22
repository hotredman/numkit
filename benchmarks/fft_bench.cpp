// benchmarks/fft_bench.cpp
//
// Timed region covers only numkit::m::dsp::fft(). Input allocation
// and RNG fill happen in SetUp / per-iteration prologue and are NOT
// counted. Sizes are powers of two (the common case) across a range
// that spans the cache hierarchy on a typical workstation.

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>
#include <numkit/m/dsp/MDspFft.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace {

// Build a real column vector of length n filled with a deterministic
// pseudo-random sequence. Allocator is nullptr → engine-free path.
numkit::m::MValue makeRealSignal(size_t n)
{
    using namespace numkit::m;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    MValue x = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *data = x.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);
    return x;
}

// Same length, complex input — bypasses the rfft real-input fast path
// inside dsp::fft so the underlying complex r2/r4 kernel sees the full
// size N (not the rfft-halved N/2). Used to isolate where the cliff at
// N=32k actually lives: in the kernel or in the rfft wrapper.
numkit::m::MValue makeComplexSignal(size_t n)
{
    using namespace numkit::m;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    MValue x = MValue::complexMatrix(n, 1, nullptr);
    Complex *data = x.complexDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = Complex(dist(rng), dist(rng));
    return x;
}

} // namespace

static void BM_Fft_PowerOfTwo(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue x = makeRealSignal(n);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        MValue y = dsp::fft(alloc, x, /*n=*/-1, /*dim=*/1);
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

// 2^10 … 2^18 spans roughly L1 through L3 on a workstation.
// Every size is a power of 2 — split between pure powers of 4
// (1024, 4096, 16384, 65536, 262144) which hit the radix-4 kernel
// once its threshold is reached, and "odd log2" sizes (2048, 8192,
// 32768, 131072) which the mixed-radix r4+r2 kernel targets.
BENCHMARK(BM_Fft_PowerOfTwo)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18)
    ->Complexity(benchmark::oNLogN);

// Complex-input variant. dsp::fft() takes the standard complex r2/r4
// path here (no rfft halving), so timings reflect the kernel directly.
// Compare against BM_Fft_PowerOfTwo at half the size to attribute time
// to {rfft pack+twist} vs {complex kernel} — the standalone complex
// kernel timing should be lower than (real_at_2N - linear_overhead).
static void BM_Fft_PowerOfTwo_Complex(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    MValue x = makeComplexSignal(n);
    Allocator alloc = Allocator::defaultAllocator();

    for (auto _ : state) {
        MValue y = dsp::fft(alloc, x, /*n=*/-1, /*dim=*/1);
        benchmark::DoNotOptimize(y);
    }
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_PowerOfTwo_Complex)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18)
    ->Complexity(benchmark::oNLogN);
