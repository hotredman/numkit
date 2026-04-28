// libs/builtin/src/math/elementary/trigonometry.cpp
//
// tan / asin / acos / atan / atan2. sin / cos live in
// libs/builtin/src/backends/MStdTranscendental_*.cpp (SIMD-backed).

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/elementary/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

Value tan(Allocator &alloc, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::tan(v); }, &alloc);
}

Value asin(Allocator &alloc, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::asin(v); }, &alloc);
}

Value acos(Allocator &alloc, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::acos(v); }, &alloc);
}

Value atan(Allocator &alloc, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::atan(v); }, &alloc);
}

Value atan2(Allocator &alloc, const Value &y, const Value &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, &alloc);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = fn(ctx.engine->allocator(), args[0]);                          \
    }

NK_UNARY_ADAPTER(tan,  tan)
NK_UNARY_ADAPTER(asin, asin)
NK_UNARY_ADAPTER(acos, acos)
NK_UNARY_ADAPTER(atan, atan)

#undef NK_UNARY_ADAPTER

void atan2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("atan2: requires 2 arguments",
                     0, 0, "atan2", "", "m:atan2:nargin");
    outs[0] = atan2(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
