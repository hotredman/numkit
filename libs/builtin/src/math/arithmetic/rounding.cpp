// libs/builtin/src/math/elementary/rounding.cpp
//
// Rounding and sign builtins. abs lives in libs/builtin/src/backends/
// MStdAbs_*.cpp (SIMD-backed) and only its declaration is in
// math/elementary/rounding.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/arithmetic/rounding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "rounding.hpp"      // detail::doubleCeilLoop / FloorLoop / RoundLoop / FixLoop

#include <cmath>

namespace numkit::builtin {

namespace {

// Shared shape: scalar shortcut → result; double-array → SIMD/portable
// loop; other numeric types → fall through to unaryDouble (which calls
// Value::elemAsDouble per element).
template <typename ScalarOp, typename SimdOp>
Value roundLikeDispatch(const Value &x, ScalarOp scalar, SimdOp simdLoop, std::pmr::memory_resource *mr)
{
    if (x.isScalar())
        return Value::scalar(scalar(x.toScalar()), mr);
    if (x.type() == ValueType::DOUBLE) {
        Value r = createLike(x, ValueType::DOUBLE, mr);
        if (x.numel() == 0) return r;
        simdLoop(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }
    return unaryDouble(x, scalar, mr);
}

} // namespace

Value floor(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::floor(v); }, ::numkit::builtin::detail::doubleFloorLoop, mr);
}

Value ceil(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::ceil(v); }, ::numkit::builtin::detail::doubleCeilLoop, mr);
}

Value round(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::round(v); }, ::numkit::builtin::detail::doubleRoundLoop, mr);
}

Value fix(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::trunc(v); }, ::numkit::builtin::detail::doubleFixLoop, mr);
}

Value sign(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : (v > 0) ? 1.0 : (v < 0 ? -1.0 : 0.0);
                       },
                       mr);
}

Value subplus(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : std::max(v, 0.0);
                       },
                       mr);
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

NK_UNARY_ADAPTER(floor,   floor)
NK_UNARY_ADAPTER(ceil,    ceil)
NK_UNARY_ADAPTER(round,   round)
NK_UNARY_ADAPTER(fix,     fix)
NK_UNARY_ADAPTER(sign,    sign)
NK_UNARY_ADAPTER(subplus, subplus)

#undef NK_UNARY_ADAPTER

} // namespace detail

} // namespace numkit::builtin
