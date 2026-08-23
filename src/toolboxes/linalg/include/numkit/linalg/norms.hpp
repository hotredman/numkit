/// @file norms.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/numkit/linalg/norms.hpp
//
// Vector and matrix norms. Migrated from toolboxes/builtin/src/language/
// arrays/{matrix,predicates}.cpp.
//
// `norm` for the matrix 2-norm (p == 2) routes through `svd_values`,
// which still lives in toolboxes/builtin/include/numkit/builtin/language/
// arrays/matrix.hpp at this point — that header is included by the
// implementation TU (norms.cpp). When SVD migrates here in a later
// group, this include will switch over to numkit/linalg/decomp/svd.hpp.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Vector / matrix p-norm (`n = norm(x, p)`).
///
/// Vector input: `(Σ |x|^p)^(1/p)`; `p = 1` → sum of abs,
/// `p = 2` → Euclidean. Matrix input: `p = 1` → max column sum,
/// `p = 2` → largest singular value (via SVD).
///
/// @param x   Input array (vector or 2-D matrix).
/// @param p   Norm order (positive real).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar norm.
/// @throws Error  Unsupported matrix p-norm.
/// @see norm_inf, norm_fro, vecnorm
Value norm_value(const Value &x, double p, std::pmr::memory_resource *mr = nullptr);

/// @brief Inf-norm (`n = norm(x, inf)`).
///
/// Vector: `max(|v|)`. Matrix: max row sum.
Value norm_inf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Frobenius norm (`n = norm(A, 'fro')`).
///
/// `sqrt(sum(A.^2))`.
Value norm_fro(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Vector p-norm along a dim (`y = vecnorm(A, p, dim)`).
///
/// Defaults: `p = 2`, `dim = 0` (first non-singleton). `p = Inf` →
/// `max(|A|)`, `p = -Inf` → `min(|A|)`.
///
/// @param A    Input array.
/// @param p    Norm order (default 2).
/// @param dim  1-based dimension (0 → first non-singleton).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Norms reduced along `dim`.
Value vecnorm(const Value &A, double p = 2.0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
