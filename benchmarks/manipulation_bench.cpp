// benchmarks/manipulation_bench.cpp
//
// Phase-10 sweep covering Phase 5-6 manipulation:
//   * Phase 5 lite:   repmat / fliplr / flipud / rot90 / circshift / tril / triu
//   * Phase 6 N-D:    permute / squeeze / cat / blkdiag
//
// All matrix-shape benches use square inputs (side × side) over a
// range that hits L1, L2/L3, and DRAM. Pure data-movement kernels;
// expect strong sensitivity to memory bandwidth.

#include <numkit/builtin/lang/arrays/manip.hpp>
#include <numkit/builtin/lang/arrays/matrix.hpp>
#include <numkit/builtin/lang/arrays/nd_manip.hpp>
#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

namespace {

using namespace numkit;

Value makeMat(size_t r, size_t c, uint32_t seed = 11)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    Value m = Value::matrix(r, c, ValueType::DOUBLE, nullptr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < r * c; ++i) p[i] = d(rng);
    return m;
}

Value make3D(size_t r, size_t c, size_t pages, uint32_t seed = 17)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-1.0, 1.0);
    Value m = Value::matrix3d(r, c, pages, ValueType::DOUBLE, nullptr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < r * c * pages; ++i) p[i] = d(rng);
    return m;
}

template <typename Fn>
void runSquareBench(benchmark::State &s, Fn fn)
{
    const size_t side = static_cast<size_t>(s.range(0));
    auto m = makeMat(side, side);
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    for (auto _ : s) {
        auto y = fn(mr, m);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(side * side));
    s.SetBytesProcessed(s.iterations() * static_cast<int64_t>(side * side) *
                        static_cast<int64_t>(sizeof(double)));
}

} // namespace

// ── Phase 5: flips / rotates / shifts ───────────────────────

static void BM_Fliplr   (benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::fliplr(a, x); }); }
static void BM_Flipud   (benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::flipud(a, x); }); }
static void BM_Rot90    (benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::rot90(a, x, 1); }); }
static void BM_Circshift(benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::circshift(a, x, 7, 13); }); }
static void BM_Tril     (benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::tril(a, x, 0); }); }
static void BM_Triu     (benchmark::State &s) { runSquareBench(s, [](auto &a, auto &x){ return builtin::triu(a, x, 0); }); }

BENCHMARK(BM_Fliplr)   ->RangeMultiplier(2)->Range(64, 2048);
BENCHMARK(BM_Flipud)   ->RangeMultiplier(2)->Range(64, 2048);
BENCHMARK(BM_Rot90)    ->RangeMultiplier(2)->Range(64, 2048);
BENCHMARK(BM_Circshift)->RangeMultiplier(2)->Range(64, 2048);
BENCHMARK(BM_Tril)     ->RangeMultiplier(2)->Range(64, 2048);
BENCHMARK(BM_Triu)     ->RangeMultiplier(2)->Range(64, 2048);

// repmat: tile a small matrix into a large one — bandwidth-bound.
static void BM_RepmatTile(benchmark::State &s)
{
    const size_t tiles = static_cast<size_t>(s.range(0));
    auto m = makeMat(64, 64);
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    for (auto _ : s) {
        auto y = builtin::repmat(mr, m, tiles, tiles);
        benchmark::DoNotOptimize(y);
    }
    const size_t totalElems = 64 * 64 * tiles * tiles;
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(totalElems));
    s.SetBytesProcessed(s.iterations() * static_cast<int64_t>(totalElems) *
                        static_cast<int64_t>(sizeof(double)));
}
BENCHMARK(BM_RepmatTile)->RangeMultiplier(2)->Range(2, 32);

// ── Phase 6: permute / squeeze / cat / blkdiag ─────────────

static void BM_PermuteTranspose(benchmark::State &s)
{
    // permute(M, [2 1]) on a square matrix == transpose. Worst-case
    // strided access pattern.
    const size_t side = static_cast<size_t>(s.range(0));
    auto m = makeMat(side, side);
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    const int perm[] = {2, 1};
    for (auto _ : s) {
        auto y = builtin::permute(mr, m, perm, 2);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(side * side));
}
BENCHMARK(BM_PermuteTranspose)->RangeMultiplier(2)->Range(64, 2048);

static void BM_Permute3D(benchmark::State &s)
{
    // permute(A, [3 1 2]) on a 3D R×C×P; output dims rotate.
    const size_t side = static_cast<size_t>(s.range(0));
    auto m = make3D(side, side, side);
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    const int perm[] = {3, 1, 2};
    for (auto _ : s) {
        auto y = builtin::permute(mr, m, perm, 3);
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(side * side * side));
}
BENCHMARK(BM_Permute3D)->RangeMultiplier(2)->Range(16, 128);

static void BM_CatDim3(benchmark::State &s)
{
    // Stack n 2D matrices into a 3D array.
    const size_t n = static_cast<size_t>(s.range(0));
    std::vector<Value> mats;
    mats.reserve(n);
    for (size_t i = 0; i < n; ++i)
        mats.push_back(makeMat(256, 256, 100 + static_cast<uint32_t>(i)));
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    for (auto _ : s) {
        auto y = builtin::cat(mr, 3, mats.data(), mats.size());
        benchmark::DoNotOptimize(y);
    }
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(256 * 256 * n));
}
BENCHMARK(BM_CatDim3)->Arg(4)->Arg(16)->Arg(64);

static void BM_Blkdiag(benchmark::State &s)
{
    // n square blocks on the diagonal; output is sum-side × sum-side.
    const size_t blocks = static_cast<size_t>(s.range(0));
    std::vector<Value> mats;
    mats.reserve(blocks);
    for (size_t i = 0; i < blocks; ++i)
        mats.push_back(makeMat(64, 64, 200 + static_cast<uint32_t>(i)));
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    for (auto _ : s) {
        auto y = builtin::blkdiag(mr, mats.data(), mats.size());
        benchmark::DoNotOptimize(y);
    }
    const size_t side = 64 * blocks;
    s.SetItemsProcessed(s.iterations() * static_cast<int64_t>(side * side));
}
BENCHMARK(BM_Blkdiag)->Arg(4)->Arg(16)->Arg(64);
