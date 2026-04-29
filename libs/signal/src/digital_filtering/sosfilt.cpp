// libs/signal/src/digital_filtering/sosfilt.cpp
//
// Apply an SOS biquad cascade to a signal. Conversions
// zp2sos / tf2sos live in filter_implementation/conversions.cpp.

#include <numkit/signal/digital_filtering/sosfilt.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <array>

namespace numkit::signal {

namespace {

// In-place: y[n] for one biquad section, single-channel signal.
// b/a are pre-normalised so a0 = 1.
//   y[n] = b0·x[n] + s1
//   s1   = b1·x[n] - a1·y[n] + s2
//   s2   = b2·x[n] - a2·y[n]
void biquadDf2t(double b0, double b1, double b2,
                double a1, double a2,
                const double *x, double *y, size_t n)
{
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i];
        const double yi = b0 * xi + s1;
        s1 = b1 * xi - a1 * yi + s2;
        s2 = b2 * xi - a2 * yi;
        y[i] = yi;
    }
}

size_t validateSosMatrix(const Value &sos)
{
    if (sos.dims().ndim() != 2 || sos.dims().cols() != 6 || sos.dims().rows() == 0)
        throw Error("sosfilt: sos matrix must be L×6 with L >= 1",
                     0, 0, "sosfilt", "", "m:sosfilt:sosShape");
    if (sos.type() != ValueType::DOUBLE)
        throw Error("sosfilt: sos matrix must be DOUBLE",
                     0, 0, "sosfilt", "", "m:sosfilt:sosType");
    return sos.dims().rows();
}

void applyCascade(const Value &sos, const double *xs, double *out, size_t n,
                  ScratchVec<double> &scratch)
{
    const size_t L = sos.dims().rows();
    const double *p = sos.doubleData();
    auto readSection = [&](size_t r) {
        const double a0 = p[3 * L + r];
        if (a0 == 0.0)
            throw Error("sosfilt: section a0 is zero",
                         0, 0, "sosfilt", "", "m:sosfilt:zeroLead");
        return std::array<double, 5>{
            p[0 * L + r] / a0,  // b0
            p[1 * L + r] / a0,  // b1
            p[2 * L + r] / a0,  // b2
            p[4 * L + r] / a0,  // a1
            p[5 * L + r] / a0,  // a2
        };
    };
    const double *src = xs;
    double *dst = out;
    for (size_t s = 0; s < L; ++s) {
        const auto c = readSection(s);
        biquadDf2t(c[0], c[1], c[2], c[3], c[4], src, dst, n);
        if (s + 1 < L) {
            if (dst == out) { src = out; dst = scratch.data(); }
            else            { src = scratch.data(); dst = out; }
        }
    }
    if (dst != out)
        std::copy(scratch.begin(), scratch.begin() + n, out);
}

} // namespace

Value sosfilt(std::pmr::memory_resource *mr, const Value &sos, const Value &x)
{
    const size_t L = validateSosMatrix(sos);
    if (x.type() != ValueType::DOUBLE)
        throw Error("sosfilt: signal x must be DOUBLE",
                     0, 0, "sosfilt", "", "m:sosfilt:xType");
    if (x.isEmpty())
        return createLike(x, ValueType::DOUBLE, mr);
    (void) L;

    auto out = createLike(x, ValueType::DOUBLE, mr);
    ScratchArena scratch(mr);
    if (x.dims().isVector() || x.isScalar()) {
        const size_t n = x.numel();
        auto buf = ScratchVec<double>(n, &scratch);
        applyCascade(sos, x.doubleData(), out.doubleDataMut(), n, buf);
        return out;
    }
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    auto buf = ScratchVec<double>(rows, &scratch);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    for (size_t c = 0; c < cols; ++c)
        applyCascade(sos, src + c * rows, dst + c * rows, rows, buf);
    return out;
}

namespace detail {

void sosfilt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sosfilt: requires (sos, x)",
                     0, 0, "sosfilt", "", "m:sosfilt:nargin");
    outs[0] = sosfilt(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::signal
