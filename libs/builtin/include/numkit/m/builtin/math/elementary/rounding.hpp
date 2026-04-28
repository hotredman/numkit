// libs/builtin/include/numkit/m/builtin/math/elementary/rounding.hpp
//
// Rounding and sign builtins. abs has a SIMD backend (libs/builtin/src/
// backends/MStdAbs_*.cpp); the rest are scalar wrappers.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// `hint` — see math/elementary/exponents.hpp for the contract.
MValue abs(Allocator &alloc, const MValue &x, MValue *hint = nullptr);

MValue floor(Allocator &alloc, const MValue &x);
MValue ceil(Allocator &alloc, const MValue &x);
MValue round(Allocator &alloc, const MValue &x);

/// fix(x) — truncate toward zero.
MValue fix(Allocator &alloc, const MValue &x);

MValue sign(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
