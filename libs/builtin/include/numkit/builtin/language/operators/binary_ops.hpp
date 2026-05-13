// libs/builtin/include/numkit/builtin/language/operators/binary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// @file
/// @brief Binary operators (`+`, `-`, `.*`, `*`, `./`, `/`, `\`, `^`,
/// `.^`, `==`, `~=`, `<`, `>`, `<=`, `>=`, `&`, `|`).
///
/// All operators broadcast elementwise unless noted otherwise.

/// @brief Addition (`y = a + b`).
///
/// Numeric addition with broadcasting; string concatenation for
/// CHAR / STRING operands; mixed CHAR + DOUBLE promotes CHAR to DOUBLE.
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Sum, broadcast shape.
/// @see minus
Value plus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Subtraction (`y = a - b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Difference, broadcast shape.
/// @see plus
Value minus(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise multiplication (`y = a .* b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise product, broadcast shape.
/// @see mtimes
Value times(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix multiplication (`y = a * b`).
///
/// `M × K * K × N → M × N`. Scalars broadcast to elementwise.
///
/// @param a   Left matrix.
/// @param b   Right matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Matrix product.
/// @throws Error  Inner-dim mismatch (`m:mtimes:innerDim`).
/// @see times
Value mtimes(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise right division (`y = a ./ b`).
///
/// @param a   Numerator.
/// @param b   Denominator.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise quotient, broadcast shape.
/// @see mrdivide
Value rdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix right division (`y = a / b`).
///
/// Currently only scalar denominator and scalar/scalar; full matrix
/// right division is not implemented.
///
/// @param a   Numerator.
/// @param b   Denominator.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quotient.
/// @see rdivide, mldivide
Value mrdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix left division (`y = a \ b`).
///
/// Currently scalar/scalar only; matrix left division is not
/// implemented.
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Left-divided result.
/// @see mrdivide
Value mldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Power (`y = a ^ b`).
///
/// Matrix / scalar power — scalar/scalar only in this revision;
/// matrix power is not yet implemented.
///
/// @param a   Base.
/// @param b   Exponent.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Power.
/// @see elementPower
Value power(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise power (`y = a .^ b`).
///
/// Broadcasts elementwise. Negative base with non-integer exponent
/// yields COMPLEX.
///
/// @param a   Base array.
/// @param b   Exponent array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise power, broadcast shape.
/// @see power
Value elementPower(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise equality (`y = a == b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see ne
Value eq(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inequality (`y = a ~= b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see eq
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Less than (`y = a < b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see gt, le, ge
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Greater than (`y = a > b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see lt, ge, le
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Less than or equal (`y = a <= b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see lt, ge
Value le(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Greater than or equal (`y = a >= b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see gt, le
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Logical AND (`y = a & b`).
///
/// Elementwise; non-zero entries coerce to `true`.
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see logicalOr
Value logicalAnd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Logical OR (`y = a | b`).
///
/// @param a   Left operand.
/// @param b   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see logicalAnd
Value logicalOr(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
