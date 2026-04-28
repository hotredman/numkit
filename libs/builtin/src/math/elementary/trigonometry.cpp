// libs/builtin/src/math/elementary/trigonometry.cpp
//
// tan / asin / acos / atan / atan2. sin / cos live in
// libs/builtin/src/backends/MStdTranscendental_*.cpp (SIMD-backed).

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/math/elementary/trigonometry.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

MValue tan(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::tan(v); }, &alloc);
}

MValue asin(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::asin(v); }, &alloc);
}

MValue acos(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::acos(v); }, &alloc);
}

MValue atan(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::atan(v); }, &alloc);
}

MValue atan2(Allocator &alloc, const MValue &y, const MValue &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, &alloc);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,                \
                    Span<MValue> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw MError(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = fn(ctx.engine->allocator(), args[0]);                          \
    }

NK_UNARY_ADAPTER(tan,  tan)
NK_UNARY_ADAPTER(asin, asin)
NK_UNARY_ADAPTER(acos, acos)
NK_UNARY_ADAPTER(atan, atan)

#undef NK_UNARY_ADAPTER

void atan2_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("atan2: requires 2 arguments",
                     0, 0, "atan2", "", "m:atan2:nargin");
    outs[0] = atan2(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::m::builtin
