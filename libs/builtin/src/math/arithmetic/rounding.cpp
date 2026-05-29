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
#include "../_unary_hint.hpp"  // 3-arg abs hint overload

#include <cmath>

namespace numkit::builtin {

// Public 2-arg wrapper — delegates to the 3-arg overload in the SIMD
// backends with no buffer hint.
Value abs(const Value &x, std::pmr::memory_resource *mr) { return abs(x, nullptr, mr); }


namespace {

// Shared shape: scalar shortcut → result; double-array → SIMD/portable
// loop; other numeric types → fall through to unaryDouble (which calls
// Value::elemAsDouble per element).
template <typename ScalarOp, typename SimdOp>
Value roundLikeDispatch(const Value &x, ScalarOp scalar, SimdOp simdLoop, std::pmr::memory_resource *mr)
{
    // floor/ceil/round/fix are the IDENTITY on integer-typed values; MATLAB
    // keeps the integer class. (The double path below would throw on integer
    // storage / drop the class for a scalar.)
    if (isIntegerType(x.type()))
        return copyIntegerSameClass(x, mr);
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

// round(x, N): N decimal places (N may be negative). round(x, N,
// 'significant'): N significant digits. Round-half-away-from-zero (MATLAB).
inline double roundNScalar(double v, int n, bool significant)
{
    if (!std::isfinite(v)) return v;
    int digits = n;
    if (significant) {
        if (v == 0.0) return 0.0;
        digits = n - static_cast<int>(std::floor(std::log10(std::fabs(v)))) - 1;
    }
    const double f = std::pow(10.0, digits);
    return std::round(v * f) / f;
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

Value roundN(const Value &x, int n, bool significant, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [n, significant](double v) { return roundNScalar(v, n, significant); }, mr);
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
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        outs[0] = fn(args[0], ctx.engine->resource());                          \
    }

NK_UNARY_ADAPTER(floor,   floor)
NK_UNARY_ADAPTER(ceil,    ceil)
NK_UNARY_ADAPTER(fix,     fix)
NK_UNARY_ADAPTER(sign,    sign)
NK_UNARY_ADAPTER(subplus, subplus)

#undef NK_UNARY_ADAPTER

// round(x) | round(x, N) | round(x, N, 'decimals'|'significant').
void round_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("round: requires 1 argument",
                     0, 0, "round", "", "numkit:round:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() < 2 || args[1].isEmpty()) {
        outs[0] = round(args[0], mr);
        return;
    }
    const int n = static_cast<int>(args[1].toScalar());
    bool significant = false;
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        std::string s = args[2].toString();
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "significant") significant = true;
        else if (s == "decimals")    significant = false;
        else
            throw Error("round: type must be 'decimals' or 'significant'",
                         0, 0, "round", "", "numkit:round:badType");
    }
    if (significant && n < 1)
        throw Error("round: N must be >= 1 for 'significant'",
                     0, 0, "round", "", "numkit:round:badN");
    outs[0] = roundN(args[0], n, significant, mr);
}

} // namespace detail

} // namespace numkit::builtin
