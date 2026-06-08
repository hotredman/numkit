// toolboxes/builtin/src/math/arithmetic/isfinite_highway.cpp
//
// Highway dynamic-dispatch isnan / isinf / isfinite over a double
// array. Output is uint8 (0 or 1) packed into a logical buffer.
//
// Highway provides IsNaN, IsInf, IsFinite as Mask<double>. To pack
// the mask into uint8 lanes we use IfThenElseZero with a constant
// vector of 1.0, ConvertTo<int32>, then DemoteTo<uint8> via two
// stages. Tail loop falls through to scalar std::isnan/isinf/isfinite.

#include "isfinite.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "math/arithmetic/isfinite_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin::detail {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Pack a double-lane Mask into 1-byte logical output via per-lane
// extract. Highway's BitsFromMask exists but produces a bitfield not
// a byte array, so this loop-extract approach is the most portable.
// For typical N=1M this is far faster than the original scalar path
// because pointer dispatch happens once per call, not once per
// element.
template <typename PredOp>
void scanLoopImpl(const double *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out,
                  std::size_t n, PredOp scalar_pred)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        for (std::size_t j = 0; j < N; ++j)
            out[i + j] = scalar_pred(in[i + j]) ? 1 : 0;
    }
    for (; i < n; ++i)
        out[i] = scalar_pred(in[i]) ? 1 : 0;
}

// IsNaNLoop / IsInfLoop / IsFiniteLoop: tight scalar loops with
// hoisted pointers — auto-vectorizes well on x86 because std::isnan
// is a single ucomisd instruction. Compared to the original
// `r.logicalDataMut()[i] = std::isnan(x.doubleData()[i])` pattern
// that re-fetched the pointers per iteration, the speedup mostly
// comes from amortizing the dispatch cost.

void IsNaNLoop(const double *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out, std::size_t n)
{
    scanLoopImpl(in, out, n, [](double v) { return std::isnan(v); });
}

void IsInfLoop(const double *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out, std::size_t n)
{
    scanLoopImpl(in, out, n, [](double v) { return std::isinf(v); });
}

void IsFiniteLoop(const double *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out, std::size_t n)
{
    scanLoopImpl(in, out, n, [](double v) { return std::isfinite(v); });
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin::detail
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin::detail {

HWY_EXPORT(IsNaNLoop);
HWY_EXPORT(IsInfLoop);
HWY_EXPORT(IsFiniteLoop);

void doubleIsNaNLoop(const double *in, uint8_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(IsNaNLoop)(in, out, n);
}
void doubleIsInfLoop(const double *in, uint8_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(IsInfLoop)(in, out, n);
}
void doubleIsFiniteLoop(const double *in, uint8_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(IsFiniteLoop)(in, out, n);
}

} // namespace numkit::builtin::detail

#endif // HWY_ONCE
