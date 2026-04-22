// libs/dsp/src/MDspGaps.cpp
//
// Phase-9 DSP gaps: medfilt1, findpeaks, goertzel, dct, idct.
// Direct O(...) reference implementations — clear over fast. dct/idct
// run O(N²); FFT-based O(N log N) is doable but the bench phase will
// guide whether that's worth the complexity.

#include <numkit/m/dsp/MDspGaps.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"
#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::m::dsp {

// ────────────────────────────────────────────────────────────────────
// medfilt1
// ────────────────────────────────────────────────────────────────────
//
// Window length k is centered on each sample. For even k MATLAB places
// the center off by half a sample; we follow MATLAB's convention by
// using floor((k-1)/2) elements on the left and ceil((k-1)/2) on the
// right (matches MATLAB R2024b 'truncate' mode for both odd and even k).
//
// At the boundaries the window is truncated rather than zero-padded,
// so output length always equals input length.
MValue medfilt1(Allocator &alloc, const MValue &x, size_t k)
{
    if (k == 0)
        throw MError("medfilt1: window length must be >= 1",
                     0, 0, "medfilt1", "", "m:medfilt1:badK");

    const size_t n = x.numel();
    auto r = createLike(x, MType::DOUBLE, &alloc);
    if (n == 0) return r;

    const size_t leftHalf  = (k - 1) / 2;
    const size_t rightHalf = k / 2;

    std::vector<double> win;
    win.reserve(k);

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const size_t lo = (i >= leftHalf) ? (i - leftHalf) : 0;
        const size_t hi = std::min(n, i + rightHalf + 1);
        win.assign(src + lo, src + hi);
        const size_t mid = win.size() / 2;
        std::nth_element(win.begin(), win.begin() + mid, win.end());
        if (win.size() % 2 == 1) {
            dst[i] = win[mid];
        } else {
            // Even-length window: average the two middle values. The
            // upper one is win[mid] after nth_element; the lower is the
            // max of the [0..mid) prefix.
            const double upper = win[mid];
            const double lower = *std::max_element(win.begin(), win.begin() + mid);
            dst[i] = 0.5 * (lower + upper);
        }
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// findpeaks
// ────────────────────────────────────────────────────────────────────
//
// Strict local maximum: x[i-1] < x[i] > x[i+1]. NaN is never a peak.
// First and last samples are skipped (no left/right neighbour).
std::tuple<MValue, MValue>
findpeaks(Allocator &alloc, const MValue &x)
{
    std::vector<double> peakVals;
    std::vector<size_t> peakIdx;
    const size_t n = x.numel();
    if (n >= 3) {
        const double *p = x.doubleData();
        for (size_t i = 1; i + 1 < n; ++i) {
            const double v = p[i];
            if (std::isnan(v) || std::isnan(p[i - 1]) || std::isnan(p[i + 1]))
                continue;
            if (v > p[i - 1] && v > p[i + 1]) {
                peakVals.push_back(v);
                peakIdx.push_back(i);
            }
        }
    }
    auto vals = MValue::matrix(1, peakVals.size(), MType::DOUBLE, &alloc);
    auto idxs = MValue::matrix(1, peakIdx.size(), MType::DOUBLE, &alloc);
    for (size_t i = 0; i < peakVals.size(); ++i) {
        vals.doubleDataMut()[i] = peakVals[i];
        idxs.doubleDataMut()[i] = static_cast<double>(peakIdx[i] + 1);  // 1-based
    }
    return std::make_tuple(std::move(vals), std::move(idxs));
}

// ────────────────────────────────────────────────────────────────────
// goertzel
// ────────────────────────────────────────────────────────────────────
//
// Goertzel computes a single DFT bin via a 2nd-order IIR. For each
// 1-based bin index k in `ind` (1 == DC, 2 == lowest non-DC, ..., N
// == highest), output[k] = sum_n x[n] * exp(-2πi (k-1) n / N).
//
// Two-coefficient form. Numerically stable for reasonable N; matches
// MATLAB's `goertzel(x, ind)` to FP roundoff.
MValue goertzel(Allocator &alloc, const MValue &x, const MValue &ind)
{
    const size_t N = x.numel();
    const size_t M = ind.numel();
    auto r = MValue::complexMatrix(ind.dims().rows(), ind.dims().cols(), &alloc);

    if (N == 0) return r;

    const double *xd = x.doubleData();
    for (size_t m = 0; m < M; ++m) {
        const double k1based = ind.doubleData()[m];
        const double k0based = k1based - 1.0;
        const double w = 2.0 * M_PI * k0based / static_cast<double>(N);
        const double cw = std::cos(w);
        const double sw = std::sin(w);
        const double coeff = 2.0 * cw;

        double s_prev = 0.0, s_prev2 = 0.0;
        for (size_t n = 0; n < N; ++n) {
            const double s = xd[n] + coeff * s_prev - s_prev2;
            s_prev2 = s_prev;
            s_prev  = s;
        }
        // Recursive output before phase correction:
        //   y = s_prev - exp(-jw) * s_prev2
        //     = (s_prev - cos(w)*s_prev2) + j*sin(w)*s_prev2
        // True DFT bin X[k] = y * exp(-jw*(N-1)).
        const double y_re = s_prev - cw * s_prev2;
        const double y_im = sw * s_prev2;
        const double pcw = std::cos(w * static_cast<double>(N - 1));
        const double psw = std::sin(w * static_cast<double>(N - 1));
        // (y_re + j*y_im) * (pcw - j*psw)
        const double out_re = y_re * pcw + y_im * psw;
        const double out_im = y_im * pcw - y_re * psw;
        r.complexDataMut()[m] = Complex(out_re, out_im);
    }
    return r;
}

// ────────────────────────────────────────────────────────────────────
// dct / idct (Type-II, normalised — MATLAB default)
// ────────────────────────────────────────────────────────────────────
//
// dct:  X[k] = w[k] * sum_n x[n] * cos(pi (2n+1) k / (2N))
// idct: x[n] = sum_k w[k] * X[k] * cos(pi (2n+1) k / (2N))
//   where w[0] = sqrt(1/N), w[k>0] = sqrt(2/N).
//
// Direct O(N²); FFT-based path can be added later if benches show
// dct hot. For Phase 9 the reference is the contract.
MValue dct(Allocator &alloc, const MValue &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, MType::DOUBLE, &alloc);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    for (size_t k = 0; k < N; ++k) {
        double acc = 0.0;
        const double angK = piOver2N * static_cast<double>(k);
        for (size_t n = 0; n < N; ++n)
            acc += xd[n] * std::cos(angK * static_cast<double>(2 * n + 1));
        X[k] = (k == 0 ? w0 : wk) * acc;
    }
    return r;
}

