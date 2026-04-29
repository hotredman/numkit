// libs/builtin/include/numkit/builtin/math/elementary/trigonometry.hpp
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

} // namespace numkit::builtin
