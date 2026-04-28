// libs/builtin/include/numkit/m/builtin/math/elementary/misc.hpp
//
// Miscellaneous elementary-math builtins that don't naturally group
// under trigonometry / exponents / rounding.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

MValue deg2rad(Allocator &alloc, const MValue &x);
MValue rad2deg(Allocator &alloc, const MValue &x);

/// mod(a, b) — modulo with sign of divisor (a - floor(a/b)*b).
MValue mod(Allocator &alloc, const MValue &a, const MValue &b);

/// rem(a, b) — IEEE remainder with sign of dividend (std::fmod).
MValue rem(Allocator &alloc, const MValue &a, const MValue &b);

/// hypot(x, y) — sqrt(x^2 + y^2) without intermediate overflow.
MValue hypot(Allocator &alloc, const MValue &x, const MValue &y);

/// nthroot(x, n) — real n-th root. Negative x with odd n produces a
/// negative real (unlike `x .^ (1/n)` which goes complex).
MValue nthroot(Allocator &alloc, const MValue &x, const MValue &n);

} // namespace numkit::m::builtin
