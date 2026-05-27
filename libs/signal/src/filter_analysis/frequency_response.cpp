// libs/signal/src/filter_analysis/frequency_response.cpp
//
// Frequency-domain analysis of an existing filter — freqz / phasez /
// grpdelay. Filter design (butter / fir1) lives in
// filter_design/filter_design.cpp.

#include <numkit/signal/filter_analysis/frequency_response.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"   // Complex typedef

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <limits>
#include <tuple>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

std::tuple<Value, Value>
freqz(const Value &b, const Value &a, size_t npts, std::pmr::memory_resource *mr)
{
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const size_t nb = b.numel(), na = a.numel();

    auto W = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
    auto H = Value::complexMatrix(npts, 1, mr);

    // MATLAB freqz(b, a, n): n equispaced frequencies on [0, π) — the
    // upper endpoint π is excluded. Grid is w = (0:n-1) * π / n. The
    // 'whole' option (n on [0, 2π)) is handled by the dispatcher
    // separately.
    for (size_t k = 0; k < npts; ++k) {
        const double w = M_PI * k / npts;
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
// dependencies on filter_analysis/unwrap.cpp's Value-typed unwrap().
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

std::tuple<Value, Value>
phasez(const Value &b, const Value &a, size_t npts, std::pmr::memory_resource *mr)
{
    auto [H, W] = freqz(b, a, npts, mr);
    auto phi = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
    const Complex *hd = H.complexData();
    double *pd = phi.doubleDataMut();
    // MATLAB convention: when |H(w)| is exactly zero, phase is
    // undefined and reported as NaN. Use a tiny epsilon to catch the
    // numerical zeros that come out of the freqz polynomial sum.
    constexpr double kZeroEps = 1e-300;
    for (size_t k = 0; k < npts; ++k) {
        const double mag2 = hd[k].real() * hd[k].real()
                          + hd[k].imag() * hd[k].imag();
        if (mag2 < kZeroEps)
            pd[k] = std::numeric_limits<double>::quiet_NaN();
        else
            pd[k] = std::atan2(hd[k].imag(), hd[k].real());
    }
    unwrapInPlace(pd, npts);
    return std::make_tuple(std::move(phi), std::move(W));
}

std::tuple<Value, Value>
grpdelay(const Value &b, const Value &a, size_t npts, std::pmr::memory_resource *mr)
{
    auto [phi, W] = phasez(b, a, npts, mr);
    auto gd = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
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

void freqz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("freqz: requires at least 2 arguments",
                     0, 0, "freqz", "", "numkit:freqz:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;

    auto [H, W] = freqz(args[0], args[1], npts, ctx.engine->resource());
    outs[0] = std::move(H);
    if (nargout > 1)
        outs[1] = std::move(W);
}

void phasez_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("phasez: requires at least 2 arguments",
                     0, 0, "phasez", "", "numkit:phasez:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [phi, W] = phasez(args[0], args[1], npts, ctx.engine->resource());
    outs[0] = std::move(phi);
    if (nargout > 1) outs[1] = std::move(W);
}

void grpdelay_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("grpdelay: requires at least 2 arguments",
                     0, 0, "grpdelay", "", "numkit:grpdelay:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [gd, W] = grpdelay(args[0], args[1], npts, ctx.engine->resource());
    outs[0] = std::move(gd);
    if (nargout > 1) outs[1] = std::move(W);
}

} // namespace detail

} // namespace numkit::signal
