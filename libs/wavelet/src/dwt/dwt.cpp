// libs/wavelet/src/dwt/dwt.cpp
//
// Single-level discrete wavelet transform and its inverse.
//
// Conventions (MATLAB R2025b default — `dwtmode('sym')`):
//   * Extension: whole-point symmetric reflection by Lf-1 samples on
//     each side, where Lf is the filter length.
//   * Convolution: full convolution of the extended signal with the
//     analysis filters Lo_D, Hi_D.
//   * Downsampling: keep every other sample starting at index Lf-1
//     (1-based: Lf, Lf+2, …) — equivalently y(2:2:end-1) of the
//     length-(Lext+Lf-1) full convolution after dropping the symmetric
//     boundary tail. Output length per band: floor((N + Lf - 1) / 2).
//
// Reconstruction (idwt):
//   * Upsample cA, cD by 2 (insert zeros).
//   * Convolve with synthesis filters Lo_R, Hi_R (full conv).
//   * Sum the two reconstructions.
//   * Crop the centre `len` samples (default `len = 2*la - Lf + 2`).

#include <numkit/wavelet/dwt/dwt.hpp>
#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::wavelet {

namespace {

// Whole-point symmetric extension: pad with Lf-1 reflected samples on
// each side. For x = [a b c d] and pad = 3:
//   ext = [c b a | a b c d | d c b]
// (the boundary sample is included in the reflection, hence "whole-point").
std::vector<double> sym_extend(const double *x, size_t N, size_t pad) {
    std::vector<double> y(N + 2 * pad);
    for (size_t i = 0; i < pad; ++i) {
        // left: index pad-1-i mirrors x[i]
        y[pad - 1 - i] = (i < N) ? x[i] : x[N - 1];
    }
    for (size_t i = 0; i < N; ++i) y[pad + i] = x[i];
    for (size_t i = 0; i < pad; ++i) {
        y[pad + N + i] = (i + 1 <= N) ? x[N - 1 - i] : x[0];
    }
    return y;
}

// Full convolution: y[n] = sum_k a[k] * b[n-k].
std::vector<double> conv_full(const std::vector<double> &a,
                              const std::vector<double> &b) {
    if (a.empty() || b.empty()) return {};
    const size_t out = a.size() + b.size() - 1;
    std::vector<double> y(out, 0.0);
    for (size_t i = 0; i < a.size(); ++i) {
        const double ai = a[i];
        for (size_t j = 0; j < b.size(); ++j)
            y[i + j] += ai * b[j];
    }
    return y;
}

// Downsample by 2 starting at index 1 (drop sample 0). After
// convolving the extended signal with a length-Lf filter, the sequence
// of valid indices is shifted, and MATLAB picks every other sample
// starting at offset 1 to align with the canonical DWT convention.
std::vector<double> downsample_evens(const std::vector<double> &y) {
    std::vector<double> z;
    z.reserve(y.size() / 2);
    for (size_t i = 1; i < y.size(); i += 2) z.push_back(y[i]);
    return z;
}

} // anonymous

