// libs/signal/src/filter_design/iir_designs.cpp
//
// Top-level cheby1 / cheby2 / besself: compose analog prototype +
// lp2X + zp2tf + bilinear.
//
// Wn convention (matches MATLAB):
//   • Digital (analog == false):  Wn ∈ (0, 1) normalised to Nyquist;
//     pre-warp via Ωₐ = 2·fs·tan(π·Wn / 2) with fs = 1.
//   • Analog  (analog == true ): Wn already in rad/s.
//
// For bandpass/bandstop, Wn is a 2-vector [Wlo, Whi]; the prototype is
// scaled to centre Ω₀ = √(Wlo·Whi), bandwidth Bw = Whi - Wlo.

#include <numkit/signal/filter_design/iir_designs.hpp>

#include <numkit/signal/filter_design/analog_filters.hpp>
#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

// Convert Wn (Value) → 1 or 2 doubles.
std::vector<double> readWn(const Value &Wn) {
    const size_t n = Wn.numel();
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = Wn.elemAsDouble(i);
    return out;
}

// Pre-warp digital Wn ∈ (0, 1) → analog Ω = 2·tan(π·Wn / 2). With fs = 1
// (we'll undo via bilinear with fs = 1), this is the standard prewarp.
inline double prewarp(double Wn_norm) {
    return 2.0 * std::tan(M_PI * Wn_norm / 2.0);
}

// Apply lp2X transform per ftype to (z, p, k).
std::tuple<Value, Value, Value>
applyLp2X(std::pmr::memory_resource *mr,
          Value z, Value p, double k,
          FilterType ftype,
          const std::vector<double> &Wo_analog)
{
    switch (ftype) {
        case FilterType::Lowpass:
            return lp2lp(mr, z, p, k, Wo_analog[0]);
        case FilterType::Highpass:
            return lp2hp(mr, z, p, k, Wo_analog[0]);
        case FilterType::Bandpass: {
            const double Wo = std::sqrt(Wo_analog[0] * Wo_analog[1]);
            const double Bw = Wo_analog[1] - Wo_analog[0];
            return lp2bp(mr, z, p, k, Wo, Bw);
        }
        case FilterType::Bandstop: {
            const double Wo = std::sqrt(Wo_analog[0] * Wo_analog[1]);
            const double Bw = Wo_analog[1] - Wo_analog[0];
            return lp2bs(mr, z, p, k, Wo, Bw);
        }
    }
    throw std::runtime_error("iir_designs: bad ftype");
}

// Validate Wn arity vs ftype.
void validateWn(FilterType ftype, const std::vector<double> &Wn) {
    const bool isBand = (ftype == FilterType::Bandpass || ftype == FilterType::Bandstop);
    if (isBand && Wn.size() != 2)
        throw std::runtime_error("bandpass/bandstop requires 2-element Wn");
    if (!isBand && Wn.size() != 1)
        throw std::runtime_error("lowpass/highpass requires scalar Wn");
}

// Common end-of-pipe: zpk → tf → bilinear (or stay analog).
std::tuple<Value, Value>
finishDesign(std::pmr::memory_resource *mr,
             Value z, Value p, Value k_v, bool analog)
{
    const double k = k_v.toScalar();
    auto [b, a] = ::numkit::builtin::zp2tf(mr, z, p, k);
    if (analog) return std::make_tuple(std::move(b), std::move(a));
    // bilinear with fs = 1 reverses the pre-warp we applied earlier
    // (Ω = 2·tan(π·Wn/2) with fs = 1).
    return bilinear(mr, b, a, /*fs=*/1.0, /*fp=*/0.0);
}

// Translate raw Wn (digital normalised or analog rad/s) into the
// analog-domain Wo's used by the lp2X step.
std::vector<double> wnToAnalog(const std::vector<double> &Wn, bool analog) {
    if (analog) return Wn;
    std::vector<double> out(Wn.size());
    for (size_t i = 0; i < Wn.size(); ++i) out[i] = prewarp(Wn[i]);
    return out;
}

} // anonymous

