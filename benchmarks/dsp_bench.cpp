// benchmarks/dsp_bench.cpp
//
// Phase-10 sweep covering the unbenched DSP surface:
//   * Existing-but-previously-unbenched: filter, filtfilt, xcorr,
//     pwelch, hilbert.
//   * Phase 9 gaps: medfilt1, findpeaks, dct.
//
// Inputs are random real signals; lengths span from 1k (interactive
// fast feedback) to 64k (where the FFT-backed kernels start to show
// their cost).

#include <numkit/m/dsp/MDspConv.hpp>
#include <numkit/m/dsp/MDspFilter.hpp>
#include <numkit/m/dsp/MDspFilterDesign.hpp>
#include <numkit/m/dsp/MDspGaps.hpp>
#include <numkit/m/dsp/MDspSpectral.hpp>
#include <numkit/m/dsp/MDspTransform.hpp>
#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <benchmark/benchmark.h>

#include <random>

namespace {

using namespace numkit::m;

MValue makeSignal(size_t n, uint32_t seed = 7)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> d(0.0, 1.0);
    MValue v = MValue::matrix(n, 1, MType::DOUBLE, nullptr);
    double *p = v.doubleDataMut();
    for (size_t i = 0; i < n; ++i) p[i] = d(rng);
    return v;
}

// Build a small lowpass filter (length 32) to feed filter / filtfilt.
struct FilterCoeffs {
    MValue b;
    MValue a;
};

FilterCoeffs makeLowpass32()
{
    Allocator alloc = Allocator::defaultAllocator();
    auto b = dsp::fir1(alloc, 32, 0.25, "low");  // 33-tap FIR
    // a = [1] for FIR
    MValue a = MValue::matrix(1, 1, MType::DOUBLE, nullptr);
    a.doubleDataMut()[0] = 1.0;
    return {b, a};
}

} // namespace

// ── filter / filtfilt (FIR length 33) ──────────────────────

static void BM_FilterFIR33(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    auto coeffs = makeLowpass32();
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = dsp::filter(alloc, coeffs.b, coeffs.a, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_FilterFIR33)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

static void BM_FiltfiltFIR33(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    auto coeffs = makeLowpass32();
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = dsp::filtfilt(alloc, coeffs.b, coeffs.a, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_FiltfiltFIR33)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

// ── xcorr (FFT-backed for large N) ──────────────────────────

static void BM_Xcorr(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n, 1);
    auto y = makeSignal(n, 2);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto [c, lags] = dsp::xcorr(alloc, x, y);
        benchmark::DoNotOptimize(c);
        benchmark::DoNotOptimize(lags);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Xcorr)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

// ── pwelch (segmented, default window/overlap) ─────────────

static void BM_Pwelch(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    MValue emptyWin = MValue::matrix(0, 0, MType::DOUBLE, nullptr);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto [pxx, f] = dsp::pwelch(alloc, x, emptyWin, 0, 0);
        benchmark::DoNotOptimize(pxx);
        benchmark::DoNotOptimize(f);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Pwelch)->RangeMultiplier(4)->Range(1 << 12, 1 << 18);

// ── hilbert (FFT-backed) ────────────────────────────────────

static void BM_Hilbert(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = dsp::hilbert(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Hilbert)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

// ── Phase 9: medfilt1 / findpeaks / dct ────────────────────

static void BM_Medfilt1_K7(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = dsp::medfilt1(alloc, x, 7);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Medfilt1_K7)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

static void BM_Findpeaks(benchmark::State &s)
{
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto [v, idx] = dsp::findpeaks(alloc, x);
        benchmark::DoNotOptimize(v);
        benchmark::DoNotOptimize(idx);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Findpeaks)->RangeMultiplier(4)->Range(1 << 10, 1 << 18);

static void BM_DCT(benchmark::State &s)
{
    // DCT is direct O(N^2); cap range much lower than the FFT-backed kernels.
    const size_t n = static_cast<size_t>(s.range(0));
    auto x = makeSignal(n);
    Allocator alloc = Allocator::defaultAllocator();
    for (auto _ : s) {
        auto y = dsp::dct(alloc, x);
        benchmark::DoNotOptimize(y);
    }
    s.SetComplexityN(static_cast<int64_t>(n));
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_DCT)->RangeMultiplier(2)->Range(64, 2048)->Complexity(benchmark::oNSquared);
