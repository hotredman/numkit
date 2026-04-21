// libs/builtin/include/numkit/m/builtin/MStdBinaryOps.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

// ── Arithmetic ───────────────────────────────────────────────────────
/// a + b. Numeric addition with broadcasting; string concatenation for
/// char/string operands; mixed char+double promotes char to double.
MValue plus(Allocator &alloc, const MValue &a, const MValue &b);

/// a - b. Numeric subtraction with broadcasting.
MValue minus(Allocator &alloc, const MValue &a, const MValue &b);

/// a .* b — elementwise multiplication with broadcasting.
MValue times(Allocator &alloc, const MValue &a, const MValue &b);

/// a * b — matrix multiplication (MxK * KxN → MxN). Scalars broadcast
/// to elementwise multiplication.
MValue mtimes(Allocator &alloc, const MValue &a, const MValue &b);

/// a ./ b — elementwise right division with broadcasting.
MValue rdivide(Allocator &alloc, const MValue &a, const MValue &b);

/// a / b — matrix right division (currently only scalar denominator and
/// scalar/scalar; matrix right division not implemented).
MValue mrdivide(Allocator &alloc, const MValue &a, const MValue &b);

/// a \ b — matrix left division (currently scalar/scalar only).
MValue mldivide(Allocator &alloc, const MValue &a, const MValue &b);

/// a ^ b — matrix/scalar power (scalar/scalar only; matrix power NYI).
MValue power(Allocator &alloc, const MValue &a, const MValue &b);

/// a .^ b — elementwise power with broadcasting.
MValue elementPower(Allocator &alloc, const MValue &a, const MValue &b);

// ── Comparisons (return logical) ─────────────────────────────────────
MValue eq(Allocator &alloc, const MValue &a, const MValue &b);
MValue ne(Allocator &alloc, const MValue &a, const MValue &b);
MValue lt(Allocator &alloc, const MValue &a, const MValue &b);
MValue gt(Allocator &alloc, const MValue &a, const MValue &b);
MValue le(Allocator &alloc, const MValue &a, const MValue &b);
MValue ge(Allocator &alloc, const MValue &a, const MValue &b);

// ── Logical (elementwise) ────────────────────────────────────────────
/// a & b — elementwise logical AND (non-zero-to-bool coercion).
MValue logicalAnd(Allocator &alloc, const MValue &a, const MValue &b);

/// a | b — elementwise logical OR.
MValue logicalOr(Allocator &alloc, const MValue &a, const MValue &b);

} // namespace numkit::m::builtin