std::tuple<Value, Value>
cheby1(std::pmr::memory_resource *mr, int N, double Rp,
       const Value &Wn, FilterType ftype, bool analog)
{
    auto wn = readWn(Wn);
    validateWn(ftype, wn);
    auto [z, p, k_v] = cheb1ap(mr, N, Rp);
    auto Wo = wnToAnalog(wn, analog);
    auto [z2, p2, k2] = applyLp2X(mr, std::move(z), std::move(p), k_v.toScalar(),
                                   ftype, Wo);
    return finishDesign(mr, std::move(z2), std::move(p2), std::move(k2), analog);
}

std::tuple<Value, Value>
cheby2(std::pmr::memory_resource *mr, int N, double Rs,
       const Value &Wn, FilterType ftype, bool analog)
{
    auto wn = readWn(Wn);
    validateWn(ftype, wn);
    auto [z, p, k_v] = cheb2ap(mr, N, Rs);
    auto Wo = wnToAnalog(wn, analog);
    auto [z2, p2, k2] = applyLp2X(mr, std::move(z), std::move(p), k_v.toScalar(),
                                   ftype, Wo);
    return finishDesign(mr, std::move(z2), std::move(p2), std::move(k2), analog);
}

std::tuple<Value, Value>
besself(std::pmr::memory_resource *mr, int N,
        const Value &Wn, FilterType ftype, bool analog)
{
    auto wn = readWn(Wn);
    validateWn(ftype, wn);
    auto [z, p, k_v] = besselap(mr, N);
    auto Wo = wnToAnalog(wn, analog);
    auto [z2, p2, k2] = applyLp2X(mr, std::move(z), std::move(p), k_v.toScalar(),
                                   ftype, Wo);
    return finishDesign(mr, std::move(z2), std::move(p2), std::move(k2), analog);
}

// ════════════════════════════════════════════════════════════════════
// Order estimators
// ════════════════════════════════════════════════════════════════════
//
// All four take (Wp, Ws, Rp, Rs) and return the minimum order N plus a
// natural-frequency vector Wn for the chosen design. For digital
// inputs Wp / Ws are normalised to Nyquist (∈ (0, 1)); we pre-warp via
// Ω = tan(π·W/2) before working in the analog domain. The final Wn is
// post-warped back via W = (2/π)·atan(Ω). For analog (analog == true)
// the inputs are already in rad/s and no warping is done.

