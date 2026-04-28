// libs/builtin/include/numkit/builtin/lang/operators/binary_ops.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Arithmetic ───────────────────────────────────────────────────────
/// a + b. Numeric addition with broadcasting; string concatenation for
/// char/string operands; mixed char+double promotes char to double.
Value plus(Allocator &alloc, const Value &a, const Value &b);

/// a - b. Numeric subtraction with broadcasting.
Value minus(Allocator &alloc, const Value &a, const Value &b);

/// a .* b — elementwise multiplication with broadcasting.
Value times(Allocator &alloc, const Value &a, const Value &b);

/// a * b — matrix multiplication (MxK * KxN → MxN). Scalars broadcast
/// to elementwise multiplication.
Value mtimes(Allocator &alloc, const Value &a, const Value &b);

/// a ./ b — elementwise right division with broadcasting.
Value rdivide(Allocator &alloc, const Value &a, const Value &b);

/// a / b — matrix right division (currently only scalar denominator and
/// scalar/scalar; matrix right division not implemented).
Value mrdivide(Allocator &alloc, const Value &a, const Value &b);

/// a \ b — matrix left division (currently scalar/scalar only).
Value mldivide(Allocator &alloc, const Value &a, const Value &b);

/// a ^ b — matrix/scalar power (scalar/scalar only; matrix power NYI).
Value power(Allocator &alloc, const Value &a, const Value &b);

/// a .^ b — elementwise power with broadcasting.
Value elementPower(Allocator &alloc, const Value &a, const Value &b);

// ── Comparisons (return logical) ─────────────────────────────────────
Value eq(Allocator &alloc, const Value &a, const Value &b);
Value ne(Allocator &alloc, const Value &a, const Value &b);
Value lt(Allocator &alloc, const Value &a, const Value &b);
Value gt(Allocator &alloc, const Value &a, const Value &b);
Value le(Allocator &alloc, const Value &a, const Value &b);
Value ge(Allocator &alloc, const Value &a, const Value &b);

// ── Logical (elementwise) ────────────────────────────────────────────
/// a & b — elementwise logical AND (non-zero-to-bool coercion).
Value logicalAnd(Allocator &alloc, const Value &a, const Value &b);

/// a | b — elementwise logical OR.
Value logicalOr(Allocator &alloc, const Value &a, const Value &b);

} // namespace numkit::builtin
