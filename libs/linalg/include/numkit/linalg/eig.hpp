// libs/linalg/include/numkit/linalg/eig.hpp
//
// Eigenvalue family — symmetric & general eig, Hessenberg reduction,
// symmetric Schur, characteristic polynomial, Sylvester equation.
// Migrated from libs/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

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

/// @brief Polynomial eigenvalue problem
/// (`e = polyeig(A0, A1, ..., Ak)`, possibly `[V, e]`).
///
/// Finds scalars `λ` and non-zero vectors `x` such that
/// `(A0 + λ·A1 + λ²·A2 + … + λ^k·Ak) · x = 0`.
///
/// Implementation: linearisation via the standard companion matrix
/// of size `k·n × k·n`; eigenvalues of the companion are the
/// polynomial eigenvalues. Right eigenvectors of the companion
/// have a Kronecker structure `[x; λ x; λ² x; …]`; the first `n`
/// rows give `x`.
///
/// Eigenvalue-only form returns a column of `k·n` values; the matrix
/// form `[V, e] = polyeig(...)` returns the eigenvectors in an
/// `n × k·n` matrix. The matrix form requires real eigenvalues
/// (Francis QR for complex pairs is deferred).
///
/// @param coeffs   Span of `k+1` polynomial coefficient matrices,
///                 ordered constant first: `[A0, A1, ..., Ak]`.
///                 All `n × n`, same size. `Ak` (leading) must be
///                 non-singular.
/// @param mr       Memory resource.
/// @return         Eigenvalue column of length `k·n` (possibly COMPLEX).
Value polyeig_values(Span<const Value> coeffs, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial eigenvalue problem, eigenvectors + eigenvalues
/// (`[V, e] = polyeig(A0, ..., Ak)`).
///
/// Throws if any eigenvalue is complex (needs Francis QR).
std::tuple<Value, Value>
polyeig_VE(Span<const Value> coeffs, std::pmr::memory_resource *mr = nullptr);

/// @brief Eigenvalues from a (quasi-)triangular Schur factor
/// (`e = ordeig(T)`).
///
/// Returns eigenvalues in the order they appear in `T`. For a true
/// triangular `T` (real eigenvalues only — e.g. diagonal symmetric
/// Schur factor) returns `diag(T)`. For a real quasi-triangular
/// Schur factor, 2×2 sub-diagonal blocks emit conjugate-pair complex
/// eigenvalues from `(a ± √(c·d))` where the block is `[[a c]; [d a]]`.
///
/// Unlike `eig(T)`, `ordeig` does NOT sort; the result preserves the
/// diagonal/block ordering of `T` exactly — useful for reading off
/// eigenvalues from a manually-reordered Schur factor.
Value ordeig(const Value &T, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