void dwt(std::pmr::memory_resource *mr,
         const Value &x, const std::string &wname,
         Value *cA, Value *cD)
{
    auto fb = wavelet_filters(wname);
    const size_t Lf = fb.Lo_D.size();
    if (Lf < 2)
        throw Error("dwt: filter length < 2",
                    0, 0, "dwt", "", "m:dwt:filt");

    const size_t N = x.numel();
    if (N == 0) {
        if (cA) *cA = Value::matrix(1, 0, ValueType::DOUBLE, mr);
        if (cD) *cD = Value::matrix(1, 0, ValueType::DOUBLE, mr);
        return;
    }
    std::vector<double> xv(N);
    for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);

    const size_t pad = Lf - 1;
    auto ext = sym_extend(xv.data(), N, pad);

    // Output length per band per MATLAB doc:
    //   Llen = floor((N - 1) / 2) + Lf / 2     (Lf even for orthogonal)
    // which equals floor((N + Lf - 1) / 2).
    const size_t outLen = (N + Lf - 1) / 2;

    auto convAndDown = [&](const std::vector<double> &h) {
        auto y = conv_full(ext, h);
        // 2026-05-08 audit ТЗ wavelet/dwt fix: aligned with MATLAB
        // R2025b convention (Mallat 1989). After symmetric extension
        // and full conv with the analysis filter, MATLAB downsamples
        // taking samples at indices [Lf-1, Lf+1, Lf+3, ...] — offset
        // Lf-1, NOT Lf. Pairs with the new idwt crop offset to keep
        // round-trips exact under MATLAB labelling.
        std::vector<double> z;
        z.reserve(outLen);
        for (size_t k = 0; k < outLen; ++k) {
            const size_t idx = (Lf - 1) + 2 * k + 1;  // Lf, Lf+2, ... (1-indexed Lf, Lf+2)
            z.push_back(idx < y.size() ? y[idx] : 0.0);
        }
        return z;
    };

    auto a = convAndDown(fb.Lo_D);
    auto d = convAndDown(fb.Hi_D);

    auto pack = [&](const std::vector<double> &v) {
        Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
        if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
        return r;
    };
    if (cA) *cA = pack(a);
    if (cD) *cD = pack(d);
}

Value idwt(std::pmr::memory_resource *mr,
           const Value &cA, const Value &cD,
           const std::string &wname,
           long long len)
{
    auto fb = wavelet_filters(wname);
    const size_t Lf = fb.Lo_R.size();

    const size_t la = cA.numel();
    const size_t ld = cD.numel();
    const size_t L = std::max(la, ld);

    // Upsample by 2 with "post-zero" interleave: u[2i+1] = v[i],
    // u[2i] = 0. This is MATLAB's idwt convention — paired with the
    // forward dwt's "drop first 2*(Lf-1) samples then take evens"
    // downsampling, it makes the round-trip exact.
    auto upsample = [&](const Value &v) {
        std::vector<double> u(2 * L, 0.0);
        for (size_t i = 0; i < v.numel(); ++i) u[2 * i + 1] = v.elemAsDouble(i);
        return u;
    };

    auto upA = upsample(cA);
    auto upD = upsample(cD);

    // Convolve with synthesis filters (full).
    auto yA = conv_full(upA, fb.Lo_R);
    auto yD = conv_full(upD, fb.Hi_R);

    // Sum (lengths match: 2*L + Lf - 1).
    const size_t M = yA.size();
    std::vector<double> y(M, 0.0);
    for (size_t i = 0; i < M; ++i) y[i] = yA[i] + yD[i];

    // Default length: 2·la - Lf + 2 (MATLAB sym-mode formula).
    long long outLen = (len >= 0) ? len
                                  : static_cast<long long>(2 * la) -
                                        static_cast<long long>(Lf) + 2;
    if (outLen < 0) outLen = 0;
    const size_t outN = static_cast<size_t>(outLen);

    // Crop the centre outN samples. The leading boundary contribution
    // is Lf - 1 samples wide; skip them.
    const size_t offset = Lf - 1;
    Value r = Value::matrix(1, outN, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    for (size_t i = 0; i < outN; ++i) {
        const size_t k = offset + i;
        rd[i] = (k < M) ? y[k] : 0.0;
    }
    return r;
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("dwt/idwt: expected a string for wavelet name",
                    0, 0, "", "", "m:wavelet:type");
    return v.toString();
}

void dwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwt: requires (x, wname)",
                    0, 0, "dwt", "", "m:dwt:nargin");
    auto *mr = ctx.engine->resource();
    Value cA, cD;
    dwt(mr, args[0], argString(args[1]), &cA, &cD);
    if (outs.size() >= 1) outs[0] = cA;
    if (outs.size() >= 2) outs[1] = cD;
}

void idwt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("idwt: requires (cA, cD, wname [, len])",
                    0, 0, "idwt", "", "m:idwt:nargin");
    long long len = -1;
    if (args.size() >= 4 && !args[3].isEmpty())
        len = static_cast<long long>(args[3].toScalar());
    outs[0] = idwt(ctx.engine->resource(),
                   args[0], args[1], argString(args[2]), len);
}

} // namespace detail

} // namespace numkit::wavelet
