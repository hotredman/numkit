// toolboxes/builtin/src/math/elementary/rounding.cpp
//
// Rounding and sign builtins. abs lives in toolboxes/builtin/src/backends/
// MStdAbs_*.cpp (SIMD-backed) and only its declaration is in
// math/elementary/rounding.hpp.

#include <numkit/math/arithmetic/rounding.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include "rounding.hpp"      // detail::doubleCeilLoop / FloorLoop / RoundLoop / FixLoop
#include "../_unary_hint.hpp"  // 3-arg abs hint overload

#include <cmath>
#include <complex>

#include "rounding_detail.hpp"

namespace numkit::math {

// Public 2-arg wrapper — delegates to the 3-arg overload in the SIMD
// backends with no buffer hint.
Value abs(const Value &x, std::pmr::memory_resource *mr) { return abs(x, nullptr, mr); }



Value floor(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::floor(v); }, ::numkit::math::detail::doubleFloorLoop, mr);
}

Value ceil(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::ceil(v); }, ::numkit::math::detail::doubleCeilLoop, mr);
}

Value round(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::round(v); }, ::numkit::math::detail::doubleRoundLoop, mr);
}

Value roundN(const Value &x, int n, bool significant, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [n, significant](double v) { return roundNScalar(v, n, significant); }, mr);
}

Value fix(const Value &x, std::pmr::memory_resource *mr)
{
    return roundLikeDispatch(x, [](double v) { return std::trunc(v); }, ::numkit::math::detail::doubleFixLoop, mr);
}

Value sign(const Value &x, std::pmr::memory_resource *mr)
{
    auto signOp = [](double v) {
        return std::isnan(v) ? v : (v > 0) ? 1.0 : (v < 0 ? -1.0 : 0.0);
    };
    // Complex: sign(z) = z/|z| for z != 0, else 0 (MATLAB R2025b);
    // sign(3-4i) = 0.6-0.8i. The double path below can't take complex.
    if (x.isComplex()) {
        auto unit = [](Complex z) -> Complex {
            const double m = std::abs(z);
            return m == 0.0 ? Complex(0.0, 0.0) : z / m;
        };
        if (x.isScalar())
            return Value::complexScalar(unit(x.toComplex()), mr);
        Value out = createLike(x, ValueType::COMPLEX, mr);
        const Complex *src = x.complexData();
        Complex *dst = out.complexDataMut();
        for (std::size_t i = 0; i < x.numel(); ++i) dst[i] = unit(src[i]);
        return out;
    }
    // Integer types keep their class (sign(int8(-5))=-1 int8). Promote to
    // double first (unaryDouble's array path needs doubleData), then cast the
    // -1/0/1 result back to the integer class.
    if (isIntegerType(x.type())) {
        Value d = unaryDouble(toDoubleValue(x, mr), signOp, mr);
        return doubleToIntegerExact(d, x.type(), mr);
    }
    return unaryDouble(x, signOp, mr);
}

Value subplus(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x,
                       [](double v) {
                           return std::isnan(v) ? v : std::max(v, 0.0);
                       },
                       mr);
}

} // namespace numkit::math
