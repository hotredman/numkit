// libs/signal/src/digital_filtering/sosfilt.cpp
//
// Apply an SOS biquad cascade to a signal. Conversions
// zp2sos / tf2sos live in filter_implementation/conversions.cpp.

#include <numkit/signal/digital_filtering/sosfilt.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

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

// Same as biquadDf2t but with explicit initial state. Used for the
// forward + backward passes of sosfiltfilt to remove edge transients.
void biquadDf2tWithState(double b0, double b1, double b2,
                         double a1, double a2,
                         const double *x, double *y, size_t n,
                         double s1_init, double s2_init)
{
    double s1 = s1_init, s2 = s2_init;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i];
        const double yi = b0 * xi + s1;
        s1 = b1 * xi - a1 * yi + s2;
        s2 = b2 * xi - a2 * yi;
        y[i] = yi;
    }
}

// Steady-state initial conditions (s1, s2) for one biquad: when input
// is constant 1, applying these as initial state makes the output
// equal y_ss = (b0+b1+b2)/(1+a1+a2) from sample 0 (no transient).
//   zi[0] = (b1 + b2 - b0*(a1+a2)) / (1 + a1 + a2)
//   zi[1] = (b2 + a1*b2 - a2*b0 - a2*b1) / (1 + a1 + a2)
struct BiquadZi { double s1, s2; };
BiquadZi biquadZi(double b0, double b1, double b2,
                  double a1, double a2)
{
    const double denom = 1.0 + a1 + a2;
    if (std::abs(denom) < 1e-300) return {0.0, 0.0};
    const double s1 = (b1 + b2 - b0 * (a1 + a2)) / denom;
    const double s2 = (b2 + a1 * b2 - a2 * b0 - a2 * b1) / denom;
    return {s1, s2};
}

size_t validateSosMatrix(const Value &sos)
{
    if (sos.dims().ndim() != 2 || sos.dims().cols() != 6 || sos.dims().rows() == 0)
        throw Error("sosfilt: sos matrix must be L×6 with L >= 1",
                     0, 0, "sosfilt", "", "numkit:sosfilt:sosShape");
    if (sos.type() != ValueType::DOUBLE)
        throw Error("sosfilt: sos matrix must be DOUBLE",
                     0, 0, "sosfilt", "", "numkit:sosfilt:sosType");
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
                         0, 0, "sosfilt", "", "numkit:sosfilt:zeroLead");
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

