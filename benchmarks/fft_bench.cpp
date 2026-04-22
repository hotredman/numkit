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

// Private headers — exposed via libs/dsp/src include path in the
// benchmarks CMakeLists. Used by BM_Fft_KernelOnly_Complex to time
// just the inner radix-2 dispatch loop.
#include "MDspHelpers.hpp"
#include "backends/FftKernels.hpp"

#include <benchmark/benchmark.h>

#include <cstring>
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

// Kernel-only timing: hoists the FFT scratch buffers out of the timed
// loop so we measure only the radix-2 stages, not the per-call
// pmr_allocator + twiddle-table-fill overhead. Compare the per-N
// timing to BM_Fft_PowerOfTwo_Complex to attribute the cliff at
// N=32k (native) to either the kernel or the wrapper.
static void BM_Fft_KernelOnly_Complex(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));

    // Pre-fill the input + twiddle table once. The bench loop just
    // memcpy's input into a working buffer and runs the kernel.
    std::vector<Complex> input(n);
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> d(-1.0, 1.0);
        for (size_t i = 0; i < n; ++i)
            input[i] = Complex(d(rng), d(rng));
    }
    std::vector<Complex> work(n);
    std::vector<Complex> twid(n / 2);
    fillFftTwiddles(twid.data(), n, /*dir=*/+1);

    for (auto _ : state) {
        std::memcpy(work.data(), input.data(), n * sizeof(Complex));
        dsp::detail::fftRadix2Impl(work.data(), n, twid.data());
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_KernelOnly_Complex)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18);

// Stockham auto-sort kernel timing — same pre-allocated input/twiddle
// strategy as BM_Fft_KernelOnly_Complex, just calling the Stockham
// dispatcher directly. Compare side-by-side to see where Stockham's
// no-bit-reversal advantage wins vs the in-place r2 path.
static void BM_Fft_KernelOnly_Stockham(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));

    std::vector<Complex> input(n);
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> d(-1.0, 1.0);
        for (size_t i = 0; i < n; ++i)
            input[i] = Complex(d(rng), d(rng));
    }
    std::vector<Complex> work(n);
    std::vector<Complex> twid(n / 2);
    fillFftTwiddles(twid.data(), n, /*dir=*/+1);

    for (auto _ : state) {
        std::memcpy(work.data(), input.data(), n * sizeof(Complex));
        dsp::detail::fftStockhamDispatch(work.data(), n, twid.data());
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_KernelOnly_Stockham)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18);

// SoA r2 kernel — same in-place semantics as the AoS r2, but the
// kernel internally splits to real/imag arrays to eliminate the
// LoadInterleaved2 permute cost on AVX2.
static void BM_Fft_KernelOnly_R2SoA(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));

    std::vector<Complex> input(n);
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> d(-1.0, 1.0);
        for (size_t i = 0; i < n; ++i)
            input[i] = Complex(d(rng), d(rng));
    }
    std::vector<Complex> work(n);
    std::vector<Complex> twid(n / 2);
    fillFftTwiddles(twid.data(), n, /*dir=*/+1);

    for (auto _ : state) {
        std::memcpy(work.data(), input.data(), n * sizeof(Complex));
        dsp::detail::fftRadix2SoaDispatch(work.data(), n, twid.data());
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_KernelOnly_R2SoA)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18);

// ── Diagnostics: micro-benchmarks of FFT sub-costs ──────────────────────
//
// Used to attribute the kernel time to (1) bit-reversal pre-pass that
// r2 pays but Stockham skips, (2) per-call memcpy that Stockham pays
// when log2(N) is odd, and (3) per-stage twiddle-table fill. If
// bit-reversal cost is small, Stockham's headroom over r2 is small —
// which is why the 12-22× kernel win in literature doesn't materialise
// on our setup at moderate N.

// Bit-reverse pass alone — what r2 pays at the start of every call,
// and what Stockham eliminates. The expected upper bound on
// Stockham-vs-r2 win at any size is roughly this time / r2 total.
static void BM_Fft_BitReverseOnly(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<Complex> work(n);
    for (size_t i = 0; i < n; ++i)
        work[i] = Complex(double(i), 0.0);

    for (auto _ : state) {
        // Same in-place bit-reverse loop as MDspFft_r2_simd.cpp:bitReverse2
        // (the SIMD r2 kernel calls it once per FFT call).
        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(work[i], work[j]);
        }
        benchmark::DoNotOptimize(work.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_BitReverseOnly)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18);

// Bulk memcpy of N complex elements — what Stockham pays at the end
// of any call where log2(N) is odd (src ended up in scratch instead
// of the original buffer). For powers of 2 N ∈ {1k, 4k, 16k, 64k, 256k}
// log2 is even → no copy. For {2k, 8k, 32k, 128k} log2 is odd → copy
// is paid. So this bench gives the ceiling of the memcpy penalty.
static void BM_Fft_FinalMemcpyOnly(benchmark::State &state)
{
    using namespace numkit::m;
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<Complex> src(n), dst(n);
    for (size_t i = 0; i < n; ++i)
        src[i] = Complex(double(i), 0.0);

    for (auto _ : state) {
        std::memcpy(dst.data(), src.data(), n * sizeof(Complex));
        benchmark::DoNotOptimize(dst.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_Fft_FinalMemcpyOnly)
    ->RangeMultiplier(2)
    ->Range(1 << 10, 1 << 18);
