// libs/builtin/include/numkit/builtin/math/elementary/rounding.hpp
//
// Rounding and sign builtins. abs has a SIMD backend (libs/builtin/src/
// backends/MStdAbs_*.cpp); the rest are scalar wrappers.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// `hint` — see math/elementary/exponents.hpp for the contract.
Value abs(Allocator &alloc, const Value &x, Value *hint = nullptr);

Value floor(Allocator &alloc, const Value &x);
Value ceil(Allocator &alloc, const Value &x);
Value round(Allocator &alloc, const Value &x);

/// fix(x) — truncate toward zero.
Value fix(Allocator &alloc, const Value &x);

Value sign(Allocator &alloc, const Value &x);

} // namespace numkit::builtin
