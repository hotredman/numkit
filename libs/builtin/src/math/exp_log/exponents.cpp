// libs/builtin/src/math/elementary/exponents.cpp
//
// Scalar exponentials: sqrt, pow2, realpow, realsqrt (+ engine adapters).
// exp / log / log2 / log10 / log1p / expm1 / reallog are backend-split
// (SIMD via Highway) and live in exp_log_highway.cpp / exp_log_portable.cpp;
// only their declarations are reproduced in math/exp_log/exponents.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "../_unary_hint.hpp"   // 3-arg exp/log hint overloads

#include <cmath>
#include <complex>

namespace numkit::builtin {

// Public 2-arg wrappers — delegate to the 3-arg overload in the SIMD
// backends with no buffer hint.
Value exp(const Value &x, std::pmr::memory_resource *mr) { return exp(x, nullptr, mr); }
Value log(const Value &x, std::pmr::memory_resource *mr) { return log(x, nullptr, mr); }


// sqrt / exp / log / log2 / log10 / log1p / expm1 / reallog / realsqrt are
// all backend-split (SIMD via Highway) — see exp_log_highway.cpp /
// exp_log_portable.cpp. Only their declarations live in exponents.hpp.

// ── pow2 / realpow ───────────────────────────────────────────────────

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

// ── Engine adapters ──────────────────────────────────────────────────
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
                     0, 0, "pow2", "", "numkit:pow2:nargin");
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
                     0, 0, "realpow", "", "numkit:realpow:nargin");
    outs[0] = realpow(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