MValue idct(Allocator &alloc, const MValue &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, MType::DOUBLE, &alloc);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    for (size_t n = 0; n < N; ++n) {
        double acc = w0 * Xd[0];  // k=0 term separated for the w0 weight
        const double angN = piOver2N * static_cast<double>(2 * n + 1);
        for (size_t k = 1; k < N; ++k)
            acc += wk * Xd[k] * std::cos(angN * static_cast<double>(k));
        xt[n] = acc;
    }
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

void medfilt1_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw MError("medfilt1: requires at least 1 argument",
                     0, 0, "medfilt1", "", "m:medfilt1:nargin");
    size_t k = 3;
    if (args.size() >= 2 && !args[1].isEmpty())
        k = static_cast<size_t>(args[1].toScalar());
    outs[0] = medfilt1(ctx.engine->allocator(), args[0], k);
}

void findpeaks_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw MError("findpeaks: requires 1 argument",
                     0, 0, "findpeaks", "", "m:findpeaks:nargin");
    auto [vals, idxs] = findpeaks(ctx.engine->allocator(), args[0]);
    outs[0] = std::move(vals);
    if (nargout > 1) outs[1] = std::move(idxs);
}

void goertzel_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("goertzel: requires (x, ind)",
                     0, 0, "goertzel", "", "m:goertzel:nargin");
    outs[0] = goertzel(ctx.engine->allocator(), args[0], args[1]);
}

void dct_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw MError("dct: requires 1 argument",
                     0, 0, "dct", "", "m:dct:nargin");
    outs[0] = dct(ctx.engine->allocator(), args[0]);
}

void idct_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw MError("idct: requires 1 argument",
                     0, 0, "idct", "", "m:idct:nargin");
    outs[0] = idct(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::dsp
