// libs/builtin/src/backends/MStdCumSum_simd.cpp
//
// Highway dynamic-dispatch prefix-sum (Hillis-Steele) for cumsum.
// Per N-lane SIMD vector: log2(N) `(shift + add)` steps to compute the
// inclusive prefix sum within the vector, then add a broadcasted carry
// from the previous vector's last lane. For AVX2 (4 lanes/double) that's
// 2 in-vector steps + 1 carry add per vector. AVX-512 (8 lanes) → 3 + 1.
//
// The scalar loop has a serial dependency `s += x[i]; r[i] = s;` so
// even at L1-residency it can't issue more than one add per cycle.
// SIMD scan breaks that chain — the carry add is the only inter-vector
// dependency, and per-vector compute happens in parallel.
//
// Bandwidth floor for N=1M doubles is ~1.6 ms (8 MB read + 8 MB write
// at ~10 GB/s). Pre-SIMD measured ~3.7 ms, so there's ~2× of headroom
// before bandwidth dominates. This kernel aims for ~2 ms.

#include "MStdCumSum.hpp"

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdCumSum_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// In-vector inclusive prefix sum via Hillis-Steele. After this, each
// lane holds the sum of itself and all lower-indexed lanes within the
// vector. Specialised at compile time on `MaxLanes(d)` so per-target
// instantiation only includes the shifts that fit the vector — without
// `if constexpr` here, MSVC tries to instantiate `ShiftLeftLanes<4>`
// even on SSE2's 2-lane double, where shift > vector size triggers a
// static_assert.
template <class D>
HWY_INLINE auto inVectorScan(D d, hn::VFromD<D> v)
{
    constexpr std::size_t N = hn::MaxLanes(d);
    if constexpr (N >= 2) v = hn::Add(v, hn::ShiftLeftLanes<1>(d, v));
    if constexpr (N >= 4) v = hn::Add(v, hn::ShiftLeftLanes<2>(d, v));
    if constexpr (N >= 8) v = hn::Add(v, hn::ShiftLeftLanes<4>(d, v));
    // 16+ lane double doesn't exist on any current target.
    return v;
}

void CumsumScanLoop(const double *HWY_RESTRICT src, double *HWY_RESTRICT dst,
                    std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    double carry = 0.0;
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, src + i);
        const auto scanned = inVectorScan(d, v);
        // Broadcast the running carry from the previous vector and add
        // to every lane, giving the global inclusive prefix.
        const auto carriedV = hn::Add(scanned, hn::Set(d, carry));
        hn::StoreU(carriedV, d, dst + i);
        // Last lane of carriedV becomes the new carry.
        carry = hn::ExtractLane(carriedV, N - 1);
    }
    for (; i < n; ++i) {
        carry += src[i];
        dst[i] = carry;
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::m::builtin {

HWY_EXPORT(CumsumScanLoop);

void cumsumScan(const double *src, double *dst, std::size_t n)
{
    if (n == 0) return;
    HWY_DYNAMIC_DISPATCH(CumsumScanLoop)(src, dst, n);
}

} // namespace numkit::m::builtin
#endif // HWY_ONCE
