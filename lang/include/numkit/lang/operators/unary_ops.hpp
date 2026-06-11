// toolboxes/builtin/include/numkit/builtin/language/operators/unary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::lang {

/// @brief Unary minus (`y = -x`).
///
/// Type-preserving: CHAR / LOGICAL promote to DOUBLE; signed integer
/// types saturate at type-min (i.e. `-INT_MIN` stays at `INT_MIN`);
/// unsigned types map every value to 0.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Negated array, same shape as `x`.
/// @see uplus
Value uminus(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unary plus (`y = +x`).
///
/// Identity — returns `x` unchanged. Provided for operator symmetry
/// (so `+x` parses to a function call like `-x`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `x` unchanged.
/// @see uminus
Value uplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise logical NOT (`y = ~x`).
///
/// Non-zero → 0, zero → 1. Output is always LOGICAL.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, same shape as `x`.
Value logicalNot(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Conjugate transpose (`y = x'`).
///
/// For COMPLEX matrices, conjugates each element while transposing.
/// 2-D only; 3-D throws. Supports DOUBLE and COMPLEX.
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Conjugate transpose.
/// @throws Error  3-D input (`m:ctranspose:3D`).
/// @see transposeNC
Value ctranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Non-conjugate transpose (`y = x.'`).
///
/// For COMPLEX matrices, does NOT conjugate. 2-D only. Distinct from
/// the `transpose()` builtin (in matrix.cpp), which is DOUBLE-only;
/// this overload supports DOUBLE and COMPLEX.
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Non-conjugate transpose.
/// @throws Error  3-D input (`m:transposeNC:3D`).
/// @see ctranspose
Value transposeNC(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::lang
