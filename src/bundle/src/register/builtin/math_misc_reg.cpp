// toolboxes/signal/src/math/arithmetic/misc_reg.cpp
//
// CallContext register half of math/arithmetic/misc.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/core/engine.hpp>
#include <nk/builtin/elfun.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/helpers.hpp>
#include "elfun/mod_simd.hpp"
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::builtin {
using namespace nk::builtin;  // C4c localized (umbrella removed)

namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "nk:" #name ":nargin");                 \
        outs[0] = fn(args[0], ctx.engine->resource());                          \
    }

NK_UNARY_ADAPTER(deg2rad, deg2rad)
NK_UNARY_ADAPTER(rad2deg, rad2deg)
NK_UNARY_ADAPTER(wrapToPi, wrapToPi)
NK_UNARY_ADAPTER(wrapTo2Pi, wrapTo2Pi)
NK_UNARY_ADAPTER(wrapTo180, wrapTo180)
NK_UNARY_ADAPTER(wrapTo360, wrapTo360)

#undef NK_UNARY_ADAPTER

void mod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mod: requires 2 arguments",
                     0, 0, "mod", "", "nk:mod:nargin");
    outs[0] = mod(args[0], args[1], ctx.engine->resource());
}

void rem_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rem: requires 2 arguments",
                     0, 0, "rem", "", "nk:rem:nargin");
    outs[0] = rem(args[0], args[1], ctx.engine->resource());
}

void hypot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("hypot: requires 2 arguments",
                     0, 0, "hypot", "", "nk:hypot:nargin");
    outs[0] = hypot(args[0], args[1], ctx.engine->resource());
}

void nthroot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nthroot: requires 2 arguments",
                     0, 0, "nthroot", "", "nk:nthroot:nargin");
    outs[0] = nthroot(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::builtin
