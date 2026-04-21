// libs/dsp/src/MDspConv.cpp
//
// Public C++ API for convolution and friends. See MDspConv.hpp for
// contracts. Algorithms unchanged from the previous lambda form — only
// moved into named free functions that take Allocator& explicitly.

#include <numkit/m/dsp/MDspConv.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::m::dsp {

// ── conv ──────────────────────────────────────────────────────────────
MValue conv(Allocator &alloc, const MValue &a, const MValue &b, const std::string &shape)
{
    const size_t na = a.numel(), nb = b.numel();

    std::vector<double> c;
    if (na * nb > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
        c = convFFT(a.doubleData(), na, b.doubleData(), nb);
    else
        c = convDirect(a.doubleData(), na, b.doubleData(), nb);

    const size_t nc = c.size();
    size_t outStart = 0, outLen = nc;
    if (shape == "same") {
        outLen = std::max(na, nb);
        outStart = (nc - outLen) / 2;
    } else if (shape == "valid") {
        outLen = (na >= nb) ? na - nb + 1 : nb - na + 1;
        outStart = std::min(na, nb) - 1;
    } else if (shape != "full") {
        throw MError("conv: shape must be 'full', 'same', or 'valid'",
                     0, 0, "conv", "", "MATLAB:conv:badShape");
    }

    auto r = MValue::matrix(1, outLen, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < outLen; ++i)
        r.doubleDataMut()[i] = c[outStart + i];
    return r;
}

// ── deconv ────────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
deconv(Allocator &alloc, const MValue &b, const MValue &a)
{
    const size_t nb = b.numel(), na = a.numel();
    if (na > nb)
        throw MError("deconv: denominator longer than numerator",
                     0, 0, "deconv", "", "MATLAB:deconv:denomTooLong");

    std::vector<double> rem(b.doubleData(), b.doubleData() + nb);
    const double *ad = a.doubleData();

    const size_t nq = nb - na + 1;
    std::vector<double> q(nq);

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw MError("deconv: leading coefficient is zero",
                     0, 0, "deconv", "", "MATLAB:deconv:zeroLead");

    for (size_t i = 0; i < nq; ++i) {
        q[i] = rem[i] / a0;
        for (size_t j = 0; j < na; ++j)
            rem[i + j] -= q[i] * ad[j];
    }

    auto qv = MValue::matrix(1, nq, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nq; ++i)
        qv.doubleDataMut()[i] = q[i];

    auto rv = MValue::matrix(1, nb, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nb; ++i)
        rv.doubleDataMut()[i] = rem[i];

    return std::make_tuple(std::move(qv), std::move(rv));
}

// ── xcorr ─────────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
xcorr(Allocator &alloc, const MValue &x, const MValue &y)
{
    const double *xd = x.doubleData();
    const size_t nx = x.numel();
    const double *yd = y.doubleData();
    const size_t ny = y.numel();

    const size_t maxLen = std::max(nx, ny);
    const size_t nc = nx + ny - 1;

    std::vector<double> yRev(ny);
    for (size_t i = 0; i < ny; ++i)
        yRev[i] = yd[ny - 1 - i];

    std::vector<double> c;
    if (nx * ny > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
        c = convFFT(xd, nx, yRev.data(), ny);
    else
        c = convDirect(xd, nx, yRev.data(), ny);

    auto r = MValue::matrix(1, nc, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nc; ++i)
        r.doubleDataMut()[i] = c[i];

    const int maxLag = static_cast<int>(maxLen) - 1;
    auto lags = MValue::matrix(1, nc, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nc; ++i)
        lags.doubleDataMut()[i] = static_cast<double>(static_cast<int>(i) - maxLag);

    return std::make_tuple(std::move(r), std::move(lags));
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void conv_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("conv: requires at least 2 arguments",
                     0, 0, "conv", "", "MATLAB:conv:nargin");

    std::string shape = "full";
    if (args.size() >= 3 && args[2].isChar())
        shape = args[2].toString();

    outs[0] = conv(ctx.engine->allocator(), args[0], args[1], shape);
}

void deconv_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("deconv: requires 2 arguments",
                     0, 0, "deconv", "", "MATLAB:deconv:nargin");

    auto [q, r] = deconv(ctx.engine->allocator(), args[0], args[1]);
    outs[0] = std::move(q);
    if (nargout > 1)
        outs[1] = std::move(r);
}

void xcorr_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("xcorr: requires at least 1 argument",
                     0, 0, "xcorr", "", "MATLAB:xcorr:nargin");

    // Autocorrelation when called with a single arg, or when second
    // arg is a char flag like 'unbiased' (MATLAB compat: flag is accepted
    // but currently ignored — scaling mode not implemented).
    const bool autoCorr = (args.size() < 2 || args[1].isChar());

    std::tuple<MValue, MValue> result = autoCorr
        ? xcorr(ctx.engine->allocator(), args[0])
        : xcorr(ctx.engine->allocator(), args[0], args[1]);

    outs[0] = std::move(std::get<0>(result));
    if (nargout > 1)
        outs[1] = std::move(std::get<1>(result));
}

} // namespace detail

} // namespace numkit::m::dsp