namespace {

inline double prewarpWp(double w, bool analog) {
    return analog ? w : std::tan(M_PI * w / 2.0);
}
inline double dewarpWp(double Omega, bool analog) {
    return analog ? Omega : (2.0 / M_PI) * std::atan(Omega);
}

// Normalised analog stopband / passband ratio for a generic spec.
// For lowpass : Ωs / Ωp.   For highpass : Ωp / Ωs.
// For bandpass / bandstop : choose the worst-case ratio over the two
// stopband edges given the geometric centre Ω0 = √(Ωp1·Ωp2). MATLAB
// uses the standard prewarp + frequency-transform identities.
struct OrdNormalised {
    double ratio;       // Ωs_norm / Ωp_norm; always > 1 for valid specs
    double Wo_passband; // analog-domain passband for the prototype
    double Wo_stopband; // analog-domain stopband edge (for buttord cutoff)
    FilterType ftype;
    double Wo_low{0.0}; // for band designs: lower & upper analog edges
    double Wo_high{0.0};
    double Bw{0.0};
    double Wo_centre{0.0};
};

OrdNormalised normaliseOrd(const std::vector<double> &Wp,
                           const std::vector<double> &Ws,
                           bool analog)
{
    OrdNormalised n{};
    if (Wp.size() == 1 && Ws.size() == 1) {
        const double Op = prewarpWp(Wp[0], analog);
        const double Os = prewarpWp(Ws[0], analog);
        if (Op == Os) throw std::runtime_error("ord: Wp and Ws must differ");
        if (Op < Os) {
            n.ftype = FilterType::Lowpass;
            n.ratio = Os / Op;
            n.Wo_passband = Op;
            n.Wo_stopband = Os;
        } else {
            n.ftype = FilterType::Highpass;
            n.ratio = Op / Os;
            n.Wo_passband = Op;
            n.Wo_stopband = Os;
        }
    } else if (Wp.size() == 2 && Ws.size() == 2) {
        const double Op1 = prewarpWp(Wp[0], analog), Op2 = prewarpWp(Wp[1], analog);
        const double Os1 = prewarpWp(Ws[0], analog), Os2 = prewarpWp(Ws[1], analog);
        // bandpass:  Ws1 < Wp1 < Wp2 < Ws2
        // bandstop:  Wp1 < Ws1 < Ws2 < Wp2
        const bool bp = (Os1 < Op1 && Op2 < Os2);
        n.ftype = bp ? FilterType::Bandpass : FilterType::Bandstop;
        const double Wo = std::sqrt(Op1 * Op2);
        const double Bw = Op2 - Op1;
        n.Wo_centre = Wo;
        n.Bw = Bw;
        n.Wo_low = Op1; n.Wo_high = Op2;
        if (bp) {
            // Bandpass→lowpass map: Ω_LP = (Ω² - Ω0²) / (Bw·Ω).
            // Take absolute value; ratio = |Ω_LP_stopband| (≥ 1 for valid spec).
            auto mp = [&](double O){ return std::abs((O * O - Wo * Wo) / (Bw * O)); };
            n.ratio = std::min(mp(Os1), mp(Os2));
            n.Wo_passband = 1.0;  // unit prototype
        } else {
            // Bandstop→lowpass: Ω_LP = (Bw·Ω) / (Ω² - Ω0²)  (reciprocal form).
            auto mp = [&](double O){ return std::abs((Bw * O) / (O * O - Wo * Wo)); };
            n.ratio = std::min(mp(Os1), mp(Os2));
            n.Wo_passband = 1.0;
        }
    } else {
        throw std::runtime_error("ord: Wp and Ws must both be scalar or both 2-vectors");
    }
    return n;
}

// Convert the prototype Ω back into the requested Wn vector.
Value wnFromPrototype(std::pmr::memory_resource *mr,
                      const OrdNormalised &n, double Wn_analog,
                      bool analog)
{
    switch (n.ftype) {
        case FilterType::Lowpass:
        case FilterType::Highpass: {
            const double w = dewarpWp(Wn_analog, analog);
            return Value::scalar(w, mr);
        }
        case FilterType::Bandpass:
        case FilterType::Bandstop: {
            // For band designs, MATLAB's *ord typically returns the
            // original passband edges (no recomputation needed).
            auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            double *od = out.doubleDataMut();
            od[0] = dewarpWp(n.Wo_low,  analog);
            od[1] = dewarpWp(n.Wo_high, analog);
            return out;
        }
    }
    throw std::runtime_error("wnFromPrototype: bad ftype");
}

} // anonymous