Value sosfilt(const Value &sos, const Value &x, std::pmr::memory_resource *mr)
{
    const size_t L = validateSosMatrix(sos);
    if (x.type() != ValueType::DOUBLE)
        throw Error("sosfilt: signal x must be DOUBLE",
                     0, 0, "sosfilt", "", "numkit:sosfilt:xType");
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

// ── sosfiltfilt ──────────────────────────────────────────────────────
// Zero-phase forward+backward SOS cascade. Matches MATLAB filtfilt(d, x)
// when d is a digitalFilter SOS object.
//
// Algorithm:
//   1. Per-section steady-state initial conditions zi[s] = (zi0[s], zi1[s])
//      via biquadZi() so that a constant input produces a transient-free
//      output for that section.
//   2. Edge-reflect padding of length 6*L (where L = number of sections),
//      capped at nx - 1. This is scipy/MATLAB's standard "padlen".
//   3. Forward cascade: for each section, apply the biquad with its zi
//      scaled by the FIRST sample of the cascade input (the signal
//      entering the WHOLE cascade, not the per-section input). This is
//      the scipy/MATLAB approximation; it's exact for unit-DC-gain
//      filters and very close otherwise.
//   4. Reverse the cascade output.
//   5. Forward cascade again with each section's zi scaled by the
//      first sample of the (reversed) signal.
//   6. Reverse + trim.
namespace {

// Apply the SOS cascade to one signal column with optional zi
// initialisation per section. zi has length 2*L (interleaved: section
// 0's s1, s2; section 1's s1, s2; ...). If zi is null, zero-state.
void applyCascadeWithZi(const Value &sos, const double *src, double *dst,
                        size_t n, ScratchVec<double> &scratch,
                        const double *zi /* may be null */)
{
    const size_t L = sos.dims().rows();
    const double *p = sos.doubleData();
    auto readSection = [&](size_t r) {
        const double a0 = p[3 * L + r];
        if (a0 == 0.0)
            throw Error("sosfiltfilt: section a0 is zero",
                         0, 0, "sosfiltfilt", "", "numkit:sosfiltfilt:zeroLead");
        return std::array<double, 5>{
            p[0 * L + r] / a0, p[1 * L + r] / a0, p[2 * L + r] / a0,
            p[4 * L + r] / a0, p[5 * L + r] / a0,
        };
    };
    const double *currentSrc = src;
    double *currentDst = dst;
    for (size_t s = 0; s < L; ++s) {
        const auto c = readSection(s);
        if (zi) {
            biquadDf2tWithState(c[0], c[1], c[2], c[3], c[4],
                                currentSrc, currentDst, n,
                                zi[2 * s + 0], zi[2 * s + 1]);
        } else {
            biquadDf2t(c[0], c[1], c[2], c[3], c[4],
                       currentSrc, currentDst, n);
        }
        if (s + 1 < L) {
            if (currentDst == dst) {
                currentSrc = dst; currentDst = scratch.data();
            } else {
                currentSrc = scratch.data(); currentDst = dst;
            }
        }
    }
    if (currentDst != dst)
        std::copy(scratch.begin(), scratch.begin() + n, dst);
}

void sosfiltfiltColumn(const Value &sos, const double *xs, double *out,
                       size_t n, std::pmr::memory_resource *mr)
{
    const size_t L = sos.dims().rows();
    if (n == 0) return;
    if (L == 0) {
        std::memcpy(out, xs, n * sizeof(double));
        return;
    }

    ScratchArena local(mr);
    auto cascadeBuf = ScratchVec<double>(n, &local);
    auto ziVec      = ScratchVec<double>(2 * L, &local);

    const double *p = sos.doubleData();
    auto readNorm = [&](size_t s, double &b0, double &b1, double &b2,
                                  double &a1, double &a2) {
        const double a0 = p[3 * L + s];
        b0 = p[0 * L + s] / a0;
        b1 = p[1 * L + s] / a0;
        b2 = p[2 * L + s] / a0;
        a1 = p[4 * L + s] / a0;
        a2 = p[5 * L + s] / a0;
    };

    // Compute per-section unit-input zi (independent of x).
    for (size_t s = 0; s < L; ++s) {
        double b0, b1, b2, a1, a2;
        readNorm(s, b0, b1, b2, a1, a2);
        const auto z = biquadZi(b0, b1, b2, a1, a2);
        ziVec[2 * s + 0] = z.s1;
        ziVec[2 * s + 1] = z.s2;
    }

    // Edge-reflect padding length: 6*L (matches scipy default).
    size_t edge = 6 * L;
    if (edge >= n) edge = (n > 0) ? n - 1 : 0;

    const size_t extLen = n + 2 * edge;
    auto ext = ScratchVec<double>(extLen, &local);
    // Reflect across each endpoint, MATLAB convention:
    //   left  pad: 2*x[0]   - x[edge - i]    for i = 0..edge-1
    //   right pad: 2*x[n-1] - x[n-2 - i]     for i = 0..edge-1
    for (size_t i = 0; i < edge; ++i)
        ext[i] = 2.0 * xs[0] - xs[edge - i];
    std::memcpy(ext.data() + edge, xs, n * sizeof(double));
    for (size_t i = 0; i < edge; ++i)
        ext[edge + n + i] = 2.0 * xs[n - 1] - xs[n - 2 - i];

    auto fwd = ScratchVec<double>(extLen, &local);
    auto bwd = ScratchVec<double>(extLen, &local);
    auto cascadeScratch = ScratchVec<double>(extLen, &local);

    // Per-section input scale for steady state. Section s sees an
    // input whose DC level equals
    //   x_input * prod_{j < s} dcGain[j]
    // where dcGain[j] = sum(b)/sum(a) of section j. So zi for
    // section s must be scaled by that accumulated DC level.
    auto dcGain = ScratchVec<double>(L, &local);
    for (size_t s = 0; s < L; ++s) {
        double b0, b1, b2, a1, a2;
        readNorm(s, b0, b1, b2, a1, a2);
        const double num = b0 + b1 + b2;
        const double den = 1.0 + a1 + a2;
        dcGain[s] = (std::abs(den) > 1e-300) ? (num / den) : 1.0;
    }

    auto ziForward  = ScratchVec<double>(2 * L, &local);
    auto ziBackward = ScratchVec<double>(2 * L, &local);
    auto scaleZi = [&](double cascadeInput, ScratchVec<double> &out_zi) {
        double scale = cascadeInput;
        for (size_t s = 0; s < L; ++s) {
            out_zi[2 * s + 0] = ziVec[2 * s + 0] * scale;
            out_zi[2 * s + 1] = ziVec[2 * s + 1] * scale;
            scale *= dcGain[s];   // input to next section is this section's output
        }
    };

    scaleZi(ext[0], ziForward);
    applyCascadeWithZi(sos, ext.data(), fwd.data(), extLen,
                       cascadeScratch, ziForward.data());

    // Reverse forward output.
    std::reverse(fwd.begin(), fwd.end());
    scaleZi(fwd[0], ziBackward);
    applyCascadeWithZi(sos, fwd.data(), bwd.data(), extLen,
                       cascadeScratch, ziBackward.data());

    std::reverse(bwd.begin(), bwd.end());

    // Trim padding.
    std::memcpy(out, bwd.data() + edge, n * sizeof(double));
}

} // namespace

Value sosfiltfilt(const Value &sos, const Value &x, std::pmr::memory_resource *mr)
{
    validateSosMatrix(sos);
    if (x.type() != ValueType::DOUBLE)
        throw Error("sosfiltfilt: signal x must be DOUBLE",
                     0, 0, "sosfiltfilt", "", "numkit:sosfiltfilt:xType");
    if (x.isEmpty())
        return createLike(x, ValueType::DOUBLE, mr);

    auto out = createLike(x, ValueType::DOUBLE, mr);
    if (x.dims().isVector() || x.isScalar()) {
        sosfiltfiltColumn(sos, x.doubleData(), out.doubleDataMut(),
                          x.numel(), mr);
        return out;
    }
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    for (size_t c = 0; c < cols; ++c)
        sosfiltfiltColumn(sos, src + c * rows, dst + c * rows, rows, mr);
    return out;
}

namespace detail {

void sosfilt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sosfilt: requires (sos, x)",
                     0, 0, "sosfilt", "", "numkit:sosfilt:nargin");
    outs[0] = sosfilt(args[0], args[1], ctx.engine->resource());
}

void sosfiltfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sosfiltfilt: requires (sos, x)",
                     0, 0, "sosfiltfilt", "", "numkit:sosfiltfilt:nargin");
    outs[0] = sosfiltfilt(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
