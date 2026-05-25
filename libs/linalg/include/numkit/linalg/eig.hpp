// libs/linalg/include/numkit/linalg/eig.hpp
//
// Eigenvalue family — symmetric & general eig, Hessenberg reduction,
// symmetric Schur, characteristic polynomial, Sylvester equation.
// Migrated from libs/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Symmetric eigendecomposition (`[V, D] = eig(A)`).
///
/// Classical Jacobi rotations. Returns `(V, D)` such that `A·V == V·D`,
/// `V` orthogonal, `D` diagonal (ascending real eigenvalues). Throws if
/// `A` is not symmetric within tolerance.
std::tuple<Value, Value>
eig_symmetric(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Eigenvalues only (`e = eig(A)` single-output form, symmetric).
Value eig_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Characteristic polynomial of a matrix (`p = poly(A)`).
///
/// Souriau-Faddeev-LeVerrier. `roots(p) == eig(A)`.
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief General (non-symmetric) eigenvalues via characteristic polynomial.
///
/// Less numerically stable than QR iteration but works for moderate `n`.
Value eig_general_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief General `[V, D]` eig for real-eigenvalue asymmetric `A`.
///
/// Throws if any eigenvalue has non-zero imaginary part (Francis QR
/// for complex eigvec extraction is deferred).
std::tuple<Value, Value>
eig_general_VD(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Hessenberg reduction (`[P, H] = hess(A)`).
///
/// `A = P·H·P'`, `P` orthogonal, `H` upper-Hessenberg.
std::tuple<Value, Value>
hess(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Hessenberg-only output (`H = hess(A)` single-output form).
Value hess_H_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Schur decomposition for symmetric A (`[U, T] = schur(A)`).
///
/// Equivalent to eigendecomposition: `A = U·T·U'`, `T` diagonal.
/// General Schur (quasi-triangular T) is deferred.
std::tuple<Value, Value>
schur_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Sylvester equation for symmetric A and B (`X = sylvester_sym(A, B, C)`).
///
/// Solves `A·X + X·B = C` via simultaneous diagonalisation.
Value sylvester_sym(const Value &A, const Value &B, const Value &C,
                    std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
