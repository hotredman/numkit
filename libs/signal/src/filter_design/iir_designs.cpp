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

} // namespace detail
} // namespace numkit::signal
