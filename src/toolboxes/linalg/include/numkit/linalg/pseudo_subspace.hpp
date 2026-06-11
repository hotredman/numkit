// toolboxes/linalg/include/numkit/linalg/pseudo_subspace.hpp
//
// SVD-based pseudoinverse and subspace queries.
// Migrated together with the SVD kernel from toolboxes/builtin/src/
// language/arrays/matrix.cpp (group 4 of the toolboxes/linalg extraction).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Moore-Penrose pseudoinverse (`P = pinv(A, tol)`).
///
/// Via SVD: `pinv(A) = V · S⁺ · U'` where `S⁺` inverts non-zero
/// singular values above `tol`.
Value pinv(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Orthonormal basis for range(A) (`Q = orth(A, tol)`).
///
/// Columns of `U` from SVD whose corresponding sigma exceeds `tol`.
Value orth(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Orthonormal basis for null(A) (`N = null(A, tol)`).
///
/// Columns of `V` from SVD whose corresponding sigma is below `tol`.
Value null_basis(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Subspace angle (`theta = subspace(A, B)`).
///
/// `theta = acos(min(svd(orth(A)' · orth(B))))`. Returns radians in
/// `[0, π/2]`.
Value subspace(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
