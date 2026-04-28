// libs/builtin/src/math/elementary/backends/abs_simd.cpp
//
// Highway dynamic-dispatch version of abs(). This file is re-included
// by <hwy/foreach_target.h> once per SIMD target; the per-target copy
// lives inside numkit::builtin::HWY_NAMESPACE, and the final
// HWY_ONCE pass at the bottom publishes the single external abs().
// See abs_portable.cpp for the scalar reference.

#include <numkit/builtin/math/elementary/rounding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/parallel_for.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>

#include <hwy/cache_control.h>

// ── Highway dynamic-dispatch boilerplate ────────────────────────────────
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/abs_simd.cpp"
#include <hwy/foreach_target.h>   // includes this file once per target
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Per-target hot loop. Only operates on raw double buffers; the Value
// plumbing (complex / scalar / shape preservation) lives outside the
// target namespace because it doesn't benefit from vectorisation.
//
// Uses the same head/body/tail NT-store pattern as the binary ops in
// binary_ops_simd.cpp — see the comment block there for the
// rationale. abs is also a write-once-discard-rest pattern at large N
// so the cache-bypass payoff is similar.
void AbsLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N           = hn::Lanes(d);
    const std::size_t align_bytes = N * sizeof(double);

    std::size_t i = 0;
    const auto addr = reinterpret_cast<std::uintptr_t>(out);
    const auto rem  = addr % align_bytes;
    const std::size_t head = (rem == 0) ? 0
                                        : std::min<std::size_t>((align_bytes - rem) / sizeof(double), n);
    for (std::size_t k = 0; k < head; ++k, ++i)
        out[i] = std::fabs(in[i]);

    // 4× unrolled body — independent loads + ops + Stream stores.
    const std::size_t step4 = 4 * N;
    for (; i + step4 <= n; i += step4) {
        hn::Stream(hn::Abs(hn::LoadU(d, in + i + 0 * N)), d, out + i + 0 * N);
        hn::Stream(hn::Abs(hn::LoadU(d, in + i + 1 * N)), d, out + i + 1 * N);
        hn::Stream(hn::Abs(hn::LoadU(d, in + i + 2 * N)), d, out + i + 2 * N);
        hn::Stream(hn::Abs(hn::LoadU(d, in + i + 3 * N)), d, out + i + 3 * N);
    }
    for (; i + N <= n; i += N)
        hn::Stream(hn::Abs(hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i)
        out[i] = std::fabs(in[i]);
    // FlushStream issued once at the dispatcher level (see abs() below).
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

// ── Single external definition (only compiled on the final pass) ────────
#if HWY_ONCE

namespace numkit::builtin {

HWY_EXPORT(AbsLoop);

Value abs(Allocator &alloc, const Value &x, Value *hint)
{
    // Complex goes through scalar std::abs(Complex) — SIMD double-lane
    // implementations don't help here (sqrt of sum-of-squares with
    // cancellation pitfalls). Keep the reference path for correctness.
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, ValueType::DOUBLE, &alloc);
        for (std::size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }

    if (x.isScalar())
        return Value::scalar(std::fabs(x.toScalar()), &alloc);

    // Output-reuse fast path: caller-provided hint is a heap double of
    // matching shape with unique ownership — steal its buffer instead
    // of allocating fresh. Skips the per-call N-element alloc that
    // dominates at large N (1.8 ms at N=1M vs 0.5 ms for the kernel).
    Value r;
    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        r = std::move(*hint);
    } else {
        r = createLike(x, ValueType::DOUBLE, &alloc);
    }
    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    numkit::detail::parallel_for(x.numel(), numkit::detail::kCheapElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(AbsLoop)(in + s, out + s, e - s);
        }, numkit::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();   // single sfence after all NT-store chunks
    return r;
}

} // namespace numkit::builtin

#endif // HWY_ONCE
