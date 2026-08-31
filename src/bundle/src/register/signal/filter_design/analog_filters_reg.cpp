// toolboxes/signal/src/filter_design/analog_filters_reg.cpp
//
// CallContext register half of filter_design/analog_filters.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/polyfun.hpp>
#include <numkit/ops/poly_helpers.hpp>
#include <numkit/signal/filter_design/analog_filters.hpp>
#include <numkit/signal/filter_implementation/conversions_extras.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

#define NK_PROTO0_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires N",                                    \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        auto [z, p, k] = name(N, ctx.engine->resource());                       \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

#define NK_PROTO1_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (N, ripple_or_atten_dB)",             \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        const double r = args[1].toScalar();                                    \
        auto [z, p, k] = name(N, r, ctx.engine->resource());                    \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_PROTO0_REG(buttap)
NK_PROTO0_REG(besselap)
NK_PROTO1_REG(cheb1ap)
NK_PROTO1_REG(cheb2ap)

// ellipap takes 3 args (N, Rp, Rs).
void ellipap_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ellipap: requires (N, Rp, Rs)",
                     0, 0, "ellipap", "", "numkit:ellipap:nargin");
    const int N     = static_cast<int>(args[0].toScalar());
    const double Rp = args[1].toScalar();
    const double Rs = args[2].toScalar();
    auto [z, p, k] = ellipap(N, Rp, Rs, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

#undef NK_PROTO0_REG
#undef NK_PROTO1_REG

// MATLAB's lp2* TF form returns the numerator at its true polynomial degree
// (length = #zeros + 1), NOT zero-padded to the denominator length the way
// zp2tf does. e.g. lp2lp of a Butterworth prototype (no zeros) returns
// bt = [Wo^N] (length 1), while zp2tf would give [0 … 0 Wo^N]. Strip the
// leading exact-zeros that zp2tf inserted so the TF form matches MATLAB.
// (No-op for lp2hp/lp2bs, whose numerators are already full length.)
static Value lp2StripLeadingZeros(const Value &b, std::pmr::memory_resource *mr)
{
    const size_t n = b.numel();
    size_t s = 0;
    while (s + 1 < n && b.elemAsDouble(s) == 0.0) ++s;
    if (s == 0) return b;
    auto r = Value::matrix(1, n - s, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    for (size_t i = s; i < n; ++i) d[i - s] = b.elemAsDouble(i);
    return r;
}

// MATLAB lp2lp/lp2hp accept BOTH:
//   [zt, pt, kt] = lp2lp(z, p, k, Wo)   -- ZPK form (4 args)
//   [bt, at]     = lp2lp(b, a, Wo)      -- TF form  (3 args)
// Same for lp2hp. The TF form is identified by the 3-arg call and
// dispatches via tf2zpk -> ZPK lp2lp -> zp2tf.
#define NK_LP2X1_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 3) {                                                  \
            /* TF form: (b, a, Wo) -> (bt, at). */                               \
            const double Wo = args[2].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, mr);                          \
            auto [bt, at] = numkit::builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 4)                                                     \
            throw Error(#name ": requires (z, p, k, Wo) or (b, a, Wo)",         \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, mr);      \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X1_REG(lp2lp, lp2lp)
NK_LP2X1_REG(lp2hp, lp2hp)

#undef NK_LP2X1_REG

// MATLAB lp2bp/lp2bs accept BOTH:
//   [zt, pt, kt] = lp2bp(z, p, k, Wo, Bw)   -- ZPK form (5 args)
//   [bt, at]     = lp2bp(b, a, Wo, Bw)      -- TF form  (4 args)
#define NK_LP2X2_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 4) {                                                  \
            /* TF form: (b, a, Wo, Bw) -> (bt, at). */                           \
            const double Wo = args[2].toScalar();                                \
            const double Bw = args[3].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, Bw, mr);                      \
            auto [bt, at] = numkit::builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 5)                                                     \
            throw Error(#name ": requires (z, p, k, Wo, Bw) or (b, a, Wo, Bw)", \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        const double Bw = args[4].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, Bw, mr);  \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X2_REG(lp2bp, lp2bp)
NK_LP2X2_REG(lp2bs, lp2bs)

#undef NK_LP2X2_REG

void bilinear_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bilinear: requires (b, a, fs[, fp])",
                     0, 0, "bilinear", "", "numkit:bilinear:nargin");
    const double fs = args[2].toScalar();
    const double fp = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 0.0;
    auto [bd, ad] = bilinear(args[0], args[1], fs, fp, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

void impinvar_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("impinvar: requires (b, a, fs[, tol])",
                     0, 0, "impinvar", "", "numkit:impinvar:nargin");
    const double fs  = args[2].toScalar();
    const double tol = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 1e-3;
    auto [bd, ad] = impinvar(args[0], args[1], fs, tol, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

namespace {

// freqs(b, a) auto grid — the classic freqint auto-ranging algorithm
// (Andy Grace 1990, rev 1996; still live inside MATLAB's freqs — every
// endpoint AND interior point verified against R2025b, see
// bugs/closed/signal/freqs-two-arg-auto-w.md):
//
//   ez    = poles (imag >= 0) ++ zeros (|z| < 1e5, imag >= 0)
//           (upper half plane only; no poles -> single pole at -1000,
//           which DERIVES the documented [100, 1e4] default).
//   low   = round(log10(0.1 * min(|Re ez| + 2*Im ez)) - 0.5)   // round half away
//   high  = round(log10(max(3*|Re ez| + 1.5*Im ez)) + 0.5)
//   base  = logspace(low, high, 200 + (P - Z) + [10 if any zero has
//           |Im| < |Re|])                                     // the LONG grid
//   refine: for each oscillatory root (Im > |Re|, descending |Re|):
//           window [max(0.8*Im - 3|Re|, 10^low), 1.2*Im + 4*Re|] —
//           base points inside are REPLACED by a denser logspace
//           (count + npts2 points, npts2 = 2 + 8/ceil(|P-Z+eps|/10)).
//   final = the long grid linearly interpolated in log10 at 200
//           equally spaced INDEX positions (caller-side resample).
//
// Roots at the origin count as "integrators" (|ez| < 1e-10) and are
// shifted by 1 before the log so the extremes stay finite. All ops
// mirror MATLAB doubles exactly (std::round is half-away-from-zero,
// linspace/logspace constructed as 10.^(lo + i*d)).

struct FreqsRoot {
    double re, im;
};

std::vector<double> freqsAutoGridVec(const Value &b, const Value &a)
{
    using numkit::ops::polyRootsDurandKerner;
    constexpr double kEps = 2.220446049250313e-16;
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    ScratchArena scratch(mr);

    auto rootsOf = [&](const Value &p) -> std::vector<FreqsRoot> {
        std::vector<FreqsRoot> out;
        if (!p.isComplex() && p.numel() >= 1) {
            auto r = polyRootsDurandKerner(&scratch, p.doubleData(), p.numel());
            for (const auto &z : r)
                out.push_back({z.real(), z.imag()});
        }
        return out;
    };
    std::vector<FreqsRoot> ep = rootsOf(a); // poles (full set)
    std::vector<FreqsRoot> tz = rootsOf(b); // zeros (full set)
    if (ep.empty())
        ep.push_back({-1000.0, 0.0});

    // ez: upper-half roots; zeros filtered to |z| < 1e5.
    std::vector<FreqsRoot> ez;
    for (auto &r : ep)
        if (r.im >= 0.0)
            ez.push_back(r);
    for (auto &r : tz)
        if (std::abs(std::complex<double>(r.re, r.im)) < 1e5 && r.im >= 0.0)
            ez.push_back(r);

    auto log10v = [](double x) { return std::log10(x); };
    double loNum = 1e300, hiNum = 0.0;
    for (auto &r : ez) {
        const bool integ = std::abs(std::complex<double>(r.re, r.im)) < 1e-10;
        loNum = std::min(loNum, std::abs(r.re + (integ ? 1.0 : 0.0)) + 2.0 * r.im);
        hiNum = std::max(hiNum, 3.0 * std::abs(r.re + (integ ? 1.0 : 0.0)) + 1.5 * r.im);
    }
    const int low = static_cast<int>(std::round(log10v(0.1 * loNum) - 0.5));
    const int high = static_cast<int>(std::round(log10v(hiNum) + 0.5));

    const int diffzp = static_cast<int>(ep.size()) - static_cast<int>(tz.size());
    bool anyRealDomZero = false;
    for (auto &r : tz)
        if (std::abs(r.im) < std::abs(r.re))
            anyRealDomZero = true;
    const int nLong = 200 + diffzp + (anyRealDomZero ? 10 : 0);

    auto logspace = [](double l, double u, int n) {
        std::vector<double> v(n);
        const double d = (u - l) / (n - 1);
        for (int i = 0; i < n; ++i)
            v[i] = std::pow(10.0, l + i * d);
        v[n - 1] = std::pow(10.0, u); // linspace forces the last point exactly
        return v;
    };
    std::vector<double> w = logspace(low, high, nLong); // the base LONG grid

    // Oscillatory refinement (Im > |Re|), descending |Re| (stable).
    std::vector<int> osc;
    for (size_t k = 0; k < ez.size(); ++k)
        if (ez[k].im > std::abs(ez[k].re))
            osc.push_back(static_cast<int>(k));
    std::stable_sort(osc.begin(), osc.end(),
                     [&](int x, int y) { return std::abs(ez[x].re) > std::abs(ez[y].re); });
    const double npts2 = 2.0 + 8.0 / std::ceil(std::abs((diffzp + kEps) / 10.0));

    std::vector<double> f = w, z;
    for (int k : osc) {
        const double r1 = std::max(0.8 * ez[k].im - 3.0 * std::abs(ez[k].re),
                                   std::pow(10.0, static_cast<double>(low)));
        const double r2 = 1.2 * ez[k].im + 4.0 * std::abs(ez[k].re);
        auto outside = [&](double x) { return x > r2 || x < r1; };
        z.erase(std::remove_if(z.begin(), z.end(), [&](double x) { return !outside(x); }), z.end());
        f.erase(std::remove_if(f.begin(), f.end(), [&](double x) { return !outside(x); }), f.end());
        long cnt = 0;
        for (double x : w)
            if (x >= r1 && x <= r2)
                ++cnt;
        auto seg = logspace(std::log10(r1), std::log10(r2), static_cast<int>(cnt + npts2));
        z.insert(z.end(), seg.begin(), seg.end());
    }
    f.insert(f.end(), z.begin(), z.end());
    std::sort(f.begin(), f.end()); // w_long

    // Resample to exactly 200 points at evenly spaced INDEX positions,
    // linear in log10 (the caller-side interp1 of the classic code).
    std::vector<double> lw(f.size());
    for (size_t i = 0; i < f.size(); ++i)
        lw[i] = std::log10(f[i]);
    const double dxi = static_cast<double>(f.size() - 1) / 199.0;
    std::vector<double> out(200);
    for (int i = 0; i < 200; ++i) {
        // 1-based index into lw; the last sample is EXACTLY the last knot
        // (linspace-forced), where interp1 returns log10(w_long(end)) as-is
        // and the pow round-trip gives back the exact decade endpoint.
        const double x = (i == 199) ? static_cast<double>(f.size()) : 1.0 + i * dxi;
        const size_t j = static_cast<size_t>(x) - 1;
        const double t = x - (j + 1);
        const double lv = lw[j] + t * (lw[j + 1] - lw[j]);
        out[i] = std::pow(10.0, lv);
    }
    return out;
}

Value freqsAutoGrid(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    auto v = freqsAutoGridVec(b, a);
    Value w = Value::matrix(1, 200, ValueType::DOUBLE, mr);
    double *wd = w.doubleDataMut();
    for (int i = 0; i < 200; ++i)
        wd[i] = v[i];
    return w;
}

} // namespace

void freqs_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() == 2) {
        // freqs(b, a): auto grid; the no-output PLOT form is a separate
        // piece of the same filed gap.
        auto w = freqsAutoGrid(args[0], args[1], ctx.engine->resource());
        outs[0] = freqs(args[0], args[1], w, ctx.engine->resource());
        if (nargout > 1)
            outs[1] = std::move(w);
        return;
    }
    if (args.size() < 3)
        throw Error("freqs: requires (b, a, w)",
                     0, 0, "freqs", "", "numkit:freqs:nargin");
    outs[0] = freqs(args[0], args[1], args[2], ctx.engine->resource());
    if (nargout > 1)
        outs[1] = args[2];
}

} // namespace detail

} // namespace numkit::signal
