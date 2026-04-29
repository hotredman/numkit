// libs/builtin/include/numkit/builtin/lang/operators/binary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Arithmetic ───────────────────────────────────────────────────────
/// a + b. Numeric addition with broadcasting; string concatenation for
/// char/string operands; mixed char+double promotes char to double.
Value plus(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a - b. Numeric subtraction with broadcasting.
Value minus(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a .* b — elementwise multiplication with broadcasting.
Value times(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a * b — matrix multiplication (MxK * KxN → MxN). Scalars broadcast
/// to elementwise multiplication.
Value mtimes(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a ./ b — elementwise right division with broadcasting.
Value rdivide(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a / b — matrix right division (currently only scalar denominator and
/// scalar/scalar; matrix right division not implemented).
Value mrdivide(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a \ b — matrix left division (currently scalar/scalar only).
Value mldivide(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a ^ b — matrix/scalar power (scalar/scalar only; matrix power NYI).
Value power(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a .^ b — elementwise power with broadcasting.
Value elementPower(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Comparisons (return logical) ─────────────────────────────────────
Value eq(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value ne(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value lt(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value gt(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value le(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value ge(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Logical (elementwise) ────────────────────────────────────────────
/// a & b — elementwise logical AND (non-zero-to-bool coercion).
Value logicalAnd(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// a | b — elementwise logical OR.
Value logicalOr(std::pmr::memory_resource *mr, const Value &a, const Value &b);

} // namespace numkit::builtin
