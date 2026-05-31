// libs/linalg/include/numkit/linalg/vector_ops.hpp
//
// Vector-algebra operations: cross product, dot product, Kronecker
// product. Migrated from libs/builtin/src/language/arrays/matrix.cpp
// (see commit history). These were always linalg-domain; they lived in
// libs/builtin only because libs/linalg did not exist yet.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Cross product of two 3-vectors or `3 × N` / `N × 3` batches.
///
/// Operates along the first dimension of length 3. Result has the same
/// shape as the inputs.
///
/// @param a   First operand (vector or 3-row/3-col batch).
/// @param b   Second operand, same shape as `a`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cross product.
/// @throws Error  Shape mismatch, or no dimension of length 3.
/// @see dot
Value cross(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Dot product of two equal-length vectors.
///
/// @param a   First vector.
/// @param b   Second vector (same `numel` as `a`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar dot product.
/// @throws Error  Length mismatch.
/// @see cross
Value dot(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Dot product along a given dimension (`dot(A, B, dim)`).
///
/// Computes `sum(conj(A) .* B, dim)` — the dot product of corresponding
/// vectors along dimension `dim`. `dim = 1` reduces down columns (result is
/// `1 × W`), `dim = 2` reduces across rows (result is `H × 1`). `dim = 0`
/// selects the default behavior (vector → scalar, matrix → per-column).
/// 2-D only.
///
/// @param a   First operand.
/// @param b   Second operand, same shape as `a`.
/// @param dim Reduction dimension (0 = auto, 1 = columns, 2 = rows).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Dot product reduced along `dim`.
/// @throws Error  Shape mismatch, 3-D input, or invalid `dim`.
/// @see dot
Value dot(const Value &a, const Value &b, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Kronecker product (`K = kron(A, B)`).
///
/// Output is `(rA·rB) × (cA·cB)`; the `(i, j)`-th block (`rB × cB`)
/// equals `A(i, j) · B`. DOUBLE only — COMPLEX inputs throw.
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Kronecker product.
/// @throws Error  COMPLEX input, or rank > 2.
Value kron(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
