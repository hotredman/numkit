// libs/builtin/src/math/elementary/rounding.cpp
//
// Rounding and sign builtins. abs lives in libs/builtin/src/backends/
// MStdAbs_*.cpp (SIMD-backed) and only its declaration is in
// math/elementary/rounding.hpp.

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/math/elementary/rounding.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>

namespace numkit::m::builtin {

MValue floor(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::floor(v); }, &alloc);
}

MValue ceil(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::ceil(v); }, &alloc);
}

MValue round(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::round(v); }, &alloc);
}

MValue fix(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::trunc(v); }, &alloc);
}

MValue sign(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : (v > 0) ? 1.0 : (v < 0 ? -1.0 : 0.0);
                       },
                       &alloc);
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

NK_UNARY_ADAPTER(floor, floor)
NK_UNARY_ADAPTER(ceil,  ceil)
NK_UNARY_ADAPTER(round, round)
NK_UNARY_ADAPTER(fix,   fix)
NK_UNARY_ADAPTER(sign,  sign)

#undef NK_UNARY_ADAPTER

} // namespace detail

} // namespace numkit::m::builtin
