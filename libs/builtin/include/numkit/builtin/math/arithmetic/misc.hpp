// libs/builtin/include/numkit/builtin/math/arithmetic/misc.hpp
//
// Miscellaneous elementary-math builtins that don't naturally group
// under trigonometry / exponents / rounding.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

Value deg2rad(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value rad2deg(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// mod(a, b) — modulo with sign of divisor (a - floor(a/b)*b).
Value mod(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// rem(a, b) — IEEE remainder with sign of dividend (std::fmod).
Value rem(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// hypot(x, y) — sqrt(x^2 + y^2) without intermediate overflow.
Value hypot(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// nthroot(x, n) — real n-th root. Negative x with odd n produces a
/// negative real (unlike `x .^ (1/n)` which goes complex).
Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
