// libs/builtin/src/backends/MStdAbs_simd.cpp
//
// Highway dynamic-dispatch version of abs(). This file is re-included
// by <hwy/foreach_target.h> once per SIMD target; the per-target copy
// lives inside numkit::m::builtin::HWY_NAMESPACE, and the final
// HWY_ONCE pass at the bottom publishes the single external abs().
// See MStdAbs_portable.cpp for the scalar reference.

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

// ── Highway dynamic-dispatch boilerplate ────────────────────────────────
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdAbs_simd.cpp"
#include <hwy/foreach_target.h>   // includes this file once per target
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Per-target hot loop. Only operates on raw double buffers; the MValue
// plumbing (complex / scalar / shape preservation) lives outside the
// target namespace because it doesn't benefit from vectorisation.
void AbsLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Abs(v), d, out + i);
    }
    for (; i < n; ++i)
        out[i] = std::fabs(in[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

// ── Single external definition (only compiled on the final pass) ────────
#if HWY_ONCE

namespace numkit::m::builtin {

HWY_EXPORT(AbsLoop);

MValue abs(Allocator &alloc, const MValue &x)
{
    // Complex goes through scalar std::abs(Complex) — SIMD double-lane
    // implementations don't help here (sqrt of sum-of-squares with
    // cancellation pitfalls). Keep the reference path for correctness.
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, MType::DOUBLE, &alloc);
        for (std::size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }

    if (x.isScalar())
        return MValue::scalar(std::fabs(x.toScalar()), &alloc);

    auto r = createLike(x, MType::DOUBLE, &alloc);
    HWY_DYNAMIC_DISPATCH(AbsLoop)(x.doubleData(), r.doubleDataMut(), x.numel());
    return r;
}

} // namespace numkit::m::builtin

#endif // HWY_ONCE
