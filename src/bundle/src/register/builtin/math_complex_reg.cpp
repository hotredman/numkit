// math/src/complex/complex_reg.cpp
//
// CallContext register half of math/complex/complex.cpp (Phase 2b split).
#include <nk/core/engine.hpp>
#include <nk/core/engine.hpp>
#include <nk/builtin/elfun.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/helpers.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <vector>

namespace nk::builtin {

using namespace nk::builtin;  // C4c: localized using (umbrella shim removed)

namespace detail {

void real_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("real: requires 1 argument", 0, 0, "real", "", "nk:real:nargin");
    outs[0] = real(args[0], ctx.engine->resource());
}

void imag_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imag: requires 1 argument", 0, 0, "imag", "", "nk:imag:nargin");
    outs[0] = imag(args[0], ctx.engine->resource());
}

void conj_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("conj: requires 1 argument", 0, 0, "conj", "", "nk:conj:nargin");
    outs[0] = conj(args[0], ctx.engine->resource());
}

void complex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("complex: requires 1 or 2 arguments", 0, 0, "complex", "",
                     "nk:complex:nargin");
    if (args.size() == 1)
        outs[0] = complex(args[0], ctx.engine->resource());
    else
        outs[0] = complex(args[0], args[1], ctx.engine->resource());
}

void angle_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("angle: requires 1 argument", 0, 0, "angle", "", "nk:angle:nargin");
    outs[0] = angle(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace nk::builtin
