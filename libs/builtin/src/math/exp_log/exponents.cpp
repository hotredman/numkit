// libs/builtin/src/math/elementary/exponents.cpp
//
// Scalar exponentials/logarithms: sqrt, log2, log10, expm1, log1p.
// exp / log live in libs/builtin/src/backends/MStdTranscendental_*.cpp
// (SIMD-backed) and only their declarations are reproduced in
// math/elementary/exponents.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

Value sqrt(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::sqrt(Complex(x.toScalar(), 0.0)), mr);
    return unaryDouble(x, [](double v) { return std::sqrt(v); }, mr);
}

Value log2(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::log2(v); }, mr);
}

Value log10(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::log10(v); }, mr);
}

Value expm1(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, mr);
}

Value log1p(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::log1p(v); }, mr);
}

// ── pow2 / realpow / reallog / realsqrt ──────────────────────────────

Value pow2(const Value &y, std::pmr::memory_resource *mr)
{
    return unaryDouble(y, [](double v) { return std::exp2(v); }, mr);
}

Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr)
{
    // ldexp(f, int_e) = f * 2^int_e. MATLAB's pow2(F, E) takes the
    // floor of E for the integer exponent.
    return elementwiseDouble(f, e,
        [](double ff, double ee) {
            return std::ldexp(ff, static_cast<int>(std::floor(ee)));
        }, mr);
}

Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    // Emit error if any (x_i < 0) AND (y_i is not an integer).
    auto checkPair = [](double xx, double yy) {
        if (xx < 0.0 && yy != std::floor(yy)) {
            throw std::runtime_error(
                "realpow produced complex result — use power(.^) instead");
        }
        return std::pow(xx, yy);
    };
    return elementwiseDouble(x, y, checkPair, mr);
}

Value reallog(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        if (v < 0.0)
            throw std::runtime_error(
                "reallog produced complex result — use log(...) instead");
        return std::log(v);
    }, mr);
}

Value realsqrt(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        if (v < 0.0)
            throw std::runtime_error(
                "realsqrt produced complex result — use sqrt(...) instead");
        return std::sqrt(v);
    }, mr);
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
        outs[0] = fn(args[0], ctx.engine->resource());                          \
    }

NK_UNARY_ADAPTER(sqrt,  sqrt)
NK_UNARY_ADAPTER(log2,  log2)
NK_UNARY_ADAPTER(log10, log10)
NK_UNARY_ADAPTER(expm1, expm1)
NK_UNARY_ADAPTER(log1p, log1p)

NK_UNARY_ADAPTER(reallog,  reallog)
NK_UNARY_ADAPTER(realsqrt, realsqrt)

#undef NK_UNARY_ADAPTER

void pow2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pow2: requires 1 or 2 arguments",
                     0, 0, "pow2", "", "m:pow2:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 2)
        outs[0] = pow2(args[0], args[1], mr);
    else
        outs[0] = pow2(args[0], mr);
}

void realpow_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("realpow: requires 2 arguments",
                     0, 0, "realpow", "", "m:realpow:nargin");
    outs[0] = realpow(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
