// libs/builtin/include/numkit/builtin/math/elementary/misc.hpp
//
// Miscellaneous elementary-math builtins that don't naturally group
// under trigonometry / exponents / rounding.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

Value deg2rad(std::pmr::memory_resource *mr, const Value &x);
Value rad2deg(std::pmr::memory_resource *mr, const Value &x);

/// mod(a, b) — modulo with sign of divisor (a - floor(a/b)*b).
Value mod(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// rem(a, b) — IEEE remainder with sign of dividend (std::fmod).
Value rem(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// hypot(x, y) — sqrt(x^2 + y^2) without intermediate overflow.
Value hypot(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// nthroot(x, n) — real n-th root. Negative x with odd n produces a
/// negative real (unlike `x .^ (1/n)` which goes complex).
Value nthroot(std::pmr::memory_resource *mr, const Value &x, const Value &n);

} // namespace numkit::builtin
