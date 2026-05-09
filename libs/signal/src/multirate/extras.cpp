// libs/signal/src/multirate/extras.cpp
//
// upfirdn / interp / intfilt / fftfilt.
//
// fftfilt uses the FFT-conv helpers from dsp_helpers.hpp; the others
// reuse intfilt (a sinc-with-Hamming-window FIR) and the existing
// filter() / upsample() / downsample() primitives.

#include <numkit/signal/multirate/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/digital_filtering/filter.hpp>
#include <numkit/signal/multirate/multirate.hpp>

#include "../dsp_helpers.hpp"   // fftRadix2, fillFftTwiddles, nextPow2, Complex

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// 1-D real vector accessor that handles both row and column orientation.
double readReal(const Value &v, size_t i) { return v.elemAsDouble(i); }

// Allocate a real row/column vector matching the orientation of `like`.
Value sameOrientReal(const Value &like, size_t n, std::pmr::memory_resource *mr)
{
    const bool isRow = (like.dims().rows() == 1);
    return isRow
            ? Value::matrix(1, n, ValueType::DOUBLE, mr)
            : Value::matrix(n, 1, ValueType::DOUBLE, mr);
}

} // namespace

// ── intfilt (FIR design) ──────────────────────────────────────────────
Value intfilt(std::pmr::memory_resource *mr, size_t r, size_t n, double alpha)
{
    if (r < 1)
        throw Error("intfilt: r must be >= 1",
                     0, 0, "intfilt", "", "m:intfilt:badR");
    if (n < 1)
        throw Error("intfilt: n must be >= 1",
                     0, 0, "intfilt", "", "m:intfilt:badN");
    if (!(alpha > 0.0) || !(alpha <= 1.0))
        throw Error("intfilt: alpha must be in (0, 1]",
                     0, 0, "intfilt", "", "m:intfilt:badAlpha");

    // MATLAB intfilt(R, L, alpha): length = 2*R*L - 1. Numkit uses a
    // Hamming-windowed sinc (not MATLAB's proprietary firgr/firls
    // equiripple), so coefficient VALUES differ -- but length matches
    // and DC gain is normalised to R so that the cascade
    //   upfirdn(x, h, R, 1)
    // recovers the original amplitude (the interp() upsampling
    // convention). Aligning bit-for-bit with MATLAB requires firls.
    if (2 * n * r < 1) return Value::matrix(1, 1, ValueType::DOUBLE, mr);
    const size_t L = 2 * n * r - 1;
    auto out = Value::matrix(1, L, ValueType::DOUBLE, mr);
    double *h = out.doubleDataMut();
    const double half = (static_cast<double>(L) - 1.0) / 2.0;
    const double wc = M_PI * alpha / static_cast<double>(r);
    double sum = 0.0;
    for (size_t i = 0; i < L; ++i) {
        const double k = static_cast<double>(i) - half;
        const double sinc = (std::abs(k) < 1e-12) ? wc / M_PI
                                                  : std::sin(wc * k) / (M_PI * k);
        const double win = 0.54 - 0.46 *
            std::cos(2.0 * M_PI * static_cast<double>(i) / (L - 1));
        h[i] = sinc * win;
        sum += h[i];
    }
    if (sum > 0.0) {
        const double scale = static_cast<double>(r) / sum;
        for (size_t i = 0; i < L; ++i) h[i] *= scale;
    }
    return out;
}

// ── upfirdn ───────────────────────────────────────────────────────────
Value upfirdn(std::pmr::memory_resource *mr, const Value &x, const Value &h,
              size_t p, size_t q)
{
    if (p < 1 || q < 1)
        throw Error("upfirdn: p and q must be >= 1",
                     0, 0, "upfirdn", "", "m:upfirdn:badPQ");
    auto upx = upsample(mr, x, p);             // length = nx * p
    auto a   = Value::scalar(1.0, mr);
    auto y   = filter(mr, h, a, upx);
    return downsample(mr, y, q);
}

// ── interp ────────────────────────────────────────────────────────────
Value interp(std::pmr::memory_resource *mr, const Value &x, size_t r,
             size_t n, double alpha)
{
    if (r == 1) {
        // Pass-through: copy x.
        auto out = sameOrientReal(x, x.numel(), mr);
        std::memcpy(out.doubleDataMut(), x.doubleData(), x.numel() * sizeof(double));
        return out;
    }
    auto h = intfilt(mr, r, n, alpha);
    auto y = upfirdn(mr, x, h, r, 1);
    // Trim group delay introduced by the symmetric FIR (length L=2nr+1
    // gives delay nr in upsampled samples). MATLAB's interp keeps the
    // first nx*r samples after the symmetric leading delay.
    const size_t target = x.numel() * r;
    const size_t leadDelay = n * r;
    auto trimmed = sameOrientReal(x, target, mr);
    double *dst = trimmed.doubleDataMut();
    const double *src = y.doubleData();
    const size_t srcN = y.numel();
    for (size_t i = 0; i < target; ++i) {
        const size_t j = i + leadDelay;
        dst[i] = (j < srcN) ? src[j] : 0.0;
    }
    return trimmed;
}

