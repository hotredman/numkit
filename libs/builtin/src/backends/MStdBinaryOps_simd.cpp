// libs/builtin/src/backends/MStdBinaryOps_simd.cpp
//
// Highway dynamic-dispatch inner loops for plus / minus / times /
// rdivide on real double arrays. One HWY_EXPORT / HWY_DYNAMIC_DISPATCH
// pair per op — the public entry points in the same detail namespace
// forward to the dispatcher. The surrounding public plus() / minus()
// / times() / rdivide() in MStdBinaryOps.cpp are unchanged; they
// call these loops only for the 2D same-shape DOUBLE fast path.

#include "BinaryOpsLoops.hpp"

#include <numkit/m/core/MParallelFor.hpp>

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdBinaryOps_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void PlusLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
              double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Add(hn::LoadU(d, a + i), hn::LoadU(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] + b[i];
}

void MinusLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Sub(hn::LoadU(d, a + i), hn::LoadU(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] - b[i];
}

void TimesLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Mul(hn::LoadU(d, a + i), hn::LoadU(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] * b[i];
}

void RdivideLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
                 double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Div(hn::LoadU(d, a + i), hn::LoadU(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] / b[i];
}

// Vectorised SAXPY matrix multiply (column-major). Loop order matches
// the portable scalar reference — identical k-reduction order means the
// only numerical divergence comes from MulAdd being a fused op on
// FMA-capable targets (AVX2 and later), which is at most 0.5 ULP per
// accumulation tighter than scalar mul-then-add.
void MatmulLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
                double *HWY_RESTRICT c, std::size_t M, std::size_t N, std::size_t K)
{
    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);

    for (std::size_t j = 0; j < N; ++j) {
        double *c_col = c + j * M;
        // Zero the output column in SIMD-wide stores.
        {
            const auto zero = hn::Zero(d);
            std::size_t i = 0;
            for (; i + lanes <= M; i += lanes) hn::StoreU(zero, d, c_col + i);
            for (; i < M; ++i) c_col[i] = 0.0;
        }

        for (std::size_t k = 0; k < K; ++k) {
            const double bkj = b[j * K + k];
            const auto vb = hn::Set(d, bkj);
            const double *a_col = a + k * M;

            std::size_t i = 0;
            for (; i + lanes <= M; i += lanes) {
                const auto va = hn::LoadU(d, a_col + i);
                const auto vc = hn::LoadU(d, c_col + i);
                hn::StoreU(hn::MulAdd(va, vb, vc), d, c_col + i);
            }
            for (; i < M; ++i) c_col[i] += bkj * a_col[i];
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::builtin::detail {

HWY_EXPORT(PlusLoop);
HWY_EXPORT(MinusLoop);
HWY_EXPORT(TimesLoop);
HWY_EXPORT(RdivideLoop);
HWY_EXPORT(MatmulLoop);

// Each dispatcher wraps the per-target SIMD body in a parallel_for so
// big arrays split across worker threads. Each thread calls the same
// per-target loop on its disjoint [start, end) slice — output buffers
// don't overlap, so per-element semantics are bit-identical to the
// single-threaded path. Below kElementwiseThreshold (and on builds
// without NUMKIT_WITH_THREADS) parallel_for runs `fn(0, n)` directly,
// preserving the prior fast path.

void plusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(PlusLoop)(a + s, b + s, out + s, e - s);
        });
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(MinusLoop)(a + s, b + s, out + s, e - s);
        });
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(TimesLoop)(a + s, b + s, out + s, e - s);
        });
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(RdivideLoop)(a + s, b + s, out + s, e - s);
        });
}

void matmulDoubleLoop(const double *a, const double *b, double *c,
                      std::size_t M, std::size_t N, std::size_t K)
{
    HWY_DYNAMIC_DISPATCH(MatmulLoop)(a, b, c, M, N, K);
}

} // namespace numkit::m::builtin::detail

#endif // HWY_ONCE
