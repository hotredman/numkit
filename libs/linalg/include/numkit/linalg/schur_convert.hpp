// libs/linalg/include/numkit/linalg/schur_convert.hpp
//
// Schur form conversions: complex-diagonal ↔ real-block-diagonal.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Complex-diagonal → real-block-diagonal Schur form
/// (`[VR, DR] = cdf2rdf(V, D)`).
///
/// Given complex `V` and complex-diagonal `D` from `eig(A)` for a
/// real `A`, returns real `VR`, `DR` such that `A == VR · DR · inv(VR)`.
/// Conjugate eigenvalue pairs `a ± b·i` become real 2×2 blocks
/// `[a -b; b a]` on the diagonal of `DR`; the corresponding pair of
/// complex-conjugate eigenvector columns of `V` become real columns
/// `Re(v)`, `Im(v)` in `VR`.
///
/// Real eigenvalues pass through unchanged.
std::tuple<Value, Value>
cdf2rdf(const Value &V, const Value &D, std::pmr::memory_resource *mr = nullptr);

/// @brief Real Schur → complex Schur form (`[U, T] = rsf2csf(UR, TR)`).
///
/// Given a real quasi-triangular Schur factor `TR` (real diagonal +
/// 2×2 blocks for complex eigenvalue pairs) and the corresponding
/// real orthogonal `UR`, returns complex `U`, upper-triangular `T`
/// such that `A == U · T · U'`.
///
/// Each 2×2 diagonal block of `TR` is diagonalised via a unitary
/// rotation that brings its complex-conjugate eigenvalues onto the
/// diagonal of `T`; the corresponding two columns of `U` get the
/// same rotation applied.
std::tuple<Value, Value>
rsf2csf(const Value &UR, const Value &TR, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
