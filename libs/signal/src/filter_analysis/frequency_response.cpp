// libs/signal/src/filter_analysis/frequency_response.cpp
//
// Frequency-domain analysis of an existing filter — freqz / phasez /
// grpdelay. Filter design (butter / fir1) lives in
// filter_design/filter_design.cpp.

#include <numkit/signal/filter_analysis/frequency_response.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
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
freqz(const Value &b, const Value &a, size_t npts, std::pmr::memory_resource *mr, bool whole, double fs)
{
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const size_t nb = b.numel(), na = a.numel();

    auto W = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
    auto H = Value::complexMatrix(npts, 1, mr);

    // MATLAB freqz(b, a, n): n equispaced frequencies on [0, π) — the
    // upper endpoint π is excluded. Grid is w = (0:n-1) * π / n. With
    // 'whole', the grid spans the full unit circle [0, 2π):
    // w = (0:n-1) * 2π / n. The response H is always evaluated at these
    // normalised radian frequencies; when fs > 0 (the freqz(b,a,n,fs)
    // form) the RETURNED frequency vector is rescaled to Hz over [0, fs/2)
    // (or [0, fs) with 'whole') — i.e. f = w * fs / (2π).
    const double span   = whole ? (2.0 * M_PI) : M_PI;
    const double hzSpan = whole ? fs : (0.5 * fs);
    for (size_t k = 0; k < npts; ++k) {
        const double w = span * k / npts;
        W.doubleDataMut()[k] = (fs > 0.0) ? (hzSpan * k / npts) : w;

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
    // EXACT group delay (MATLAB's method), NOT a finite-difference of the
    // phase. For H(z) = B(z)/A(z), form the combined polynomial
    //   c = conv(b, reverse(a)),
    // whose argument is arg(B) - w*(na-1) - arg(A). Hence
    //   gd(w) = -d/dw arg(H) = Re{ CR(e^jw) / C(e^jw) } - (na - 1),
    // where CR has coefficients n*c[n]. This is exact at every frequency
    // (the old phase finite-difference was wildly off at small npts).
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const size_t nb = b.numel(), na = a.numel();

    auto W  = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
    auto gd = Value::matrix(npts, 1, ValueType::DOUBLE, mr);
    if (npts == 0 || nb == 0 || na == 0)
        return std::make_tuple(std::move(gd), std::move(W));

    ScratchArena scratch(mr);
    const size_t nc = nb + na - 1;
    auto c = ScratchVec<double>(nc, &scratch);
    for (size_t i = 0; i < nc; ++i) c[i] = 0.0;
    for (size_t i = 0; i < nb; ++i)
        for (size_t j = 0; j < na; ++j)
            c[i + j] += bd[i] * ad[na - 1 - j];   // conv(b, reverse(a))

    double *wp = W.doubleDataMut();
    double *g  = gd.doubleDataMut();
    const double offset = static_cast<double>(na) - 1.0;
    for (size_t k = 0; k < npts; ++k) {
        const double w = M_PI * static_cast<double>(k) / static_cast<double>(npts);
        wp[k] = w;
        const Complex ejw(std::cos(w), -std::sin(w));
        Complex C(0, 0), CR(0, 0), ejwk(1, 0);
        for (size_t n = 0; n < nc; ++n) {
            C  += c[n] * ejwk;
            CR += (static_cast<double>(n) * c[n]) * ejwk;
            ejwk *= ejw;
        }
        const double cmag2 = C.real() * C.real() + C.imag() * C.imag();
        g[k] = (cmag2 < 1e-300) ? 0.0 : ((CR / C).real() - offset);
    }
    return std::make_tuple(std::move(gd), std::move(W));
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void freqz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("freqz: requires at least 2 arguments",
                     0, 0, "freqz", "", "numkit:freqz:nargin");
    // Parse the trailing args: 'whole' (string, any position) and up to two
    // numerics after b,a — the first is n (npts), the second is fs (the
    // freqz(b,a,n,fs) / freqz(b,a,n,'whole',fs) sample-rate form).
    size_t npts = 512;
    double fs   = 0.0;
    bool   whole = false;
    int    numericSeen = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "whole") whole = true;
        } else if (!args[i].isEmpty()) {
            if (numericSeen == 0)      npts = static_cast<size_t>(args[i].toScalar());
            else if (numericSeen == 1) fs   = args[i].toScalar();
            ++numericSeen;
        }
    }

    auto [H, W] = freqz(args[0], args[1], npts, ctx.engine->resource(), whole, fs);
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
