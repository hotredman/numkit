// src/ops/benchmarks/fused_bench.cpp
//
// Fused element-wise SIMD kernels (ops/fused) — the fixed-structure one-pass
// loops behind VM expression fusion. Benched at the KERNEL level (raw double
// buffers, pre-allocated output) so the portable-vs-Highway A/B isolates the
// SIMD win, not Value plumbing.
//
// One bench per distinct idiom shape (real-double). The many divide-inner
// (*ShiftDiv), min-outer-clamp and complex (*Cx) variants share the same perf
// profile as the representative below, so they're not benched separately.

#include <numkit/ops/fused/fused_kernels.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace {

std::vector<double> makeBuf(size_t n, uint32_t seed, double lo, double hi)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(lo, hi);
    std::vector<double> v(n);
    for (auto &x : v)
        x = dist(rng);
    return v;
}

void mark(benchmark::State &state, size_t n, int streams)
{
    state.SetComplexityN(static_cast<int64_t>(n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
    // `streams` array reads + 1 write, sizeof(double) each.
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(n) *
                            (streams + 1) * static_cast<int64_t>(sizeof(double)));
}

} // namespace

using numkit::ops::TransAffineFn;
using numkit::ops::UnaryAffineFn;

#define FUSED_RANGE ->RangeMultiplier(4)->Range(1 << 10, 1 << 22)->Complexity(benchmark::oN)

// ── single-array affine-family shapes ───────────────────────────────────────
static void BM_FusedAffine(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -1.0, 1.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedAffine(x.data(), 2.5, -1.0, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedAffineClamp(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -2.0, 2.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedAffineClamp(x.data(), 1.5, 0.2, 0.0, 1.0, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedShiftScaleMul(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -5.0, 5.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedShiftScaleMul(x.data(), 1.0, 0.5, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedAbsAffine(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -1.0, 1.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedAbsAffine(x.data(), 2.0, -0.5, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedSqAffine(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -1.0, 1.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedSqAffine(x.data(), 2.0, -0.5, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedSoftThreshold(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -2.0, 2.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedSoftThreshold(x.data(), 0.3, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedUnaryAffineSqrt(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, 0.0, 100.0);   // sqrt domain
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedUnaryAffine(x.data(), 1.0, 0.0, UnaryAffineFn::Sqrt, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

static void BM_FusedTransAffineExp(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -5.0, 5.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedTransAffine(x.data(), 1.0, 0.0, TransAffineFn::Exp, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 1);
}

// ── two-array shapes ────────────────────────────────────────────────────────
static void BM_FusedAxpby(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -1.0, 1.0);
    auto y = makeBuf(n, 2, -1.0, 1.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedAxpby(x.data(), 0.7, y.data(), 0.3, out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 2);
}

static void BM_FusedSqrtSumSq(benchmark::State &state)
{
    const size_t n = static_cast<size_t>(state.range(0));
    auto x = makeBuf(n, 1, -1.0, 1.0);
    auto y = makeBuf(n, 2, -1.0, 1.0);
    std::vector<double> out(n);
    for (auto _ : state) {
        numkit::ops::fusedSqrtSumSq(x.data(), y.data(), out.data(), n);
        benchmark::DoNotOptimize(out.data());
    }
    mark(state, n, 2);
}

BENCHMARK(BM_FusedAffine)          FUSED_RANGE;
BENCHMARK(BM_FusedAffineClamp)     FUSED_RANGE;
BENCHMARK(BM_FusedShiftScaleMul)   FUSED_RANGE;
BENCHMARK(BM_FusedAbsAffine)       FUSED_RANGE;
BENCHMARK(BM_FusedSqAffine)        FUSED_RANGE;
BENCHMARK(BM_FusedSoftThreshold)   FUSED_RANGE;
BENCHMARK(BM_FusedUnaryAffineSqrt) FUSED_RANGE;
BENCHMARK(BM_FusedTransAffineExp)  FUSED_RANGE;
BENCHMARK(BM_FusedAxpby)           FUSED_RANGE;
BENCHMARK(BM_FusedSqrtSumSq)       FUSED_RANGE;
