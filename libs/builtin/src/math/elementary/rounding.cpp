// libs/builtin/src/math/elementary/rounding.cpp
//
// Rounding and sign builtins. abs lives in libs/builtin/src/backends/
// MStdAbs_*.cpp (SIMD-backed) and only its declaration is in
// math/elementary/rounding.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/elementary/rounding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>

namespace numkit::builtin {

Value floor(Allocator &alloc, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::floor(v); }, &alloc);
}

Value ceil(Allocator &alloc, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::ceil(v); }, &alloc);
}

Value round(Allocator &alloc, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::round(v); }, &alloc);
}

Value fix(Allocator &alloc, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::trunc(v); }, &alloc);
}

Value sign(Allocator &alloc, const Value &x)
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
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
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

} // namespace numkit::builtin