std::tuple<int, Value>
buttord(std::pmr::memory_resource *mr, const Value &Wp_v, const Value &Ws_v,
        double Rp, double Rs, bool analog)
{
    auto Wp = readWn(Wp_v);
    auto Ws = readWn(Ws_v);
    auto n = normaliseOrd(Wp, Ws, analog);
    const double GsLin = std::pow(10.0, Rs / 10.0) - 1.0;
    const double GpLin = std::pow(10.0, Rp / 10.0) - 1.0;
    if (GpLin <= 0.0 || GsLin <= 0.0)
        throw std::runtime_error("buttord: Rp, Rs must be positive");
    const int N = static_cast<int>(std::ceil(
        0.5 * std::log10(GsLin / GpLin) / std::log10(n.ratio)));
    // MATLAB convention: Wn computed from STOPBAND spec, so the resulting
    // filter meets stopband attenuation exactly. For Butterworth:
    //   lowpass:  |H|² = 1/(1+(Ω/Ωc)^{2N})  ⇒ Ωc = Ωs / GsLin^{1/(2N)}
    //   highpass: |H|² = 1/(1+(Ωc/Ω)^{2N})  ⇒ Ωc = Ωs · GsLin^{1/(2N)}
    const double r = std::pow(GsLin, 1.0 / (2.0 * N));
    double Wn_analog = 0.0;
    switch (n.ftype) {
        case FilterType::Lowpass:  Wn_analog = n.Wo_stopband / r; break;
        case FilterType::Highpass: Wn_analog = n.Wo_stopband * r; break;
        case FilterType::Bandpass:
        case FilterType::Bandstop: Wn_analog = 1.0; break;
    }
    Value Wn = wnFromPrototype(mr, n, Wn_analog, analog);
    return std::make_tuple(N, std::move(Wn));
}

std::tuple<int, Value>
cheb1ord(std::pmr::memory_resource *mr, const Value &Wp_v, const Value &Ws_v,
         double Rp, double Rs, bool analog)
{
    auto Wp = readWn(Wp_v);
    auto Ws = readWn(Ws_v);
    auto n = normaliseOrd(Wp, Ws, analog);
    const double GsLin = std::pow(10.0, Rs / 10.0) - 1.0;
    const double GpLin = std::pow(10.0, Rp / 10.0) - 1.0;
    if (GpLin <= 0.0 || GsLin <= 0.0)
        throw std::runtime_error("cheb1ord: Rp, Rs must be positive");
    const int N = static_cast<int>(std::ceil(
        std::acosh(std::sqrt(GsLin / GpLin)) / std::acosh(n.ratio)));
    // Natural cutoff for cheby1 = passband edge — return Wp directly.
    if (n.ftype == FilterType::Lowpass || n.ftype == FilterType::Highpass)
        return std::make_tuple(N, Value::scalar(Wp[0], mr));
    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    od[0] = Wp[0]; od[1] = Wp[1];
    return std::make_tuple(N, std::move(out));
}

std::tuple<int, Value>
cheb2ord(std::pmr::memory_resource *mr, const Value &Wp_v, const Value &Ws_v,
         double Rp, double Rs, bool analog)
{
    auto Wp = readWn(Wp_v);
    auto Ws = readWn(Ws_v);
    auto n = normaliseOrd(Wp, Ws, analog);
    const double GsLin = std::pow(10.0, Rs / 10.0) - 1.0;
    const double GpLin = std::pow(10.0, Rp / 10.0) - 1.0;
    if (GpLin <= 0.0 || GsLin <= 0.0)
        throw std::runtime_error("cheb2ord: Rp, Rs must be positive");
    const int N = static_cast<int>(std::ceil(
        std::acosh(std::sqrt(GsLin / GpLin)) / std::acosh(n.ratio)));
    // Natural cutoff for cheby2 = stopband edge — return Ws directly.
    if (n.ftype == FilterType::Lowpass || n.ftype == FilterType::Highpass)
        return std::make_tuple(N, Value::scalar(Ws[0], mr));
    auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    od[0] = Ws[0]; od[1] = Ws[1];
    return std::make_tuple(N, std::move(out));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
inline FilterType parseFtype(const Value &v) {
    if (!v.isChar() && !v.isString()) return FilterType::Lowpass;
    auto s = v.toString();
    // Case-insensitive compare
    auto eq = [&](const char *t) {
        if (s.size() != std::strlen(t)) return false;
        for (size_t i = 0; i < s.size(); ++i)
            if (std::tolower((unsigned char)s[i]) != (unsigned char)t[i]) return false;
        return true;
    };
    if (eq("low"))      return FilterType::Lowpass;
    if (eq("high"))     return FilterType::Highpass;
    if (eq("bandpass")) return FilterType::Bandpass;
    if (eq("stop"))     return FilterType::Bandstop;
    return FilterType::Lowpass;
}

inline bool isAnalogFlag(const Value &v) {
    if (!v.isChar() && !v.isString()) return false;
    auto s = v.toString();
    return s.size() == 1 && (s[0] == 's' || s[0] == 'S');
}

// Parse trailing string args: any combination of ftype string + 's' analog
// flag, in any order.
struct Trailing {
    FilterType ftype{FilterType::Lowpass};
    bool ftype_set{false};
    bool analog{false};
};

inline Trailing parseTrailing(Span<const Value> args, size_t start) {
    Trailing t;
    for (size_t i = start; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString()) continue;
        if (isAnalogFlag(args[i])) t.analog = true;
        else                       { t.ftype = parseFtype(args[i]); t.ftype_set = true; }
    }
    return t;
}

