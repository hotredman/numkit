// toolboxes/builtin/src/math/elementary/exponents.cpp
//
// Scalar exponentials: sqrt, pow2, realpow, realsqrt (+ engine adapters).
// exp / log / log2 / log10 / log1p / expm1 / reallog are backend-split
// (SIMD via Highway) and live in exp_log_highway.cpp / exp_log_portable.cpp;
// only their declarations are reproduced in math/exp_log/exponents.hpp.

#include <numkit/math/exp_log/exponents.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
#include "../_unary_hint.hpp"   // 3-arg exp/log hint overloads

#include <cmath>
#include <complex>

namespace numkit::math {

// Public 2-arg wrappers — delegate to the 3-arg overload in the SIMD
// backends with no buffer hint.
Value exp(const Value &x, std::pmr::memory_resource *mr) { return exp(x, nullptr, mr); }
Value log(const Value &x, std::pmr::memory_resource *mr) { return log(x, nullptr, mr); }


// sqrt / exp / log / log2 / log10 / log1p / expm1 / reallog / realsqrt are
// all backend-split (SIMD via Highway) — see exp_log_highway.cpp /
// exp_log_portable.cpp. Only their declarations live in exponents.hpp.

// ── pow2 / realpow ───────────────────────────────────────────────────

// pow2(y) == 2^y is backend-split (SIMD via Highway Exp2Loop) — see
// exp_log_highway.cpp / exp_log_portable.cpp. The 2-arg pow2(f, e) below
// stays here (it is an ldexp, already cheap).
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

} // namespace numkit::math
