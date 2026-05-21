// libs/builtin/include/numkit/builtin/language/arrays/matrix.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {

/// @file
/// @brief Matrix builtins — constructors, linear algebra, shape, sort.
///
/// **Scope.** This header is the linalg surface of `libs/builtin`: matrix
/// constructors (`zeros`, `ones`, `eye`, `magic`, …), classical
/// factorisations (`lu`, `qr`, `svd`, `chol`, `eig`, `schur`, `hess`),
/// the matrix functions (`expm`, `logm_sym`, `sqrtm_sym`), solvers
/// (`linsolve`, `pinv`, `lsqminnorm`, `mldivide` via `linsolve`),
/// and the surrounding ecosystem (shape, sort, reductions, concatenation).
///
/// **Conventions.** Functions taking a single matrix conventionally
/// accept it as `const Value &A`. `mr = nullptr` selects the process
/// default memory resource. Multi-output results are returned as
/// `std::tuple`, `std::pair`, or named structs documented inline.

// ── Constructors: zero / one / identity / shaped ─────────────────────

/// @brief All-zero array (`y = zeros(rows, cols, pages)`).
///
/// @param rows   Row count. @param cols  Column count (default 1).
/// @param pages  Page count; 0 → 2-D output.
/// @param mr     Memory resource (nullptr → process default).
/// @return       DOUBLE array of the requested shape. @see ones, eye
Value zeros(size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief All-one array (`y = ones(rows, cols, pages)`).
/// @param rows   Row count. @param cols  Column count (default 1).
/// @param pages  Page count; 0 → 2-D output.
/// @param mr     Memory resource (nullptr → process default).
/// @return       DOUBLE array. @see zeros
Value ones(size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Identity-like matrix (`I = eye(rows, cols)`).
///
/// Rectangular when `rows != cols` — 1 on the leading diagonal, 0 elsewhere.
///
/// @param rows  Row count. @param cols  Column count.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` array. @see zeros, diag
Value eye(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);

/// @brief Magic square (`M = magic(N)`).
///
/// `N × N` matrix where rows, columns, both diagonals sum to the magic
/// constant `N · (N² + 1) / 2`. Three branches by `N`'s parity (Siamese
/// for odd N, doubly-even, Strachey for singly-even).
///
/// Edge cases: `N == 0 → 0×0`, `N == 1 → [1]`, `N == 2 → [1 3; 4 2]`
/// (preserved for parity; not strictly magic).
///
/// @param N   Order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N × N` DOUBLE matrix.
Value magic(size_t N, std::pmr::memory_resource *mr = nullptr);

/// @brief Toeplitz matrix (`T = toeplitz(c, r)`).
///
/// `T(i, j) = c(i - j)` for `i >= j`, else `r(j - i)`.
/// Single-arg form (`r == Value::Empty`) takes `r = c` (real input).
/// If `c(0) != r(0)`, `r(0)` is silently overridden.
///
/// @param c   First column.
/// @param r   First row (default `Value::Empty` → `r = c`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `numel(c) × numel(r)` matrix. @see hankel
Value toeplitz(const Value &c, const Value &r = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Hankel matrix (`H = hankel(c, r)`).
///
/// `H(i, j) = c(i + j)` for `i + j < numel(c)`, else `r(i + j - numel(c) + 1)`.
/// Single-arg form: `r` is all zeros (anti-triangular Hankel).
///
/// @param c   First column.
/// @param r   Last row (default `Value::Empty` → zeros).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `numel(c) × numel(r)` matrix. @see toeplitz
Value hankel(const Value &c, const Value &r = Value::Empty,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Vandermonde matrix (`V = vander(v)`).
///
/// `V(i, j) = v(i)^(n - 1 - j)`. Columns ordered from highest to lowest
/// power.
///
/// @param v   Generator vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` Vandermonde matrix.
Value vander(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Companion matrix of a monic polynomial (`C = compan(p)`).
///
/// `C(0, :) = -p(2:end) / p(1)`, subdiagonal = ones. Eigenvalues equal
/// the roots of `p`.
///
/// @param p   Polynomial coefficients (length `n + 1`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` companion matrix.
Value compan(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Pascal matrix (`P = pascal(n)`).
///
/// Symmetric form `P(i, j) = C(i + j, i)`. Only `k = 0` (symmetric) is
/// implemented; the `k = 1, 2` forms are deferred.
///
/// @param n   Order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` Pascal matrix.
Value pascal(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Hilbert matrix (`H = hilb(n)`).
///
/// `H(i, j) = 1 / (i + j - 1)` (1-indexed).
///
/// @param n   Order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` Hilbert matrix.
/// @see invhilb
Value hilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Exact inverse Hilbert matrix (`H = invhilb(n)`).
///
/// Closed-form using binomial coefficients. Exact integer entries up
/// to `n ≈ 13`; beyond, accuracy degrades due to FP overflow.
///
/// @param n   Order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` matrix.
/// @see hilb
Value invhilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Wilkinson's eigenvalue test matrix (`W = wilkinson(n)`).
///
/// Symmetric tridiagonal with subdiagonal of ones and main diagonal
/// `|i - (n+1)/2|` (1-indexed).
///
/// @param n   Order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` matrix.
Value wilkinson(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Hadamard matrix via Sylvester construction (`H = hadamard(n)`).
///
/// Requires `n` to be a power of 2. The Paley constructions
/// (`12·2^k`, `20·2^k`) are deferred.
///
/// @param n   Order (power of 2).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` `±1` matrix.
/// @throws Error  `n` not a power of 2 (`m:hadamard:badN`).
Value hadamard(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Rosser's 8×8 eigenvalue test matrix (`R = rosser()`).
///
/// The 8×8 entries are fixed, hardcoded constants.
///
/// @param mr  Memory resource (nullptr → process default).
/// @return    `8 × 8` matrix.
Value rosser(std::pmr::memory_resource *mr = nullptr);

// ── Solvers and inverses ─────────────────────────────────────────────

/// @brief Matrix inverse via LU (`B = inv(A)`).
///
/// Prefer @ref linsolve / `\` for solving `A·x = b`; this function
/// exists when the inverse itself is needed as a matrix.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `A^{-1}`.
/// @throws Error  Non-square or singular (`m:inv:singular`).
/// @see linsolve, pinv
Value inv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Solve `A·X = B` (`X = linsolve(A, B)`).
///
/// LU for square `A`, Householder QR for tall `A` (least-squares).
/// Backs `mldivide` / `\`. The optional `opts` arg is
/// accepted for compatibility but ignored — LU/QR auto-detection
/// handles the same cases.
///
/// @param A   System matrix.
/// @param B   RHS vector or matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Solution `X`.
/// @throws Error  Singular / rank-deficient / wide `A`.
/// @see inv, pinv, lsqminnorm
Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise inverse of a 3-D array (`B = pageinv(A)`).
///
/// Each `m × n` page is independently inverted via LU.
///
/// @param A   3-D array with square pages.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape array of page inverses.
/// @throws Error  Non-square or singular page.
Value pageinv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Trace = sum of diagonal (`t = trace(A)`).
///
/// `sum(diag(A))`. Works for any 2-D matrix (square or rectangular).
///
/// @param A   2-D matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar trace.
Value trace(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Determinant via LU with partial pivoting (`d = det(A)`).
///
/// `det(A) = sign(P) · prod(diag(U))` where `A = P·L·U`.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar determinant.
/// @throws Error  Non-square (`m:det:notSquare`).
Value det(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Cholesky factorisation (`R = chol(A)`).
///
/// Returns the upper-triangular `R` such that `R' · R == A`.
///
/// @param A   Symmetric positive-definite matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upper-triangular factor `R`.
/// @throws Error  Non-square or not positive-definite (`m:chol:notPosDef`).
/// @see ldl
Value chol(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Top-`k` rows sorted descending (`T = topkrows(A, k)`).
///
/// Lex-descending across all columns. Single-arg form requires
/// `size(A, 1) >= k`.
///
/// @param A   Input matrix.
/// @param k   Number of rows to return.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `k × size(A, 2)` matrix of top rows.
/// @see sortrows, maxk
Value topkrows(const Value &A, std::size_t k, std::pmr::memory_resource *mr = nullptr);

// ── LU / QR / SVD ────────────────────────────────────────────────────

/// @brief LU with partial pivoting (`[L, U, P] = lu_decompose(A)`).
///
/// `P · A == L · U` where `L` is unit-lower-triangular, `U` is
/// upper-triangular, `P` is a permutation matrix. The single-output
/// `LU = lu(A)` form lives in @ref lu_combined.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(L, U, P)` triple.
/// @throws Error  Non-square (`m:lu:notSquare`).
/// @see lu_combined, linsolve
std::tuple<Value, Value, Value>
lu_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Combined L+U output (`LU = lu(A)` single-output form).
///
/// Strict lower triangle is `L` (unit diagonal implicit); upper
/// triangle (including diagonal) is `U`. Rows are already permuted.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Combined L+U matrix.
/// @see lu_decompose
Value lu_combined(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief QR via Householder reflections (`[Q, R] = qr_decompose(A)`).
///
/// Full-size form (not `"econ"`). `A == Q · R`. `Q` is `m × m`
/// orthogonal, `R` is `m × n` upper-triangular.
///
/// @param A   `m × n` matrix with `m >= n`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(Q, R)` pair.
/// @throws Error  Wide matrix (`m:qr:wide`).
/// @see qr_R_only
std::tuple<Value, Value>
qr_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief R-only QR output (`R = qr(A)` single-output form).
///
/// @param A   `m × n` matrix with `m >= n`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upper-triangular `R`.
/// @see qr_decompose
Value qr_R_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Singular value decomposition (`[U, S, V] = svd_decompose(A)`).
///
/// `A = U · S · V'`. One-sided Jacobi rotations; converges to
/// orthogonal columns. For `m × n` `A` with `m >= n`: `U` is `m × m`
/// orthogonal, `S` is `m × n` diagonal (sigma >= 0, descending), `V`
/// is `n × n` orthogonal. `m < n` cases transpose internally.
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(U, S, V)` triple.
/// @see svd_values, pinv
std::tuple<Value, Value, Value>
svd_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Singular values only (`s = svd(A)` single-output form).
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of singular values, length `min(m, n)`,
///            descending order.
/// @see svd_decompose
Value svd_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical rank (`r = rank(A, tol)`).
///
/// Count of singular values above `tol`. Default `tol = max(size(A))·eps(max(svd(A)))`.
///
/// @param A    Input matrix.
/// @param tol  Tolerance, or `-1.0` for default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar rank.
Value rank_of(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Moore-Penrose pseudoinverse (`P = pinv(A, tol)`).
///
/// Via SVD: `pinv(A) = V · S⁺ · U'` where `S⁺` inverts non-zero
/// singular values above `tol`.
///
/// @param A    Input matrix.
/// @param tol  Tolerance, or `-1.0` for default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Pseudoinverse.
/// @see inv, lsqminnorm
Value pinv(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-norm condition number (`c = cond(A)`).
///
/// `sigma_max / sigma_min` via SVD. Returns `Inf` for singular `A`.
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar condition number.
/// @see rcond, svd_values
Value cond_2norm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Orthonormal basis for range(A) (`Q = orth(A, tol)`).
///
/// Columns of `U` from SVD whose corresponding sigma exceeds `tol`.
///
/// @param A    Input matrix.
/// @param tol  Tolerance, or `-1.0` for default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `m × rank(A)` orthonormal basis.
/// @see null_basis
Value orth(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Orthonormal basis for null(A) (`N = null(A, tol)`).
///
/// Columns of `V` from SVD whose corresponding sigma is below `tol`.
///
/// @param A    Input matrix.
/// @param tol  Tolerance, or `-1.0` for default.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `n × (n - rank(A))` orthonormal basis.
/// @see orth
Value null_basis(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-norm estimate (`n = normest(A)`).
///
/// Currently equivalent to `svd(A)(1)` — no power-iteration shortcut
/// yet (correctness over performance).
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Estimate of largest singular value.
/// @see svd_values, norm_value
Value normest(const Value &A, std::pmr::memory_resource *mr = nullptr);

// ── Eigenvalues / eigenvectors ───────────────────────────────────────

/// @brief Symmetric eigendecomposition (`[V, D] = eig(A)`).
///
/// Classical Jacobi rotations. Returns `(V, D)` such that `A·V == V·D`,
/// `V` orthogonal, `D` diagonal. Throws if `A` is not symmetric within
/// tolerance (general eig is @ref eig_general_VD / @ref eig_general_values).
///
/// @param A   Square symmetric matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(V, D)` pair.
/// @throws Error  Non-symmetric input.
/// @see eig_values, eig_general_VD
std::tuple<Value, Value>
eig_symmetric(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Eigenvalues only (`e = eig(A)` single-output form).
///
/// For symmetric `A` returns ascending real eigenvalues.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of eigenvalues.
/// @see eig_symmetric, eig_general_values
Value eig_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Characteristic polynomial of a matrix (`p = poly(A)`).
///
/// Souriau-Faddeev-LeVerrier. `p(λ) = λ^n + p(2)·λ^(n-1) + … + p(n+1)`
/// and `roots(p) == eig(A)`. Square inputs only.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Coefficient row of length `n + 1`.
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief General (non-symmetric) eigenvalues (`e = eig(A)` general form).
///
/// Via characteristic polynomial + roots. Possibly-complex column.
/// Less numerically stable than QR iteration but works for moderate `n`.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of eigenvalues (possibly COMPLEX).
/// @see eig_general_VD
Value eig_general_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Subspace angle (`theta = subspace(A, B)`).
///
/// `theta = acos(min(svd(orth(A)' · orth(B))))`. Returns radians in
/// `[0, π/2]`.
///
/// @param A   First matrix.
/// @param B   Second matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar angle (radians).
Value subspace(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief General `[V, D]` eig for real-eigenvalue asymmetric `A`
/// (`[V, D] = eig_general_VD(A)`).
///
/// For each real eigenvalue `λ_i`, eigenvector `v_i` is the last column
/// of `V` from `svd(A - λ_i · I)` (right null vector). Throws if any
/// eigenvalue has non-zero imaginary part (those require Francis QR
/// iteration for proper complex-eigvec extraction — deferred).
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(V, D)` pair with `A·V == V·D` verified.
/// @throws Error  Complex eigenvalue encountered.
/// @see eig_general_values, eig_symmetric
std::tuple<Value, Value>
eig_general_VD(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Sylvester equation for symmetric A and B
/// (`X = sylvester_sym(A, B, C)`).
///
/// Solves `A·X + X·B = C` via simultaneous diagonalisation:
/// `A = V_a·D_a·V_a'`, `B = V_b·D_b·V_b'`, `Y = V_a'·C·V_b`,
/// `Y_ij /= (d_a_i + d_b_j)`, then `X = V_a·Y·V_b'`.
///
/// @param A   Symmetric matrix.
/// @param B   Symmetric matrix.
/// @param C   RHS matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Solution `X`.
/// @throws Error  Non-symmetric A or B; degenerate spectra
///                (`d_a_i + d_b_j == 0` for some pair).
Value sylvester_sym(const Value &A, const Value &B, const Value &C, std::pmr::memory_resource *mr = nullptr);

// ── Norms ────────────────────────────────────────────────────────────

/// @brief Vector / matrix p-norm (`n = norm(x, p)`).
///
/// Vector input: `norm(v, p) = (Σ |v|^p)^(1/p)`; `p = 1` → sum of abs,
/// `p = 2` → Euclidean. Matrix input: `p = 1` → max column sum, `p = 2`
/// → largest singular value.
///
/// @param x   Input array (vector or matrix).
/// @param p   Norm order (positive real).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar norm.
/// @see norm_inf, norm_fro, vecnorm
Value norm_value(const Value &x, double p, std::pmr::memory_resource *mr = nullptr);

/// @brief Inf-norm (`n = norm(x, inf)`).
///
/// Vector: `max(|v|)`. Matrix: max row sum.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar norm.
/// @see norm_value
Value norm_inf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Frobenius norm (`n = norm(A, 'fro')`).
///
/// `sqrt(sum(A.^2))`.
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar Frobenius norm.
/// @see norm_value
Value norm_fro(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Matrix functions (expm / logm / sqrtm / schur / hess) ────────────

/// @brief Matrix exponential (`B = expm(A)`).
///
/// Padé approximation with scaling-and-squaring (Higham 2005). Works
/// for any square matrix.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `e^A`.
/// @see logm_sym, sqrtm_sym
Value expm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix logarithm for symmetric positive-definite A
/// (`B = logm_sym(A)`).
///
/// Via eigendecomposition: `logm(A) = V · diag(log(eig)) · V'`.
/// General `logm` requires complex Schur (deferred).
///
/// @param A   Symmetric positive-definite matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log(A)`.
/// @see expm
Value logm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix square root for symmetric PSD A (`B = sqrtm_sym(A)`).
///
/// Via eigendecomposition: `sqrtm(A) = V · diag(sqrt(eig)) · V'`.
///
/// @param A   Symmetric positive-semidefinite matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `sqrt(A)`.
/// @see expm, logm_sym
Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Schur decomposition for symmetric A (`[U, T] = schur(A)`).
///
/// Equivalent to eigendecomposition: `A = U·T·U'`, `T` diagonal,
/// `U` orthogonal. General Schur (quasi-triangular `T`) is deferred.
///
/// @param A   Symmetric matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(U, T)` pair.
/// @see eig_symmetric, hess
std::tuple<Value, Value>
schur_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Hessenberg reduction (`[P, H] = hess(A)`).
///
/// `A = P·H·P'`, `P` orthogonal, `H` upper-Hessenberg (zeros below the
/// first sub-diagonal). Foundation for general eig and Schur.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(P, H)` pair.
/// @see hess_H_only
std::tuple<Value, Value>
hess(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Hessenberg-only output (`H = hess(A)` single-output form).
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upper-Hessenberg `H`.
/// @see hess
Value hess_H_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

// ── Matrix predicates ────────────────────────────────────────────────

/// @brief Banded structure test (`tf = isbanded(A, lower, upper)`).
///
/// True iff entries outside the `[-lower, +upper]` diagonal band are
/// exactly zero. Exact comparison (no tolerance).
///
/// @param A      Input matrix.
/// @param lower  Allowed sub-diagonal width.
/// @param upper  Allowed super-diagonal width.
/// @param mr     Memory resource (nullptr → process default).
/// @return       LOGICAL scalar.
/// @see isdiag, istril, istriu, bandwidth
Value isbanded(const Value &A, long lower, long upper, std::pmr::memory_resource *mr = nullptr);

/// @brief Diagonal structure (`tf = isdiag(A)`). True iff `isbanded(A, 0, 0)`.
/// @param A   Input matrix. @param mr  Memory resource. @return  LOGICAL scalar.
Value isdiag(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Lower-triangular structure (`tf = istril(A)`).
/// @param A   Input matrix. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see istriu
Value istril(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Upper-triangular structure (`tf = istriu(A)`).
/// @param A   Input matrix. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see istril
Value istriu(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Symmetry test (`tf = issymmetric(A, skew)`).
///
/// `skew == false` (default): `A == A.'` (transpose, no conj).
/// `skew == true`: `A == -A.'`.
///
/// @param A     Input matrix.
/// @param skew  Skew-symmetric flag.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL scalar.
/// @see ishermitian
Value issymmetric(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Hermitian test (`tf = ishermitian(A, skew)`).
///
/// `A == A'` (conjugate transpose); `skew == true` → `A == -A'`.
///
/// @param A     Input matrix.
/// @param skew  Skew-Hermitian flag.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL scalar.
/// @see issymmetric
Value ishermitian(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Bandwidth pair (`[lower, upper] = bandwidth(A)`).
///
/// Single-output form returns just the lower bandwidth
/// (`x = bandwidth(A)`).
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(lower, upper)` pair.
/// @see bandwidthOpt, isbanded
std::pair<Value, Value>
bandwidth(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Single-output bandwidth selector (`x = bandwidth(A, which)`).
///
/// `which` is `"lower"` or `"upper"`.
///
/// @param A      Input matrix.
/// @param which  `"lower"` or `"upper"`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar bandwidth.
/// @throws Error  Unknown `which` value.
Value bandwidthOpt(const Value &A, const std::string &which, std::pmr::memory_resource *mr = nullptr);

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
/// @see norm_value
Value vecnorm(const Value &A, double p = 2.0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Reduced row echelon form (`[R, jb] = rref(A, have_tol, tol)`).
///
/// `jb` is the 1-based pivot-column indices. Real-only in v1.
///
/// @param A         Input matrix.
/// @param have_tol  When `true`, use `tol_user`; otherwise default to
///                  `max(M, N) · eps(norm(A, inf))`.
/// @param tol_user  User-supplied tolerance.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `(R, jb)` pair.
std::pair<Value, Value>
rref(const Value &A, bool have_tol, double tol_user, std::pmr::memory_resource *mr = nullptr);

/// @brief Reciprocal 1-norm condition estimate (`c = rcond(A)`).
///
/// Cheap path: `1 / (norm(A, 1) · norm(inv(A), 1))`. Returns 0 for
/// singular `A`. KNOWN GAP: accurate on well-conditioned cases;
/// differs from LAPACK's `dgecon` on near-singular matrices.
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar reciprocal condition number.
/// @see cond_2norm
Value rcond(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Givens plane rotation (`[G, y_out] = planerot([x; y])`).
///
/// Returns `G` such that `G · [x; y] = [r; 0]` where `r = hypot(x, y)`.
/// Real-only.
///
/// @param xy  `2 × 1` input column.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(G, y_out)` pair (`y_out` is the rotated vector).
std::pair<Value, Value>
planerot(const Value &xy, std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum-norm least-squares solution (`X = lsqminnorm(A, B, have_tol, tol)`).
///
/// Computes `pinv(A, tol) · B`. `rankWarn` / `RegularizationFactor`
/// name-value args deferred.
///
/// @param A         Coefficient matrix (may be rank-deficient).
/// @param B         RHS matrix.
/// @param have_tol  When `true`, use `tol_user`; otherwise default.
/// @param tol_user  User-supplied tolerance.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Minimum-norm solution `X`.
/// @see pinv, linsolve
Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref balance_impl.
struct BalanceResult {
    Value B;         ///< Balanced matrix.
    Value d_col;     ///< Column of scaling factors.
    Value perm_col;  ///< Column of permutation indices (1..n in v1).
};

/// @brief Diagonal-similarity balancing for eigenvalue computations
/// (`[B, d, p] = balance(A, noperm)`).
///
/// Parlett-Reinsch (1969). v1 implements only the scaling phase
/// (permutation phase deferred; behaves like `balance(A, 'noperm')`).
/// Dispatch in `balance_reg` picks 1/2/3-output forms.
///
/// @param A       Square matrix.
/// @param noperm  Skip permutation phase (currently always `true`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `{B, d_col, perm_col}` struct.
BalanceResult balance_impl(const Value &A, bool noperm, std::pmr::memory_resource *mr = nullptr);

/// @brief Block LDL' factorisation (`[L, D, P] = ldl(A, upper_form, p_as_vector)`).
///
/// v1 implements Crout LDL' without pivoting (works for PD/ND and most
/// indefinite matrices). `P` is identity in v1. Bunch-Kaufman 2×2
/// pivoting and the sparse `[L, D, P, C]` form are deferred.
///
/// @param A             Symmetric matrix.
/// @param upper_form    When `true`, return upper-triangular `L`.
/// @param p_as_vector   When `true`, return `P` as a `1 × n` permutation
///                      vector instead of a matrix.
/// @param mr            Memory resource (nullptr → process default).
/// @return              `(L, D, P)` triple.
/// @see chol
std::tuple<Value, Value, Value>
ldl(const Value &A, bool upper_form, bool p_as_vector, std::pmr::memory_resource *mr = nullptr);

// ── Shape / size queries ─────────────────────────────────────────────

/// @brief Size as a row vector (`s = size(x)`).
///
/// Returns `[rows, cols]` (2-D) or `[rows, cols, pages]` (3-D+).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of dimensions.
/// @see size(x, dim, mr), sizePair, numel, length
Value size(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Size along a single dim (`d = size(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar size along `dim`.
Value size(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Size as a `(rows, cols)` pair (`[r, c] = size(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(rows, cols)` pair.
std::tuple<Value, Value> sizePair(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest dimension (`n = length(x)`).
///
/// `max(size(x))`; 0 for empty arrays.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar length.
/// @see numel, size
Value length(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Total element count (`n = numel(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    Scalar element count. @see length, size
Value numel(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number of dimensions (`n = ndims(x)`).
///
/// 2 for matrix, 3 for 3-D array, etc.
///
/// @param x   Input array. @param mr  Memory resource.
/// @return    Scalar ndims.
Value ndims(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Reshape preserving column-major order (`y = reshape(x, rows, cols, pages)`).
///
/// `numel(x)` must equal `rows · cols · (pages == 0 ? 1 : pages)`.
/// `pages == 0` → 2-D output. A `[]` dimension placeholder must be
/// resolved by the caller — this function requires concrete dims.
///
/// @param x      Input array.
/// @param rows   Target rows.
/// @param cols   Target cols.
/// @param pages  Target pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Reshaped array (column-major preserved).
/// @throws Error  Element-count mismatch.
/// @see reshapeND
Value reshape(const Value &x, size_t rows, size_t cols, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief ND reshape (`y = reshapeND(x, dims)`).
///
/// Accepts a flat dim list of arbitrary rank. Same element-count check
/// as the 2-D / 3-D form. CELL / STRING reshape past 3-D is not yet
/// supported.
///
/// @param x     Input array.
/// @param dims  Target shape span.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Reshaped array.
/// @throws Error  Element-count mismatch (`m:reshape:elementCountMismatch`),
///                CELL/STRING ND > 3 (`m:reshape:cellND`).
Value reshapeND(const Value &x, Span<const size_t> dims, std::pmr::memory_resource *mr = nullptr);

// ── Transpose family ─────────────────────────────────────────────────

/// @brief 2-D non-conjugate transpose (`y = transpose(x)`).
///
/// DOUBLE-only. Use @ref pagetranspose for ND or COMPLEX.
///
/// @param x   2-D matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Transposed matrix.
/// @throws Error  3-D input.
/// @see pagetranspose, pagectranspose
Value transpose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise transpose (`y = pagetranspose(x)`).
///
/// Each `rows × cols` page is transposed independently. For 1-D / 2-D
/// inputs falls back to plain transpose. Real-only at the element
/// level; complex elements transposed without conjugation.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Page-transposed array.
/// @see pagectranspose
Value pagetranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise conjugate transpose (`y = pagectranspose(x)`).
///
/// Identical to @ref pagetranspose for real inputs; for COMPLEX
/// inputs conjugates each element while transposing.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Conjugate-transposed array.
Value pagectranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Demo surfaces ────────────────────────────────────────────────────

/// @brief Demo surface `peaks(n)`.
///
/// Sample points of the bivariate polynomial-exponential function on
/// the `n × n` grid `(x, y) = linspace(-3, 3, n)`. Default `n = 49`.
///
/// @param n   Grid size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × n` DOUBLE matrix.
Value peaks(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Triple-output surface (struct returned by @ref sphere /
///        @ref cylinder / @ref ellipsoid).
struct Surface3 {
    Value X;  ///< x-coordinates.
    Value Y;  ///< y-coordinates.
    Value Z;  ///< z-coordinates.
};

/// @brief Unit sphere surface (`[X, Y, Z] = sphere(n)`).
///
/// `(n + 1) × (n + 1)` grids. Default `n = 20`.
///
/// @param n   Resolution.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y, Z)` struct.
/// @see cylinder, ellipsoid
Surface3 sphere(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Surface of revolution (`[X, Y, Z] = cylinder(R, n)`).
///
/// Profile `R` revolved along `z ∈ [0, 1]`. Output is
/// `length(R) × (n + 1)`. Default `n = 20`, default `R = [1 1]` (unit
/// cylinder).
///
/// @param R   Radius profile.
/// @param n   Angular resolution.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y, Z)` struct.
/// @see sphere
Surface3 cylinder(const Value &R, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Axis-aligned ellipsoid (`[X, Y, Z] = ellipsoid(xc, yc, zc, xr, yr, zr, n)`).
///
/// Centered at `(xc, yc, zc)` with semi-axes `(xr, yr, zr)`. Output
/// `(n + 1) × (n + 1)`.
///
/// @param xc,yc,zc  Centre coordinates.
/// @param xr,yr,zr  Semi-axes.
/// @param n         Resolution.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `(X, Y, Z)` struct.
/// @see sphere
Surface3 ellipsoid(double xc, double yc, double zc, double xr, double yr, double zr, size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Page-wise matmul / diag / sort / find ────────────────────────────

/// @brief Transpose mode for @ref pagemtimes.
enum class TranspOp {
    None,        ///< No transpose.
    Transpose,   ///< Per-page transpose (no conj).
    CTranspose   ///< Per-page conjugate transpose.
};

/// @brief Page-wise matrix multiply (`C = pagemtimes(A, B)`).
///
/// Treats axes 1–2 as `M × K` / `K × N` matrices, axes ≥ 3 as batch
/// dims. Output shape is `[M, N, ...broadcast(batchA, batchB)]`.
/// DOUBLE only.
///
/// @param x   Left operand.
/// @param y   Right operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Page-wise product.
/// @throws Error  Inner-dim mismatch.
/// @see pagemtimes(x, tx, y, ty, mr)
Value pagemtimes(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise matmul with transpose flags
/// (`C = pagemtimes(A, tx, B, ty)`).
///
/// `tx` / `ty` are option strings: `"none"` = no op, `"transpose"` =
/// per-page transpose, `"ctranspose"` = per-page conjugate-transpose
/// (identical to transpose for real input; complex input not yet
/// supported).
///
/// @param x   Left operand.
/// @param tx  Transpose mode for `x`.
/// @param y   Right operand.
/// @param ty  Transpose mode for `y`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Page-wise product.
/// @see pagetranspose, pagectranspose
Value pagemtimes(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr = nullptr);

/// @brief Diagonal / build-from-diagonal (`y = diag(x)`).
///
/// Matrix input → main diagonal as a column vector.
/// Vector input → diagonal matrix with the vector on the main diagonal.
///
/// @param x   Vector or matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector or square matrix.
/// @see eye, trace
Value diag(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Sort along first non-singleton dim (`[sorted, idx] = sort(x)`).
///
/// Indices are 1-based permutation. For 3-D input, operates per-slice.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(sorted, idx)` pair.
/// @see sortrows
std::tuple<Value, Value> sort(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Lex-sort rows ascending (`[sorted, idx] = sortrows(M)`).
///
/// Stable sort across all columns (column 1 most significant).
/// Promotes integer / logical input to DOUBLE.
///
/// @param x   2-D matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(sorted, idx)` pair; `idx` is 1-based original row order.
/// @see sortrows(x, cols, mr), sort
std::tuple<Value, Value> sortrows(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Lex-sort rows by selected columns
/// (`[sorted, idx] = sortrows(M, cols)`).
///
/// Each `cols` entry is a 1-based column index; negative entries flip
/// direction (descending). Empty `cols` ≡ 1-arg form (all ascending).
///
/// @param x     2-D matrix.
/// @param cols  Signed 1-based column keys.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(sorted, idx)` pair.
std::tuple<Value, Value> sortrows(const Value &x, Span<const int> cols, std::pmr::memory_resource *mr = nullptr);

/// @brief Linear indices of non-zero entries (`idx = find(x)`).
///
/// Row vector when `x` is a row, column vector otherwise.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1-based indices of true / non-zero entries.
/// @see nnz, nonzeros
Value find(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Count non-zero entries (`n = nnz(x)`).
///
/// NaN counts as non-zero. For COMPLEX, an element is non-zero iff
/// either part is non-zero.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE scalar count.
/// @see find, nonzeros
Value nnz(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Non-zero entries as a column vector (`v = nonzeros(x)`).
///
/// Column-major order. Output type matches input.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of non-zero entries.
/// @see find, nnz
Value nonzeros(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Concatenation, grids, products ───────────────────────────────────

/// @brief Horizontal concatenation along columns (`C = horzcat(values)`).
///
/// @param values  Inputs to concatenate (must agree on row counts).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Concatenated matrix.
/// @throws Error  Row-count mismatch.
/// @see vertcat, cat
Value horzcat(Span<const Value> values, std::pmr::memory_resource *mr = nullptr);

/// @brief Vertical concatenation along rows (`C = vertcat(values)`).
///
/// @param values  Inputs to concatenate (must agree on column counts).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Concatenated matrix.
/// @throws Error  Column-count mismatch.
/// @see horzcat, cat
Value vertcat(Span<const Value> values, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D meshgrid (`[X, Y] = meshgrid(x, y)`).
///
/// `X` / `Y` are `numel(y) × numel(x)` matrices.
///
/// @param x   x-axis grid.
/// @param y   y-axis grid.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y)` pair.
/// @see ndgrid, meshgrid(x, y, z, mr)
std::tuple<Value, Value> meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D meshgrid (`[X, Y, Z] = meshgrid(x, y, z)`).
///
/// Each output is `numel(y) × numel(x) × numel(z)`.
///
/// @param x   x-axis grid.
/// @param y   y-axis grid.
/// @param z   z-axis grid.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y, Z)` triple.
std::tuple<Value, Value, Value>
meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D N-D companion to meshgrid (`[X, Y] = ndgrid(x, y)`).
///
/// Each output has shape `[numel(x), numel(y), …]` (first-arg axes-major)
/// — the opposite of meshgrid's axis order.
///
/// @param x   First-axis grid.
/// @param y   Second-axis grid.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y)` pair.
/// @see meshgrid
std::tuple<Value, Value>
ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D ndgrid (`[X, Y, Z] = ndgrid(x, y, z)`).
///
/// Outputs have shape `[numel(x), numel(y), numel(z)]`.
///
/// @param x   First-axis grid.
/// @param y   Second-axis grid.
/// @param z   Third-axis grid.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(X, Y, Z)` triple.
std::tuple<Value, Value, Value>
ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Kronecker product (`K = kron(A, B)`).
///
/// Output is `(rA·rB) × (cA·cB)`; the `(i, j)`-th block (`rB × cB`)
/// equals `A(i, j) · B`. Vector inputs are treated as matrices of
/// their natural orientation. DOUBLE only (integer / logical / single
/// promoted; complex throws).
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Kronecker product.
/// @throws Error  COMPLEX input not supported.
Value kron(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Cumulative ops and diff ──────────────────────────────────────────

/// @brief Cumulative sum, auto-dim (`y = cumsum(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    Same-shape cumulative sums. @see cumsum(x, dim, mr)
Value cumsum(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative sum along `dim` (`y = cumsum(x, dim)`).
/// @param x    Input. @param dim  1-based dim (0 → auto).
/// @param mr   Memory resource. @return  Same-shape cumulative sums.
Value cumsum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative product (`y = cumprod(x, dim)`).
/// @param x   Input. @param dim  1-based dim (0 → auto).
/// @param mr  Memory resource. @return  Same-shape cumulative products.
Value cumprod(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative maximum (`y = cummax(x, dim)`).
/// @param x   Input. @param dim  1-based dim (0 → auto).
/// @param mr  Memory resource. @return  Same-shape running max.
Value cummax(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative minimum (`y = cummin(x, dim)`).
/// @param x   Input. @param dim  1-based dim (0 → auto).
/// @param mr  Memory resource. @return  Same-shape running min.
Value cummin(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete difference (`y = diff(x, n, dim)`).
///
/// `n`-th order differences: `out[i] = x[i+1] - x[i]`. Output shape
/// reduces `dim[d-1]` by `n` (clamped to 0). `n = 0` returns a copy.
/// Scalar input returns `1 × 0` empty.
///
/// @param x    Input array.
/// @param n    Difference order (default 1).
/// @param dim  1-based dim (0 → first non-singleton).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Differences along `dim`.
Value diff(const Value &x, int n = 1, int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Logical reductions / cross / dot ─────────────────────────────────

/// @brief `any` reduction (`y = any(x, dim)`).
///
/// Collapses `dim` to a single LOGICAL value. Empty slice → `false`.
///
/// @param x    Input array.
/// @param dim  1-based dim (0 → first non-singleton).
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL reduction.
/// @see allOf, xorOf
Value anyOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief `all` reduction (`y = all(x, dim)`).
///
/// Empty slice → `true`.
///
/// @param x    Input array.
/// @param dim  1-based dim (0 → first non-singleton).
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL reduction.
/// @see anyOf
Value allOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise xor (`y = xor(a, b)`).
///
/// Both inputs coerced to LOGICAL.
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, broadcast shape.
/// @see logicalAnd, logicalOr
Value xorOf(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Cross product of 3-vectors (`c = cross(a, b)`).
///
/// Row vector output.
///
/// @param a   First 3-vector.
/// @param b   Second 3-vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row 3-vector cross product.
/// @see dot
Value cross(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Dot product (`d = dot(a, b)`).
///
/// Two vectors of equal length.
///
/// @param a   First vector.
/// @param b   Second vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar dot product.
/// @see cross
Value dot(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
