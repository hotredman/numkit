// toolboxes/signal/src/math/trig/trigonometry_reg.cpp
//
// CallContext register half of math/trig/trigonometry.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/math/arithmetic/misc.hpp>          // hypot decl
#include <numkit/math/trig/trigonometry.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "_unary_hint.hpp"   // 3-arg sin/cos hint overloads
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
using namespace numkit::math;  // C4c localized (umbrella removed)

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

NK_UNARY_ADAPTER(tan,  tan)
NK_UNARY_ADAPTER(asin, asin)
NK_UNARY_ADAPTER(acos, acos)
NK_UNARY_ADAPTER(atan, atan)

NK_UNARY_ADAPTER(sinh,  sinh)
NK_UNARY_ADAPTER(cosh,  cosh)
NK_UNARY_ADAPTER(tanh,  tanh)
NK_UNARY_ADAPTER(asinh, asinh)
NK_UNARY_ADAPTER(acosh, acosh)
NK_UNARY_ADAPTER(atanh, atanh)

NK_UNARY_ADAPTER(sind,  sind)
NK_UNARY_ADAPTER(cosd,  cosd)
NK_UNARY_ADAPTER(tand,  tand)
NK_UNARY_ADAPTER(asind, asind)
NK_UNARY_ADAPTER(acosd, acosd)
NK_UNARY_ADAPTER(atand, atand)

NK_UNARY_ADAPTER(sinpi, sinpi)
NK_UNARY_ADAPTER(cospi, cospi)

NK_UNARY_ADAPTER(sec,   sec)
NK_UNARY_ADAPTER(csc,   csc)
NK_UNARY_ADAPTER(cot,   cot)
NK_UNARY_ADAPTER(sech,  sech)
NK_UNARY_ADAPTER(csch,  csch)
NK_UNARY_ADAPTER(coth,  coth)
NK_UNARY_ADAPTER(secd,  secd)
NK_UNARY_ADAPTER(cscd,  cscd)
NK_UNARY_ADAPTER(cotd,  cotd)
NK_UNARY_ADAPTER(asec,  asec)
NK_UNARY_ADAPTER(acsc,  acsc)
NK_UNARY_ADAPTER(acot,  acot)
NK_UNARY_ADAPTER(asech, asech)
NK_UNARY_ADAPTER(acsch, acsch)
NK_UNARY_ADAPTER(acoth, acoth)
NK_UNARY_ADAPTER(asecd, asecd)
NK_UNARY_ADAPTER(acscd, acscd)
NK_UNARY_ADAPTER(acotd, acotd)

#undef NK_UNARY_ADAPTER

void atan2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("atan2: requires 2 arguments",
                     0, 0, "atan2", "", "numkit:atan2:nargin");
    outs[0] = atan2(args[0], args[1], ctx.engine->resource());
}

void atan2d_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("atan2d: requires 2 arguments",
                     0, 0, "atan2d", "", "numkit:atan2d:nargin");
    outs[0] = atan2d(args[0], args[1], ctx.engine->resource());
}

void cart2pol_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cart2pol: requires at least 2 arguments",
                     0, 0, "cart2pol", "", "numkit:cart2pol:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        auto [theta, rho, z] = cart2pol(args[0], args[1], args[2], mr);
        outs[0] = std::move(theta);
        if (nargout > 1) outs[1] = std::move(rho);
        if (nargout > 2) outs[2] = std::move(z);
        return;
    }
    auto [theta, rho] = cart2pol(args[0], args[1], mr);
    outs[0] = std::move(theta);
    if (nargout > 1) outs[1] = std::move(rho);
}

void pol2cart_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pol2cart: requires at least 2 arguments",
                     0, 0, "pol2cart", "", "numkit:pol2cart:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        auto [xv, yv, zv] = pol2cart(args[0], args[1], args[2], mr);
        outs[0] = std::move(xv);
        if (nargout > 1) outs[1] = std::move(yv);
        if (nargout > 2) outs[2] = std::move(zv);
        return;
    }
    auto [xv, yv] = pol2cart(args[0], args[1], mr);
    outs[0] = std::move(xv);
    if (nargout > 1) outs[1] = std::move(yv);
}

void cart2sph_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cart2sph: requires 3 arguments",
                     0, 0, "cart2sph", "", "numkit:cart2sph:nargin");
    auto [az, el, r] = cart2sph(args[0], args[1], args[2], ctx.engine->resource());
    outs[0] = std::move(az);
    if (nargout > 1) outs[1] = std::move(el);
    if (nargout > 2) outs[2] = std::move(r);
}

void sph2cart_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("sph2cart: requires 3 arguments",
                     0, 0, "sph2cart", "", "numkit:sph2cart:nargin");
    auto [xv, yv, zv] = sph2cart(args[0], args[1], args[2], ctx.engine->resource());
    outs[0] = std::move(xv);
    if (nargout > 1) outs[1] = std::move(yv);
    if (nargout > 2) outs[2] = std::move(zv);
}

} // namespace detail

} // namespace numkit::builtin
