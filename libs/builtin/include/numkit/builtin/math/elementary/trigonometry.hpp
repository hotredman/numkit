// libs/builtin/include/numkit/builtin/math/elementary/trigonometry.hpp
//
// Trigonometric and hyperbolic-style builtins. sin/cos go through the
// SIMD-backed transcendentals (libs/builtin/src/backends/MStdTranscendental_*.cpp);
// tan/asin/acos/atan/atan2 are scalar (Highway has equivalents but they
// haven't been wired in).

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// `hint` is passed through to SIMD-backed unaries — see
/// `math/elementary/exponents.hpp` for the contract.
Value sin(Allocator &alloc, const Value &x, Value *hint = nullptr);
Value cos(Allocator &alloc, const Value &x, Value *hint = nullptr);
Value tan(Allocator &alloc, const Value &x);
Value asin(Allocator &alloc, const Value &x);
Value acos(Allocator &alloc, const Value &x);
Value atan(Allocator &alloc, const Value &x);
Value atan2(Allocator &alloc, const Value &y, const Value &x);

} // namespace numkit::builtin
