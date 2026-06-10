// toolboxes/signal/src/filter_design/iir_designs_reg.cpp
//
// CallContext register half of filter_design/iir_designs.cpp (Phase 2b compute/register split).
// The detail{{}} *_reg fns were interleaved with compute in the original
// TU; collected here verbatim (reg-side arg parsers nested in their anon
// namespaces ride along). See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/signal/filter_design/analog_filters.hpp>
#include <numkit/signal/filter_design/iir_designs.hpp>
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
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

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
                     0, 0, "cheby1", "", "numkit:cheby1:nargin");
    const int N    = static_cast<int>(args[0].toScalar());
    const double R = args[1].toScalar();
    const Value &Wn = args[2];
    auto t = parseTrailing(args, 3);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = cheby1(N, R, Wn, ftype, t.analog, ctx.engine->resource());
    if (outs.size() >= 3) {   // [z, p, k] = cheby1(...): digital ZPK via tf2zp
        auto [z, p, k] = ::numkit::math::tf2zp(b, a, ctx.engine->resource());
        outs[0] = std::move(z); outs[1] = std::move(p); outs[2] = std::move(k);
        return;
    }
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

void cheby2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cheby2: requires (N, Rs, Wn[, ftype][, 's'])",
                     0, 0, "cheby2", "", "numkit:cheby2:nargin");
    const int N    = static_cast<int>(args[0].toScalar());
    const double R = args[1].toScalar();
    const Value &Wn = args[2];
    auto t = parseTrailing(args, 3);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = cheby2(N, R, Wn, ftype, t.analog, ctx.engine->resource());
    if (outs.size() >= 3) {   // [z, p, k] = cheby2(...): digital ZPK via tf2zp
        auto [z, p, k] = ::numkit::math::tf2zp(b, a, ctx.engine->resource());
        outs[0] = std::move(z); outs[1] = std::move(p); outs[2] = std::move(k);
        return;
    }
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

void besself_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("besself: requires (N, Wn[, ftype][, 's'])",
                     0, 0, "besself", "", "numkit:besself:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const Value &Wn = args[1];
    auto t = parseTrailing(args, 2);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    // besself is ALWAYS analog — MATLAB has no digital Bessel filter (the
    // bilinear transform destroys the maximally-flat group delay). The 's'
    // flag is therefore redundant; ignore the digital path entirely.
    (void)t.analog;
    auto [b, a] = besself(N, Wn, ftype, /*analog=*/true, ctx.engine->resource());
    outs[0] = std::move(b);
    if (outs.size() > 1) outs[1] = std::move(a);
}

void ellip_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ellip: requires (N, Rp, Rs, Wn[, ftype][, 's'])",
                     0, 0, "ellip", "", "numkit:ellip:nargin");
    const int N      = static_cast<int>(args[0].toScalar());
    const double Rp  = args[1].toScalar();
    const double Rs  = args[2].toScalar();
    const Value &Wn  = args[3];
    auto t = parseTrailing(args, 4);
    auto ftype = defaultFtype(Wn, t.ftype, t.ftype_set);
    auto [b, a] = ellip(N, Rp, Rs, Wn, ftype, t.analog, ctx.engine->resource());
    if (outs.size() >= 3) {   // [z, p, k] = ellip(...): digital ZPK via tf2zp
        auto [z, p, k] = ::numkit::math::tf2zp(b, a, ctx.engine->resource());
        outs[0] = std::move(z); outs[1] = std::move(p); outs[2] = std::move(k);
        return;
    }
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
                         0, 0, #name, "", "numkit:" #name ":nargin");                   \
        const Value &Wp = args[0];                                                \
        const Value &Ws = args[1];                                                \
        const double Rp = args[2].toScalar();                                     \
        const double Rs = args[3].toScalar();                                     \
        const bool analog = parseAnalogFromOrdArgs(args, 4);                      \
        auto [N, Wn] = name(Wp, Ws, Rp, Rs, analog, ctx.engine->resource());     \
        outs[0] = Value::scalar(static_cast<double>(N), ctx.engine->resource()); \
        if (nargout > 1) outs[1] = std::move(Wn);                                 \
    }

NK_ORD_REG(buttord)
NK_ORD_REG(cheb1ord)
NK_ORD_REG(cheb2ord)

#undef NK_ORD_REG

} // namespace detail

namespace detail {

void ellipord_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ellipord: requires (Wp, Ws, Rp, Rs[, 's'])",
                    0, 0, "ellipord", "", "numkit:ellipord:nargin");
    bool analog = false;
    if (args.size() >= 5) {
        std::string s = args[4].toString();
        analog = (s == "s" || s == "S");
    }
    auto [N, Wn] = ellipord(args[0], args[1], args[2].toScalar(), args[3].toScalar(), analog, ctx.engine->resource());
    outs[0] = Value::scalar(static_cast<double>(N), ctx.engine->resource());
    if (nargout > 1) outs[1] = std::move(Wn);
}

} // namespace detail

namespace detail {

void firpmord_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("firpmord: requires (F, A, dev [, fs])",
                    0, 0, "firpmord", "", "numkit:firpmord:nargin");
    double fs = 2.0;
    if (args.size() >= 4 && !args[3].isEmpty()) fs = args[3].toScalar();
    auto [N, ff, aa, wts] = firpmord(args[0], args[1], args[2], fs, ctx.engine->resource());
    outs[0] = Value::scalar(static_cast<double>(N), ctx.engine->resource());
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(ff);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(aa);
    if (nargout >= 4 && outs.size() >= 4) outs[3] = std::move(wts);
}

} // namespace detail

namespace detail {

void kaiserord_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("kaiserord: requires (F, A, dev [, fs])",
                    0, 0, "kaiserord", "", "numkit:kaiserord:nargin");
    double fs = 2.0;
    if (args.size() >= 4 && !args[3].isEmpty()) fs = args[3].toScalar();
    auto [N, Wn, beta, ftype] = kaiserord(args[0], args[1], args[2], fs, ctx.engine->resource());
    auto *mr = ctx.engine->resource();
    outs[0] = Value::scalar(static_cast<double>(N), mr);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(Wn);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = Value::scalar(beta, mr);
    if (nargout >= 4 && outs.size() >= 4) {
        Value f = Value::matrix(1, ftype.size(), ValueType::CHAR, mr);
        char *cd = f.charDataMut();
        for (size_t i = 0; i < ftype.size(); ++i) cd[i] = ftype[i];
        outs[3] = std::move(f);
    }
}

} // namespace detail

} // namespace numkit::signal
