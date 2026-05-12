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
Value sin(const Value &x, Value *hint = nullptr, std::pmr::memory_resource *mr = nullptr);
Value cos(const Value &x, Value *hint = nullptr, std::pmr::memory_resource *mr = nullptr);
Value tan(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asin(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acos(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value atan(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value atan2(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Hyperbolic — sinh/cosh/tanh and their inverses.
Value sinh(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cosh(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value tanh(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asinh(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acosh(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value atanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Degree-input/-output forms — sind(x) = sin(x*pi/180) etc.
Value sind(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cosd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value tand(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asind(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acosd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value atand(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Pi-scaled forms — sinpi(x) = sin(pi*x), accurate near integers.
Value sinpi(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cospi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Reciprocal-trig family: sec/csc/cot and their hyperbolic / degree /
/// inverse / inverse-hyperbolic / inverse-degree forms.
Value sec(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value csc(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cot(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value sech(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value csch(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value coth(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value secd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cscd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cotd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asec(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acsc(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acot(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asech(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acsch(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acoth(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value asecd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acscd(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value acotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Coordinate transforms — Cartesian ↔ polar / cylindrical / spherical.
/// 2-arg cart2pol/pol2cart are the planar (polar) form; 3-arg variants
/// pass z through unchanged (cylindrical).
struct PolarPair { Value theta, rho; };
struct CylTriple { Value theta, rho, z; };
struct CartPair  { Value x, y; };
struct CartTriple{ Value x, y, z; };
struct SphTriple { Value az, el, r; };

PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
CylTriple cart2pol(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);
CartPair  pol2cart(const Value &theta, const Value &rho, std::pmr::memory_resource *mr = nullptr);
CartTriple pol2cart(const Value &theta, const Value &rho, const Value &z, std::pmr::memory_resource *mr = nullptr);
SphTriple cart2sph(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);
CartTriple sph2cart(const Value &az, const Value &el, const Value &r, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
