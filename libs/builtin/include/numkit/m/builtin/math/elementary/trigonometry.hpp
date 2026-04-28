// libs/builtin/include/numkit/m/builtin/math/elementary/trigonometry.hpp
//
// Trigonometric and hyperbolic-style builtins. sin/cos go through the
// SIMD-backed transcendentals (libs/builtin/src/backends/MStdTranscendental_*.cpp);
// tan/asin/acos/atan/atan2 are scalar (Highway has equivalents but they
// haven't been wired in).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// `hint` is passed through to SIMD-backed unaries — see
/// `math/elementary/exponents.hpp` for the contract.
MValue sin(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue cos(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue tan(Allocator &alloc, const MValue &x);
MValue asin(Allocator &alloc, const MValue &x);
MValue acos(Allocator &alloc, const MValue &x);
MValue atan(Allocator &alloc, const MValue &x);
MValue atan2(Allocator &alloc, const MValue &y, const MValue &x);

} // namespace numkit::m::builtin
