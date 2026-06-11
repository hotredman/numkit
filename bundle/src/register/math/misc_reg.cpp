// toolboxes/signal/src/math/arithmetic/misc_reg.cpp
//
// CallContext register half of math/arithmetic/misc.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/math/arithmetic/misc.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "helpers.hpp"
#include "arithmetic/mod_simd.hpp"
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
                     0, 0, "mod", "", "numkit:mod:nargin");
    outs[0] = mod(args[0], args[1], ctx.engine->resource());
}

void rem_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rem: requires 2 arguments",
                     0, 0, "rem", "", "numkit:rem:nargin");
    outs[0] = rem(args[0], args[1], ctx.engine->resource());
}

void hypot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("hypot: requires 2 arguments",
                     0, 0, "hypot", "", "numkit:hypot:nargin");
    outs[0] = hypot(args[0], args[1], ctx.engine->resource());
}

void nthroot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nthroot: requires 2 arguments",
                     0, 0, "nthroot", "", "numkit:nthroot:nargin");
    outs[0] = nthroot(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
