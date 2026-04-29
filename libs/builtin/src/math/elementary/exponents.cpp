// libs/builtin/src/math/elementary/exponents.cpp
//
// Scalar exponentials/logarithms: sqrt, log2, log10, expm1, log1p.
// exp / log live in libs/builtin/src/backends/MStdTranscendental_*.cpp
// (SIMD-backed) and only their declarations are reproduced in
// math/elementary/exponents.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/elementary/exponents.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

Value sqrt(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::sqrt(Complex(x.toScalar(), 0.0)), mr);
    return unaryDouble(x, [](double v) { return std::sqrt(v); }, mr);
}

Value log2(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::log2(v); }, mr);
}

Value log10(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::log10(v); }, mr);
}

Value expm1(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, mr);
}

Value log1p(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x, [](double v) { return std::log1p(v); }, mr);
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
        outs[0] = fn(ctx.engine->resource(), args[0]);                          \
    }

NK_UNARY_ADAPTER(sqrt,  sqrt)
NK_UNARY_ADAPTER(log2,  log2)
NK_UNARY_ADAPTER(log10, log10)
NK_UNARY_ADAPTER(expm1, expm1)
NK_UNARY_ADAPTER(log1p, log1p)

#undef NK_UNARY_ADAPTER

} // namespace detail

} // namespace numkit::builtin
