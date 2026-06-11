// toolboxes/linalg/include/numkit/linalg/page_ops.hpp
//
// Page-wise linalg ops on 3-D arrays. Wrappers that iterate over
// pages and dispatch to the 2-D variant. The general page operators
// (pagetranspose, pagectranspose, pagemtimes) stay in toolboxes/builtin —
// they're basic tensor ops, not strictly linalg.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Page-wise inverse of a 3-D array (`B = pageinv(A)`).
///
/// Each `m × n` page is independently inverted via LU.
Value pageinv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise eigenvalues (`e = pageeig(A)`).
///
/// For each page, returns a column of eigenvalues. Output is
/// `n × 1 × pages`. Only real eigenvalues supported (matches
/// the 2-D eig behaviour).
Value pageeig_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise eigendecomposition (`[V, D] = pageeig(A)`).
///
/// Returns `(V_pages, D_pages)`, each shaped `n × n × pages`.
std::tuple<Value, Value>
pageeig_VD(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise singular values (`s = pagesvd(A)`).
Value pagesvd_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise full SVD (`[U, S, V] = pagesvd(A)`).
std::tuple<Value, Value, Value>
pagesvd_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise pseudo-inverse (`B = pagepinv(A[, tol])`).
Value pagepinv(const Value &A, double tol = -1.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise norm (`y = pagenorm(A, p)`).
///
/// `p` may be a positive real (2-norm by default), `Inf`, or one of
/// `"fro"` / `"inf"`. Output is `1 × 1 × pages`.
Value pagenorm(const Value &A, double p, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise mldivide (`X = pagemldivide(A, B)`).
///
/// For each page solves `A_p · X_p = B_p`. Both inputs must have
/// matching page counts, or one of them may be 2-D (broadcast).
Value pagemldivide(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise mrdivide (`X = pagemrdivide(A, B)`).
///
/// For each page solves `X_p · B_p = A_p` (right division).
Value pagemrdivide(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise minimum-norm least-squares (`X = pagelsqminnorm(A, B[, tol])`).
Value pagelsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user,
                     std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
