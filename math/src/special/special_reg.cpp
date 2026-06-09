// toolboxes/signal/src/math/special/special_reg.cpp
//
// CallContext register half of math/special/special.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "helpers.hpp"
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

namespace numkit::builtin {

namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        outs[0] = fn(args[0], ctx.engine->resource());                          \
    }

NK_UNARY_ADAPTER(gamma,   gammaFn)
NK_UNARY_ADAPTER(gammaln, gammaln)
NK_UNARY_ADAPTER(erf,     erf)
NK_UNARY_ADAPTER(erfc,    erfc)
NK_UNARY_ADAPTER(erfinv,  erfinv)
NK_UNARY_ADAPTER(erfcinv, erfcinv)
NK_UNARY_ADAPTER(erfcx,   erfcx)
NK_UNARY_ADAPTER(expint,  expint)
NK_UNARY_ADAPTER(psi,     psi)

#undef NK_UNARY_ADAPTER

void beta_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("beta: requires (Z, W)",
                     0, 0, "beta", "", "numkit:beta:nargin");
    outs[0] = beta(args[0], args[1], ctx.engine->resource());
}

void betaln_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("betaln: requires (Z, W)",
                     0, 0, "betaln", "", "numkit:betaln:nargin");
    outs[0] = betaln(args[0], args[1], ctx.engine->resource());
}

void gammainc_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gammainc: requires (X, A)", 0, 0, "gammainc", "", "numkit:gammainc:nargin");
    outs[0] = gammainc(args[0], args[1], ctx.engine->resource());
}

void betainc_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betainc: requires (X, A, B)", 0, 0, "betainc", "", "numkit:betainc:nargin");
    outs[0] = betainc(args[0], args[1], args[2], ctx.engine->resource());
}

void legendre_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("legendre: requires (n, x)", 0, 0, "legendre", "", "numkit:legendre:nargin");
    const int n = static_cast<int>(args[0].toScalar());
    outs[0] = legendre(n, args[1], ctx.engine->resource());
}

#define NK_BESSEL_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                  \
                    Span<Value> outs, CallContext &ctx)                          \
    {                                                                              \
        if (args.size() < 2)                                                       \
            throw Error(#name ": requires (nu, x)",                              \
                         0, 0, #name, "", "numkit:" #name ":nargin");                  \
        outs[0] = name(args[0], args[1], ctx.engine->resource());                \
    }

NK_BESSEL_REG(besselj)
NK_BESSEL_REG(bessely)
NK_BESSEL_REG(besseli)
NK_BESSEL_REG(besselk)

#undef NK_BESSEL_REG

void besselh_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("besselh: requires (nu, x) or (nu, k, x)",
                     0, 0, "besselh", "", "numkit:besselh:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // 2-arg form defaults to k = 1.
        outs[0] = besselh(args[0], 1, args[1], mr);
        return;
    }
    const int k = static_cast<int>(args[1].toScalar());
    outs[0] = besselh(args[0], k, args[2], mr);
}

void ellipke_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ellipke: requires 1 argument",
                     0, 0, "ellipke", "", "numkit:ellipke:nargin");
    auto res = ellipke(args[0], ctx.engine->resource());
    outs[0] = std::move(res.K);
    if (nargout > 1) outs[1] = std::move(res.E);
}

void airy_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("airy: requires at least 1 argument (x or k,x)",
                     0, 0, "airy", "", "numkit:airy:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = airy(0, args[0], mr);  // default Ai
        return;
    }
    int k = static_cast<int>(args[0].toScalar());
    outs[0] = airy(k, args[1], mr);
}

void gammaincinv_reg(Span<const Value> args, size_t, Span<Value> outs,
                     CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gammaincinv: requires 2 arguments (P, a)",
                     0, 0, "gammaincinv", "", "numkit:gammaincinv:nargin");
    outs[0] = gammaincinv(args[0], args[1], ctx.engine->resource());
}

void betaincinv_reg(Span<const Value> args, size_t, Span<Value> outs,
                    CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("betaincinv: requires 3 arguments (P, a, b)",
                     0, 0, "betaincinv", "", "numkit:betaincinv:nargin");
    outs[0] = betaincinv(args[0], args[1], args[2], ctx.engine->resource());
}

void ellipj_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ellipj: requires 2 arguments (u, m)",
                     0, 0, "ellipj", "", "numkit:ellipj:nargin");
    auto r = ellipj(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(r.sn);
    if (nargout > 1) outs[1] = std::move(r.cn);
    if (nargout > 2) outs[2] = std::move(r.dn);
}

} // namespace detail

} // namespace numkit::builtin
