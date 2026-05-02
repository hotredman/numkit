// libs/builtin/include/numkit/builtin/math/trig/trigonometry.hpp
//
// Trigonometric and hyperbolic-style builtins. sin/cos go through the
// SIMD-backed transcendentals (libs/builtin/src/backends/MStdTranscendental_*.cpp);
// tan/asin/acos/atan/atan2 are scalar (Highway has equivalents but they
// haven't been wired in).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// `hint` is passed through to SIMD-backed unaries — see
/// `math/elementary/exponents.hpp` for the contract.
Value sin(std::pmr::memory_resource *mr, const Value &x, Value *hint = nullptr);
Value cos(std::pmr::memory_resource *mr, const Value &x, Value *hint = nullptr);
Value tan(std::pmr::memory_resource *mr, const Value &x);
Value asin(std::pmr::memory_resource *mr, const Value &x);
Value acos(std::pmr::memory_resource *mr, const Value &x);
Value atan(std::pmr::memory_resource *mr, const Value &x);
Value atan2(std::pmr::memory_resource *mr, const Value &y, const Value &x);

// Hyperbolic — sinh/cosh/tanh and their inverses.
Value sinh(std::pmr::memory_resource *mr, const Value &x);
Value cosh(std::pmr::memory_resource *mr, const Value &x);
Value tanh(std::pmr::memory_resource *mr, const Value &x);
Value asinh(std::pmr::memory_resource *mr, const Value &x);
Value acosh(std::pmr::memory_resource *mr, const Value &x);
Value atanh(std::pmr::memory_resource *mr, const Value &x);

// Degree-input/-output forms — sind(x) = sin(x*pi/180) etc.
Value sind(std::pmr::memory_resource *mr, const Value &x);
Value cosd(std::pmr::memory_resource *mr, const Value &x);
Value tand(std::pmr::memory_resource *mr, const Value &x);
Value asind(std::pmr::memory_resource *mr, const Value &x);
Value acosd(std::pmr::memory_resource *mr, const Value &x);
Value atand(std::pmr::memory_resource *mr, const Value &x);
Value atan2d(std::pmr::memory_resource *mr, const Value &y, const Value &x);

// Pi-scaled forms — sinpi(x) = sin(pi*x), accurate near integers.
Value sinpi(std::pmr::memory_resource *mr, const Value &x);
Value cospi(std::pmr::memory_resource *mr, const Value &x);

// Reciprocal-trig family: sec/csc/cot and their hyperbolic / degree /
// inverse / inverse-hyperbolic / inverse-degree forms.
Value sec(std::pmr::memory_resource *mr, const Value &x);
Value csc(std::pmr::memory_resource *mr, const Value &x);
Value cot(std::pmr::memory_resource *mr, const Value &x);
Value sech(std::pmr::memory_resource *mr, const Value &x);
Value csch(std::pmr::memory_resource *mr, const Value &x);
Value coth(std::pmr::memory_resource *mr, const Value &x);
Value secd(std::pmr::memory_resource *mr, const Value &x);
Value cscd(std::pmr::memory_resource *mr, const Value &x);
Value cotd(std::pmr::memory_resource *mr, const Value &x);
Value asec(std::pmr::memory_resource *mr, const Value &x);
Value acsc(std::pmr::memory_resource *mr, const Value &x);
Value acot(std::pmr::memory_resource *mr, const Value &x);
Value asech(std::pmr::memory_resource *mr, const Value &x);
Value acsch(std::pmr::memory_resource *mr, const Value &x);
Value acoth(std::pmr::memory_resource *mr, const Value &x);
Value asecd(std::pmr::memory_resource *mr, const Value &x);
Value acscd(std::pmr::memory_resource *mr, const Value &x);
Value acotd(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