inline FilterType defaultFtype(const Value &Wn, FilterType picked, bool was_set) {
    if (was_set) return picked;
    return Wn.numel() == 2 ? FilterType::Bandpass : FilterType::Lowpass;
}
} // anonymous

void cheby1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cheby1: requires (N, Rp, Wn[, ftype][, 's'])",
                     0, 0, "cheby1", "", "m:cheby1:nargin");
    const int N    = static_cast<int>(args[0].toScalar());
    const double R = args[1].toScalar();
    const Value &Wn = args[2];
    auto t = parseTrailing(args, 3);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = cheby1(ctx.engine->resource(), N, R, Wn, ftype, t.analog);
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

void cheby2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cheby2: requires (N, Rs, Wn[, ftype][, 's'])",
                     0, 0, "cheby2", "", "m:cheby2:nargin");
    const int N    = static_cast<int>(args[0].toScalar());
    const double R = args[1].toScalar();
    const Value &Wn = args[2];
    auto t = parseTrailing(args, 3);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = cheby2(ctx.engine->resource(), N, R, Wn, ftype, t.analog);
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

void besself_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("besself: requires (N, Wn[, ftype][, 's'])",
                     0, 0, "besself", "", "m:besself:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const Value &Wn = args[1];
    auto t = parseTrailing(args, 2);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = besself(ctx.engine->resource(), N, Wn, ftype, t.analog);
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

namespace {
inline bool parseAnalogFromOrdArgs(Span<const Value> args, size_t start) {
    for (size_t i = start; i < args.size(); ++i)
        if ((args[i].isChar() || args[i].isString()) && isAnalogFlag(args[i]))
            return true;
    return false;
}
}

#define NK_ORD_REG(name)                                                          \
    void name##_reg(Span<const Value> args, size_t nargout,                       \
                    Span<Value> outs, CallContext &ctx)                           \
    {                                                                              \
        if (args.size() < 4)                                                       \
            throw Error(#name ": requires (Wp, Ws, Rp, Rs[, 's'])",               \
                         0, 0, #name, "", "m:" #name ":nargin");                   \
        const Value &Wp = args[0];                                                \
        const Value &Ws = args[1];                                                \
        const double Rp = args[2].toScalar();                                     \
        const double Rs = args[3].toScalar();                                     \
        const bool analog = parseAnalogFromOrdArgs(args, 4);                      \
        auto [N, Wn] = name(ctx.engine->resource(), Wp, Ws, Rp, Rs, analog);     \
        outs[0] = Value::scalar(static_cast<double>(N), ctx.engine->resource()); \
        if (nargout > 1) outs[1] = std::move(Wn);                                 \
    }

NK_ORD_REG(buttord)
NK_ORD_REG(cheb1ord)
NK_ORD_REG(cheb2ord)

#undef NK_ORD_REG

} // namespace detail
} // namespace numkit::signal
