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
Value roundLikeDispatch(std::pmr::memory_resource *mr, const Value &x,
                        ScalarOp scalar, SimdOp simdLoop)
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

Value floor(std::pmr::memory_resource *mr, const Value &x)
{
    return roundLikeDispatch(mr, x,
        [](double v) { return std::floor(v); },
        ::numkit::builtin::detail::doubleFloorLoop);
}

Value ceil(std::pmr::memory_resource *mr, const Value &x)
{
    return roundLikeDispatch(mr, x,
        [](double v) { return std::ceil(v); },
        ::numkit::builtin::detail::doubleCeilLoop);
}

Value round(std::pmr::memory_resource *mr, const Value &x)
{
    return roundLikeDispatch(mr, x,
        [](double v) { return std::round(v); },
        ::numkit::builtin::detail::doubleRoundLoop);
}

Value fix(std::pmr::memory_resource *mr, const Value &x)
{
    return roundLikeDispatch(mr, x,
        [](double v) { return std::trunc(v); },
        ::numkit::builtin::detail::doubleFixLoop);
}

Value sign(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : (v > 0) ? 1.0 : (v < 0 ? -1.0 : 0.0);
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
        outs[0] = fn(ctx.engine->resource(), args[0]);                          \
    }

NK_UNARY_ADAPTER(floor, floor)
NK_UNARY_ADAPTER(ceil,  ceil)
NK_UNARY_ADAPTER(round, round)
NK_UNARY_ADAPTER(fix,   fix)
NK_UNARY_ADAPTER(sign,  sign)

#undef NK_UNARY_ADAPTER

} // namespace detail

} // namespace numkit::builtin
