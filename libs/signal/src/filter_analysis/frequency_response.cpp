// libs/signal/src/filter_analysis/frequency_response.cpp
//
// Frequency-domain analysis of an existing filter — freqz / phasez /
// grpdelay. Filter design (butter / fir1) lives in
// filter_design/filter_design.cpp.

#include <numkit/m/signal/filter_analysis/frequency_response.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MDspHelpers.hpp"   // Complex typedef

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <tuple>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m::signal {

std::tuple<MValue, MValue>
freqz(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const size_t nb = b.numel(), na = a.numel();

    auto W = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    auto H = MValue::complexMatrix(npts, 1, &alloc);

    for (size_t k = 0; k < npts; ++k) {
        const double w = M_PI * k / (npts - 1);
        W.doubleDataMut()[k] = w;

        const Complex ejw(std::cos(w), -std::sin(w));
        Complex num(0, 0), den(0, 0);
        Complex ejwk(1, 0);
        for (size_t i = 0; i < nb; ++i) {
            num += bd[i] * ejwk;
            ejwk *= ejw;
        }
        ejwk = Complex(1, 0);
        for (size_t i = 0; i < na; ++i) {
            den += ad[i] * ejwk;
            ejwk *= ejw;
        }
        H.complexDataMut()[k] = num / den;
    }

    return std::make_tuple(std::move(H), std::move(W));
}

namespace {

// Local unwrap (default tolerance π) — keeps phasez free of inter-file
// dependencies on filter_analysis/unwrap.cpp's MValue-typed unwrap().
void unwrapInPlace(double *p, size_t n)
{
    if (n < 2) return;
    constexpr double kPi  = M_PI;
    constexpr double k2Pi = 2.0 * M_PI;
    double offset = 0.0;
    for (size_t i = 1; i < n; ++i) {
        const double diff = p[i] + offset - p[i - 1];
        if (diff >  kPi) offset -= k2Pi;
        else if (diff < -kPi) offset += k2Pi;
        p[i] += offset;
    }
}

} // namespace

std::tuple<MValue, MValue>
phasez(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    auto [H, W] = freqz(alloc, b, a, npts);
    auto phi = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    const Complex *hd = H.complexData();
    double *pd = phi.doubleDataMut();
    for (size_t k = 0; k < npts; ++k)
        pd[k] = std::atan2(hd[k].imag(), hd[k].real());
    unwrapInPlace(pd, npts);
    return std::make_tuple(std::move(phi), std::move(W));
}

std::tuple<MValue, MValue>
grpdelay(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    auto [phi, W] = phasez(alloc, b, a, npts);
    auto gd = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    const double *p = phi.doubleData();
    const double *w = W.doubleData();
    double *g = gd.doubleDataMut();
    if (npts == 0) return std::make_tuple(std::move(gd), std::move(W));
    if (npts == 1) {
        g[0] = 0.0;
        return std::make_tuple(std::move(gd), std::move(W));
    }
    g[0]        = -(p[1] - p[0])               / (w[1] - w[0]);
    g[npts - 1] = -(p[npts - 1] - p[npts - 2]) / (w[npts - 1] - w[npts - 2]);
    for (size_t k = 1; k + 1 < npts; ++k)
        g[k] = -(p[k + 1] - p[k - 1]) / (w[k + 1] - w[k - 1]);
    return std::make_tuple(std::move(gd), std::move(W));
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void freqz_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("freqz: requires at least 2 arguments",
                     0, 0, "freqz", "", "m:freqz:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;

    auto [H, W] = freqz(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(H);
    if (nargout > 1)
        outs[1] = std::move(W);
}

void phasez_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("phasez: requires at least 2 arguments",
                     0, 0, "phasez", "", "m:phasez:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [phi, W] = phasez(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(phi);
    if (nargout > 1) outs[1] = std::move(W);
}

void grpdelay_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("grpdelay: requires at least 2 arguments",
                     0, 0, "grpdelay", "", "m:grpdelay:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [gd, W] = grpdelay(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(gd);
    if (nargout > 1) outs[1] = std::move(W);
}

} // namespace detail

} // namespace numkit::m::signal
