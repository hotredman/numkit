// libs/builtin/include/numkit/builtin/language/operators/binary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Arithmetic ───────────────────────────────────────────────────────
/// a + b. Numeric addition with broadcasting; string concatenation for
/// char/string operands; mixed char+double promotes char to double.
Value plus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a - b. Numeric subtraction with broadcasting.
Value minus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a .* b — elementwise multiplication with broadcasting.
Value times(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a * b — matrix multiplication (MxK * KxN → MxN). Scalars broadcast
/// to elementwise multiplication.
Value mtimes(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a ./ b — elementwise right division with broadcasting.
Value rdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a / b — matrix right division (currently only scalar denominator and
/// scalar/scalar; matrix right division not implemented).
Value mrdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a \ b — matrix left division (currently scalar/scalar only).
Value mldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a ^ b — matrix/scalar power (scalar/scalar only; matrix power NYI).
Value power(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a .^ b — elementwise power with broadcasting.
Value elementPower(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Comparisons (return logical) ─────────────────────────────────────
Value eq(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value le(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Logical (elementwise) ────────────────────────────────────────────
/// a & b — elementwise logical AND (non-zero-to-bool coercion).
Value logicalAnd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// a | b — elementwise logical OR.
Value logicalOr(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
