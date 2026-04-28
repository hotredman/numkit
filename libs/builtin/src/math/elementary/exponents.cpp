// libs/builtin/src/math/elementary/exponents.cpp
//
// Scalar exponentials/logarithms: sqrt, log2, log10, expm1, log1p.
// exp / log live in libs/builtin/src/backends/MStdTranscendental_*.cpp
// (SIMD-backed) and only their declarations are reproduced in
// math/elementary/exponents.hpp.

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/math/elementary/exponents.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

MValue sqrt(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::sqrt(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryDouble(x, [](double v) { return std::sqrt(v); }, &alloc);
}

MValue log2(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log2(v); }, &alloc);
}

MValue log10(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log10(v); }, &alloc);
}

MValue expm1(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, &alloc);
}

MValue log1p(Allocator &alloc, const MValue &x)
{
    return unaryDouble(x, [](double v) { return std::log1p(v); }, &alloc);
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

NK_UNARY_ADAPTER(sqrt,  sqrt)
NK_UNARY_ADAPTER(log2,  log2)
NK_UNARY_ADAPTER(log10, log10)
NK_UNARY_ADAPTER(expm1, expm1)
NK_UNARY_ADAPTER(log1p, log1p)

#undef NK_UNARY_ADAPTER

} // namespace detail

} // namespace numkit::m::builtin