// ── fftfilt ───────────────────────────────────────────────────────────
// Overlap-add convolution. Choose block length L per typical heuristic:
// L ≈ 2 * nb (rounded up to power of 2). NFFT = L + nb - 1 → next power
// of 2.
Value fftfilt(std::pmr::memory_resource *mr, const Value &b, const Value &x, size_t nfft)
{
    const size_t nb = b.numel();
    const size_t nx = x.numel();
    if (nb == 0 || nx == 0)
        return sameOrientReal(x, 0, mr);

    size_t blockL;
    if (nfft >= nb) {
        blockL = nfft - nb + 1;
    } else {
        // Heuristic: block ≈ max(nb, 32), then NFFT next pow2 of L+nb-1.
        blockL = std::max<size_t>(nb, 32);
        nfft = nextPow2(blockL + nb - 1);
        blockL = nfft - nb + 1;
    }
    if (blockL < 1) blockL = 1;
    if (nfft < blockL + nb - 1)
        nfft = nextPow2(blockL + nb - 1);

    ScratchArena scratch(mr);
    // Precompute B(f) = FFT(b padded to nfft).
    auto Bf = ScratchVec<Complex>(nfft, &scratch);
    for (size_t i = 0; i < nb; ++i)
        Bf[i] = Complex(readReal(b, i), 0.0);
    auto W_fwd = ScratchVec<Complex>(nfft / 2, &scratch);
    auto W_inv = ScratchVec<Complex>(nfft / 2, &scratch);
    fillFftTwiddles(W_fwd.data(), nfft, +1);
    fillFftTwiddles(W_inv.data(), nfft, -1);
    fftRadix2(Bf.data(), nfft, W_fwd.data());

    auto out = sameOrientReal(x, nx, mr);
    double *dst = out.doubleDataMut();
    std::fill(dst, dst + nx, 0.0);

    auto Xf  = ScratchVec<Complex>(nfft, &scratch);
    const double invN = 1.0 / static_cast<double>(nfft);
    for (size_t pos = 0; pos < nx; pos += blockL) {
        const size_t L = std::min(blockL, nx - pos);
        std::fill(Xf.begin(), Xf.end(), Complex(0, 0));
        for (size_t i = 0; i < L; ++i)
            Xf[i] = Complex(readReal(x, pos + i), 0.0);
        fftRadix2(Xf.data(), nfft, W_fwd.data());
        for (size_t i = 0; i < nfft; ++i)
            Xf[i] *= Bf[i];
        fftRadix2(Xf.data(), nfft, W_inv.data());
        const size_t blockOut = L + nb - 1;
        for (size_t i = 0; i < blockOut; ++i) {
            const size_t outIdx = pos + i;
            if (outIdx < nx)
                dst[outIdx] += Xf[i].real() * invN;
        }
    }
    return out;
}

namespace detail {

void upfirdn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("upfirdn: requires (x, h, p[, q])",
                     0, 0, "upfirdn", "", "m:upfirdn:nargin");
    const size_t p = static_cast<size_t>(args[2].toScalar());
    const size_t q = (args.size() >= 4) ? static_cast<size_t>(args[3].toScalar()) : 1;
    outs[0] = upfirdn(ctx.engine->resource(), args[0], args[1], p, q);
}

void interp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("interp: requires (x, r[, n[, alpha]])",
                     0, 0, "interp", "", "m:interp:nargin");
    const size_t r = static_cast<size_t>(args[1].toScalar());
    const size_t n = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 4;
    const double alpha = (args.size() >= 4) ? args[3].toScalar() : 0.5;
    outs[0] = interp(ctx.engine->resource(), args[0], r, n, alpha);
}

void intfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("intfilt: requires (r[, n[, alpha]])",
                     0, 0, "intfilt", "", "m:intfilt:nargin");
    const size_t r = static_cast<size_t>(args[0].toScalar());
    const size_t n = (args.size() >= 2) ? static_cast<size_t>(args[1].toScalar()) : 4;
    const double alpha = (args.size() >= 3) ? args[2].toScalar() : 0.5;
    outs[0] = intfilt(ctx.engine->resource(), r, n, alpha);
}

void fftfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fftfilt: requires (b, x[, nfft])",
                     0, 0, "fftfilt", "", "m:fftfilt:nargin");
    const size_t nfft = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 0;
    outs[0] = fftfilt(ctx.engine->resource(), args[0], args[1], nfft);
}

} // namespace detail

} // namespace numkit::signal
