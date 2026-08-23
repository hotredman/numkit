// include/numkit/builtin/ops.hpp
//
// Arithmetic, relational, logical operators and array transformations (MATLAB parity).
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @addtogroup group_matlab
/// @{


/// @file
/// @ingroup group_matlab
/// @brief Fundamental arithmetic, relational, logical, and array operator functions.
///
/// Provides an engine-free, highly-optimized C++ interface for standard MATLAB operators,
/// supporting elementwise broadcasting, type promotions, and SIMD acceleration.

// ── Arithmetic Operations ───────────────────────────────────────────────────

/// @brief Elementwise addition (`y = a + b`).
///
/// Computes numeric addition with automatic singleton expansion (broadcasting).
/// For string and character arrays, performs concatenation or character promotion.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Sum array with broadcast shape.
/// @see minus, uplus, times
Value plus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise subtraction (`y = a - b`).
///
/// Computes numeric subtraction with automatic singleton expansion (broadcasting).
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Difference array with broadcast shape.
/// @see plus, uminus
Value minus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise multiplication (`y = a .* b`).
///
/// Multiplies arrays element-by-element with singleton expansion broadcasting.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Elementwise product array with broadcast shape.
/// @see mtimes, rdivide
Value times(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix multiplication (`Y = A * B`).
///
/// Performs standard 2-D matrix multiplication (`M x K * K x N -> M x N`).
/// If either operand is a scalar, automatically falls back to elementwise multiplication.
///
/// @param a Left matrix (`M x K`) or scalar.
/// @param b Right matrix (`K x N`) or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Matrix product (`M x N`).
/// @throws std::runtime_error If inner matrix dimensions do not agree.
/// @see times, pagemtimes, mrdivide, mldivide
Value mtimes(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise right division (`y = a ./ b`).
///
/// Divides each element of `a` by the corresponding element of `b` with singleton expansion.
///
/// @param a Numerator array or scalar.
/// @param b Denominator array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Elementwise quotient array with broadcast shape.
/// @see ldivide, mrdivide
Value rdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise left division (`y = a .\\ b` == `b ./ a`).
///
/// Divides each element of `b` by the corresponding element of `a` with singleton expansion.
///
/// @param a Denominator array or scalar.
/// @param b Numerator array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Elementwise quotient array (`b ./ a`) with broadcast shape.
/// @see rdivide, mldivide
Value ldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix right division / slash (`y = A / B`).
///
/// Solves the linear system `Y * B = A` (or for scalar `B`, equivalent to `A ./ B`).
///
/// @param a Numerator matrix or scalar.
/// @param b Denominator matrix or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Solution matrix or quotient.
/// @see mldivide, rdivide
Value mrdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix left division / backslash (`y = A \\ B`).
///
/// Solves the linear system `A * Y = B` (or for scalar `A`, equivalent to `B ./ A`).
///
/// @param a Coefficient matrix `A` or scalar.
/// @param b Right-hand side matrix `B` or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Solution vector or matrix.
/// @see mrdivide, ldivide
Value mldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise power (`y = a .^ b`).
///
/// Computes `a` raised to the power `b` element-by-element with singleton expansion.
/// Yields complex output when negative reals are raised to non-integer exponents.
///
/// @param a Base array or scalar.
/// @param b Exponent array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Elementwise power array with broadcast shape.
/// @see mpower
Value power(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise power alias (`y = a .^ b`).
Value elementPower(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix power (`Y = A ^ B`).
///
/// Computes square matrix `A` raised to the scalar power `B`.
///
/// @param a Square base matrix or scalar.
/// @param b Exponent scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Matrix power result.
/// @see power
Value mpower(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Unary plus (`y = +a`).
///
/// Returns identity copy of array `a`.
///
/// @param a Input array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Copy of input array `a`.
/// @see uminus, plus
Value uplus(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Unary minus / negation (`y = -a`).
///
/// Negates each element in array `a`. For signed integers, saturates at minimum value.
/// For logicals and characters, converts to double before negation.
///
/// @param a Input array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Elementwise negated array.
/// @see uplus, minus
Value uminus(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Transposition operator for page-wise matrix multiplication.
enum class TranspOp {
    None,       ///< No transposition
    Transpose,  ///< Non-conjugate transpose (.')
    CTranspose  ///< Complex conjugate transpose (')
};

/// @brief Page-wise matrix multiplication (`pagemtimes(a, b)`).
///
/// Multiplies slices (pages) of N-D arrays along the first two dimensions with broadcasting.
///
/// @param a Left N-D matrix operand.
/// @param b Right N-D matrix operand.
/// @param mr Memory resource for allocations.
/// @return Resulting page-wise multiplied array.
/// @see mtimes
Value pagemtimes(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise matrix multiplication with transpositions (`pagemtimes(x, tx, y, ty)`).
/// @param x Left operand.
/// @param tx Transposition operator for left operand (`None`, `Transpose`, `CTranspose`).
/// @param y Right operand.
/// @param ty Transposition operator for right operand (`None`, `Transpose`, `CTranspose`).
/// @param mr Memory resource.
/// @return Resulting page-wise multiplied array.
Value pagemtimes(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr = nullptr);

// ── Relational & Comparison Operations ──────────────────────────────────────

/// @brief Elementwise equality (`y = a == b`).
///
/// Compares corresponding elements for equality with automatic singleton expansion.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see ne, lt, le, gt, ge
Value eq(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inequality (`y = a ~= b`).
///
/// Compares corresponding elements for inequality with automatic singleton expansion.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see eq, lt, le, gt, ge
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise less than (`y = a < b`).
///
/// Compares whether elements of `a` are strictly less than elements of `b`.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see le, gt, ge, eq
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise less than or equal (`y = a <= b`).
///
/// Compares whether elements of `a` are less than or equal to elements of `b`.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see lt, ge, gt, eq
Value le(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise greater than (`y = a > b`).
///
/// Compares whether elements of `a` are strictly greater than elements of `b`.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see ge, lt, le, eq
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise greater than or equal (`y = a >= b`).
///
/// Compares whether elements of `a` are greater than or equal to elements of `b`.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see gt, le, lt, eq
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Logical Operations & Predicates ─────────────────────────────────────────

/// @brief Elementwise logical AND (`y = a & b`).
///
/// Converts non-zero values to `true` and performs elementwise AND with singleton expansion.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see logical_or, logical_not, and_op
Value logical_and(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for @ref logical_and (`y = a & b`).
inline Value and_op(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr)
{
    return logical_and(a, b, mr);
}

/// @brief Elementwise logical OR (`y = a | b`).
///
/// Converts non-zero values to `true` and performs elementwise OR with singleton expansion.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see logical_and, logical_not, or_op
Value logical_or(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for @ref logical_or (`y = a | b`).
inline Value or_op(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr)
{
    return logical_or(a, b, mr);
}

/// @brief Elementwise logical NOT (`y = ~a`).
///
/// Converts zeros to `true` and non-zeros to `false`.
///
/// @param a Input array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with same shape as `a`.
/// @see logical_and, logical_or, not_op
Value logical_not(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for @ref logical_not (`y = ~a`).
inline Value not_op(const Value &a, std::pmr::memory_resource *mr = nullptr)
{
    return logical_not(a, mr);
}

/// @brief Elementwise logical exclusive OR (`y = xor(a, b)`).
///
/// Returns `true` where exactly one of `a` or `b` is non-zero.
///
/// @param a Left operand array or scalar.
/// @param b Right operand array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL array with broadcast shape.
/// @see logical_and, logical_or, xor_op
Value logical_xor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for @ref logical_xor (`y = xor(a, b)`).
inline Value xor_op(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr)
{
    return logical_xor(a, b, mr);
}

/// @brief True if any element of array is non-zero (`y = any(x, dim)`).
///
/// Tests whether any elements along the specified dimension are non-zero.
///
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton dimension).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL reduced array.
/// @see all, logical_or
Value any(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief True if all elements of array are non-zero (`y = all(x, dim)`).
///
/// Tests whether all elements along the specified dimension are non-zero.
///
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton dimension).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL reduced array.
/// @see any, logical_and
Value all(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Transposition ──────────────────────────────────────────────────────────

/// @brief Complex conjugate transpose (`y = a'`).
///
/// Transposes 2-D matrix `a` and conjugates complex elements.
///
/// @param a Input 2-D matrix.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Conjugate transpose of `a`.
/// @throws std::runtime_error If `a` is N-D with >2 dimensions.
/// @see transpose
Value ctranspose(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Non-conjugate array transpose (`y = a.'`).
///
/// Transposes 2-D matrix `a` without conjugating complex elements.
///
/// @param a Input 2-D matrix.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Transpose of `a`.
/// @throws std::runtime_error If `a` is N-D with >2 dimensions.
Value transpose(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for @ref logical_and.
inline Value logicalAnd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr) { return logical_and(a, b, mr); }

/// @brief Alias for @ref logical_or.
inline Value logicalOr(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr) { return logical_or(a, b, mr); }

/// @brief Alias for @ref logical_not.
inline Value logicalNot(const Value &a, std::pmr::memory_resource *mr = nullptr) { return logical_not(a, mr); }

/// @brief Alias for @ref logical_xor.
inline Value logicalXor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr) { return logical_xor(a, b, mr); }

inline Value transposeNC(const Value &a, std::pmr::memory_resource *mr = nullptr) { return transpose(a, mr); }

/// @brief Bitwise AND of integer arrays (`bitand(a, b)`).
/// @param a First integer operand.
/// @param b Second integer operand.
/// @param mr Memory resource.
/// @return Bitwise AND result.
Value bitand_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise OR of integer arrays (`bitor(a, b)`).
/// @param a First integer operand.
/// @param b Second integer operand.
/// @param mr Memory resource.
/// @return Bitwise OR result.
Value bitor_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise XOR of integer arrays (`bitxor(a, b)`).
/// @param a First integer operand.
/// @param b Second integer operand.
/// @param mr Memory resource.
/// @return Bitwise XOR result.
Value bitxor_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise shift of integer values (`bitshift(a, k)`).
/// @param a Input integer array.
/// @param k Shift amount (positive for left shift, negative for right shift).
/// @param mr Memory resource.
/// @return Bitwise shifted values.
Value bitshift(const Value &a, const Value &k, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise complement (`bitcmp(a, width)`).
/// @param a Input integer array.
/// @param width Number of bits in unsigned bit representation (default: 64).
/// @param mr Memory resource.
/// @return Bitwise inverted values.
Value bitcmp(const Value &a, int width = 64, std::pmr::memory_resource *mr = nullptr);

/// @brief Sets specified bit in integer array (`bitset(a, n, val)`).
/// @param a Input integer array.
/// @param n 1-based bit position index.
/// @param val Bit value (0 or 1, default 1).
/// @param mr Memory resource.
/// @return Modified integer array.
Value bitset(const Value &a, const Value &n, const Value &val = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Gets bit value at specified bit position in integer array (`bitget(a, n)`).
/// @param a Input integer array.
/// @param n 1-based bit position index.
/// @param mr Memory resource.
/// @return Bit values (0 or 1).
Value bitget(const Value &a, const Value &n, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::builtin
