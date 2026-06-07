// libs/signal/src/spectral_analysis/signal_modeling_reg.cpp
//
// CallContext register half of spectral_analysis/signal_modeling.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/signal/spectral_analysis/signal_modeling.hpp>
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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void levinson_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("levinson: requires at least 1 argument",
                     0, 0, "levinson", "", "numkit:levinson:nargin");
    int n = -1;
    if (args.size() >= 2 && !args[1].isEmpty()) n = static_cast<int>(args[1].toScalar());
    auto [a, e, k] = levinson(args[0], n, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
    if (nargout > 2) outs[2] = std::move(k);
}

void rlevinson_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rlevinson: requires (a, e)",
                     0, 0, "rlevinson", "", "numkit:rlevinson:nargin");
    auto [R, k] = rlevinson(args[0], args[1].toScalar(), ctx.engine->resource());
    outs[0] = std::move(R);
    if (nargout > 1) outs[1] = std::move(k);
}

#define NK_AR_REG(name, fn)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (x, p)",                              \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int p = static_cast<int>(args[1].toScalar());                     \
        auto [a, e, k] = fn(args[0], p, ctx.engine->resource());                \
        outs[0] = std::move(a);                                                  \
        if (nargout > 1) outs[1] = std::move(e);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_AR_REG(aryule, aryule)
NK_AR_REG(arburg, arburg)

#undef NK_AR_REG

void lpc_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lpc: requires (x, p)",
                     0, 0, "lpc", "", "numkit:lpc:nargin");
    const int p = static_cast<int>(args[1].toScalar());
    auto [a, g] = lpc(args[0], p, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(g);
}

void ac2poly_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2poly: requires 1 argument", 0, 0, "ac2poly", "", "numkit:ac2poly:nargin");
    auto [a, e] = ac2poly(args[0], ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
}

void poly2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("poly2ac: requires 1 argument", 0, 0, "poly2ac", "", "numkit:poly2ac:nargin");
    const double e = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 1.0;
    outs[0] = poly2ac(args[0], e, ctx.engine->resource());
}

void ac2rc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2rc: requires 1 argument", 0, 0, "ac2rc", "", "numkit:ac2rc:nargin");
    auto [k, r0] = ac2rc(args[0], ctx.engine->resource());
    outs[0] = std::move(k);
    if (nargout > 1) outs[1] = std::move(r0);
}

void schurrc_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("schurrc: requires 1 argument (R)", 0, 0, "schurrc", "", "numkit:schurrc:nargin");
    outs[0] = schurrc(args[0], ctx.engine->resource());
}

void rc2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2) throw Error("rc2ac: requires (k, r0)", 0, 0, "rc2ac", "", "numkit:rc2ac:nargin");
    outs[0] = rc2ac(args[0], args[1].toScalar(), ctx.engine->resource());
}

void poly2rc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poly2rc: requires at least 1 argument (a)",
                     0, 0, "poly2rc", "", "numkit:poly2rc:nargin");
    // [k, r0] = poly2rc(a, efinal). efinal (final prediction error) is
    // only needed for the second output; defaults to 0.
    const double efinal = (args.size() >= 2 && !args[1].isEmpty())
                              ? args[1].toScalar() : 0.0;
    auto [k, r0] = poly2rc(args[0], efinal, ctx.engine->resource());
    outs[0] = std::move(k);
    if (nargout > 1) outs[1] = std::move(r0);
}

void rc2poly_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rc2poly: requires at least 1 argument (k)",
                     0, 0, "rc2poly", "", "numkit:rc2poly:nargin");
    // [a, efinal] = rc2poly(k, r0). r0 (zero-lag autocorrelation) is only
    // needed for the second output efinal = r0*prod(1-k.^2); defaults to 1.
    const double r0 = (args.size() >= 2 && !args[1].isEmpty())
                          ? args[1].toScalar() : 1.0;
    auto [a, efinal] = rc2poly(args[0], r0, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(efinal);
}

#define NK_UNARY_CONV_REG(name)                                                 \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        outs[0] = name(args[0], ctx.engine->resource());                         \
    }

NK_UNARY_CONV_REG(is2rc)
NK_UNARY_CONV_REG(rc2is)
NK_UNARY_CONV_REG(lar2rc)
NK_UNARY_CONV_REG(rc2lar)
NK_UNARY_CONV_REG(poly2lsf)
NK_UNARY_CONV_REG(lsf2poly)

#undef NK_UNARY_CONV_REG

#define NK_AR2_REG(name, fn)                                                    \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (x, p)",                              \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int p = static_cast<int>(args[1].toScalar());                     \
        auto [a, e] = fn(args[0], p, ctx.engine->resource());                   \
        outs[0] = std::move(a);                                                  \
        if (nargout > 1) outs[1] = std::move(e);                                 \
    }

NK_AR2_REG(arcov,  arcov)
NK_AR2_REG(armcov, armcov)

#undef NK_AR2_REG

void prony_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prony: requires (h, nb, na)",
                     0, 0, "prony", "", "numkit:prony:nargin");
    const int nb = static_cast<int>(args[1].toScalar());
    const int na = static_cast<int>(args[2].toScalar());
    auto [b, a] = prony(args[0], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void corrmtx_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("corrmtx: requires (x, m)",
                     0, 0, "corrmtx", "", "numkit:corrmtx:nargin");
    const int m = static_cast<int>(args[1].toScalar());
    outs[0] = corrmtx(args[0], m, ctx.engine->resource());
}

void invfreqs_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("invfreqs: requires (H, w, nb, na)",
                     0, 0, "invfreqs", "", "numkit:invfreqs:nargin");
    const int nb = static_cast<int>(args[2].toScalar());
    const int na = static_cast<int>(args[3].toScalar());
    auto [b, a] = invfreqs(args[0], args[1], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void invfreqz_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("invfreqz: requires (H, w, nb, na)",
                     0, 0, "invfreqz", "", "numkit:invfreqz:nargin");
    const int nb = static_cast<int>(args[2].toScalar());
    const int na = static_cast<int>(args[3].toScalar());
    auto [b, a] = invfreqz(args[0], args[1], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

} // namespace detail

} // namespace numkit::signal
