// libs/builtin/include/numkit/builtin/language/arrays/matrix.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {

//// All-zero matrix. pages == 0 → 2D matrix.
Value zeros(size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value ones(size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// Identity matrix. rows, cols may differ — produces rectangular "identity".
Value eye(size_t rows, size_t cols, std::pmr::memory_resource *mr = nullptr);

/// Magic square of order N (N×N matrix where rows, columns, both
/// diagonals all sum to the magic constant N·(N²+1)/2).
/// Three branches by N's parity, matching MATLAB R2025b:
///   - N odd  (N >= 3) : Siamese / de la Loubère method
///   - N ≡ 0 mod 4     : doubly-even (4×4-block diagonal swap pattern)
///   - N ≡ 2 mod 4     : singly-even (Strachey -- 4 odd-magic quadrants
///                                    + corner swaps)
/// Edge cases: N == 0 → 0×0; N == 1 → [1]; N == 2 → MATLAB's [1 3; 4 2]
/// (not strictly magic; preserved for parity).
Value magic(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Toeplitz matrix from first column c (length m) and optional first
/// row r (length n). T[i, j] = c[i-j] if i >= j else r[j-i].
/// Single-arg form: r is taken as conj(c) (real input → r = c).
/// MATLAB convention: if c[0] != r[0], r[0] is silently overridden by c[0].
Value toeplitz(const double *c, std::size_t m, const double *r, std::size_t n, std::pmr::memory_resource *mr = nullptr);

/// Hankel matrix from first column c (length m) and optional last
/// row r (length n). H[i, j] = c[i+j] for i+j < m, else r[i+j-m+1].
/// Single-arg form: r is all zeros (anti-triangular Hankel).
/// MATLAB convention: if c[end] != r[0], r[0] is silently overridden.
Value hankel(const double *c, std::size_t m, const double *r, std::size_t n, std::pmr::memory_resource *mr = nullptr);

/// Vandermonde matrix V[i, j] = v[i]^(n-1-j) where n = numel(v).
/// Returns n×n matrix; columns from highest to lowest power
/// (matches MATLAB R2025b layout).
Value vander(const double *v, std::size_t n, std::pmr::memory_resource *mr = nullptr);

/// Companion matrix of monic polynomial p (length n+1).
/// Returns n×n matrix whose top row is [-p[1]/p[0], ..., -p[n]/p[0]]
/// and whose subdiagonal is all ones (eigenvalues = roots of p).
Value compan(const double *p, std::size_t pn, std::pmr::memory_resource *mr = nullptr);

/// Pascal matrix of order n. Default form (k = 0): symmetric with
/// P[i, j] = C(i+j, i). MATLAB also defines k=1 (lower-triangular
/// Cholesky factor) and k=2 (cube-root of identity); only k=0 is
/// implemented in this revision.
Value pascal(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Hilbert matrix of order n. H[i, j] = 1 / (i + j - 1) (1-indexed).
Value hilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Inverse Hilbert matrix of order n via the closed-form formula
/// involving binomials. Exact integer entries up to n ≈ 13; for
/// larger n the result loses accuracy due to floating-point overflow
/// in the binomial coefficients.
Value invhilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Wilkinson's eigenvalue test matrix: symmetric tridiagonal with
/// subdiagonal of ones and main diagonal = |(1:n) - (n+1)/2|.
Value wilkinson(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Hadamard matrix of order n via the Sylvester construction.
/// Requires n to be a power of 2 (1, 2, 4, 8, ...). Other valid
/// MATLAB orders (12·2^k, 20·2^k via Paley constructions) are
/// deferred -- see implementation note.
Value hadamard(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Rosser's 8×8 eigenvalue test matrix (hardcoded constants from
/// MATLAB R2025b's gallery / rosser).
Value rosser(std::pmr::memory_resource *mr = nullptr);

/// Matrix inverse via LU. inv(A) ≡ A \ eye(n) -- prefer mldivide /
/// linsolve / `\` for solving A·x = b; inv exists for the cases where
/// the inverse itself is needed as a matrix.
/// @throws Error if A is non-square or singular.
Value inv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Solve A·X = B via LU (square A) or Householder QR (tall A,
/// least-squares). Wrapper over the same la_solve backend that powers
/// MATLAB's mldivide / `\`. The optional 3rd argument `opts` is
/// accepted for MATLAB-compatibility but ignored in this revision
/// (LU/QR auto-detection handles the same cases).
/// @throws Error on singular / rank-deficient / wide A.
Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// Page-wise inverse of a 3D array A (m×n×p). Each m×n page is
/// independently inverted via LU. Output shape matches input.
/// @throws Error if any page is non-square or singular.
Value pageinv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Sum of the diagonal elements of A. Equivalent to sum(diag(A)).
/// Works for any 2D matrix (square or rectangular).
Value trace(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Determinant via LU factorisation with partial pivoting.
/// det(A) = sign(P) * prod(diag(U)) where A = P·L·U.
/// @throws Error if A is non-square.
Value det(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Cholesky factorisation of a symmetric positive-definite matrix A.
/// Returns upper-triangular R such that R' * R = A
/// (matches MATLAB R2025b's chol(A) default).
/// @throws Error if A is non-square or not positive-definite.
Value chol(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Top k rows of A in sort order. Default sort: descending by all
/// columns lexicographically. Single-arg form (k only) sorts on every
/// column. Single-arg form requires the matrix to have at least k rows.
Value topkrows(const Value &A, std::size_t k, std::pmr::memory_resource *mr = nullptr);

/// LU decomposition of an n×n matrix A with partial pivoting:
/// returns (L, U, P) where L is unit-lower-triangular, U is
/// upper-triangular, P is the permutation matrix, and P*A == L*U.
/// MATLAB single-output form `LU = lu(A)` returns L+U combined
/// (zero diagonal of L implicit, P baked into L's row order).
/// @throws Error if A is non-square.
std::tuple<Value, Value, Value>
lu_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Combined L+U output: returns a single matrix whose strict lower
/// triangle is L (unit diagonal implicit) and whose upper triangle
/// (including diagonal) is U, with rows already permuted -- matches
/// MATLAB's single-output `lu(A)` form.
Value lu_combined(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// QR decomposition of an m×n matrix A (m >= n) via Householder
/// reflections: returns (Q, R) where Q is m×m orthogonal and R is
/// m×n upper-triangular, with A == Q*R. Full-size form (not "econ").
/// MATLAB single-output form `R = qr(A)` returns just R.
/// @throws Error if A has more columns than rows.
std::tuple<Value, Value>
qr_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// R-only output -- matches MATLAB's single-output `qr(A)` form.
Value qr_R_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Singular Value Decomposition: A = U * S * V'.
/// One-sided Jacobi rotations on the columns of A; converges to
/// orthogonal columns and reads sigma_i = ||A(:,i)||.
///
/// For m×n A with m >= n:
///   U is m×m orthogonal, S is m×n diagonal (sigma >= 0, descending),
///   V is n×n orthogonal.
/// For m < n we transpose, run, and swap U/V at the end.
///
/// MATLAB single-output form `s = svd(A)` returns the singular values
/// as a column vector (length min(m,n), descending order).
std::tuple<Value, Value, Value>
svd_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Singular values only -- matches MATLAB's single-output svd(A).
Value svd_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Numerical rank: count of singular values above tolerance.
/// Default tol = max(size(A)) * eps(max(svd(A))). Two-arg form
/// rank(A, tol) takes user tolerance.
Value rank_of(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// Pseudoinverse (Moore-Penrose) via SVD: pinv(A) = V * S^+ * U'
/// where S^+ inverts non-zero singular values above tolerance.
Value pinv(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// 2-norm condition number: cond(A) = sigma_max / sigma_min via SVD.
/// Returns Inf for singular A.
Value cond_2norm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Orthonormal basis for the range of A (n columns of U from SVD
/// where corresponding sigma > tolerance). Output is m × rank(A).
Value orth(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// Orthonormal basis for the null space of A (n - rank(A) columns of
/// V from SVD where corresponding sigma is below tolerance). Output
/// is n × (n - rank(A)).
Value null_basis(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// Estimate of the 2-norm (largest singular value). Same as svd(A)(1)
/// for now (no power-iteration shortcut yet -- correctness over
/// performance).
Value normest(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Eigenvalues + eigenvectors of a symmetric real matrix via
/// classical Jacobi rotations. A must be square; if not symmetric
/// within tol, throws (general eig requires Hessenberg + Francis QR
/// iteration; deferred as Phase 2b).
///
/// Returns (V, D) such that A*V == V*D, V orthogonal, D diagonal.
/// MATLAB single-output form `e = eig(A)` returns eigenvalues as a
/// column vector (ascending order for symmetric A).
std::tuple<Value, Value>
eig_symmetric(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Eigenvalues only -- matches MATLAB's single-output eig(A).
Value eig_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Characteristic polynomial coefficients of a square matrix A
/// via Souriau-Faddeev-LeVerrier algorithm.
/// Returns p such that p(lambda) = lambda^n + p(2)*lambda^(n-1)
/// + ... + p(n+1) and roots(p) == eig(A).
/// Matches MATLAB's `poly(A)` for square inputs.
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// General (non-symmetric) eigenvalues of A via characteristic
/// polynomial + roots. Returns possibly-complex column vector.
/// Numerically less stable than QR iteration but works for moderate
/// n; QR-iteration (Phase 2c-3) will be a future replacement.
Value eig_general_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Angle between two subspaces spanned by the columns of A and B.
/// Computed as theta = acos(min(svd(orth(A)' * orth(B)))).
/// Returns radians in [0, pi/2].
Value subspace(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// General [V, D] eig for asymmetric matrices when ALL eigenvalues
/// are real. For each real eigenvalue λ_i, eigenvector v_i is the
/// last column of V from svd(A - λ_i I) (right null vector).
/// Throws if any eigenvalue has non-zero imaginary part -- those
/// require Francis QR iteration for proper complex-eigvec extraction
/// (Phase 2c-3-future). Returns (V, D) with A*V == V*D verified.
std::tuple<Value, Value>
eig_general_VD(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Sylvester equation A*X + X*B = C for symmetric A and B.
/// Solved via simultaneous diagonalisation:
///   A = V_a*D_a*V_a',  B = V_b*D_b*V_b'
///   Y = V_a'*C*V_b,    y_ij = Y_ij / (d_a_i + d_b_j)
///   X = V_a*Y*V_b'
/// Throws if A or B is non-symmetric (general case requires Bartels-
/// Stewart on Schur forms -- deferred). Throws if any d_a_i + d_b_j
/// equals zero (no unique solution).
Value sylvester_sym(const Value &A, const Value &B, const Value &C, std::pmr::memory_resource *mr = nullptr);

/// Vector or matrix norm. Vector input:
///   norm(v)         = norm(v, 2) = sqrt(sum(|v|^2))     (Euclidean)
///   norm(v, p)      = (sum(|v|^p))^(1/p)
///   norm(v, inf)    = max(|v|)
///   norm(v, 1)      = sum(|v|)
/// Matrix input:
///   norm(A)         = norm(A, 2) = max singular value
///   norm(A, 1)      = max column sum
///   norm(A, inf)    = max row sum
///   norm(A, 'fro')  = sqrt(sum(A.^2))
Value norm_value(const Value &x, double p, std::pmr::memory_resource *mr = nullptr);
Value norm_inf(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value norm_fro(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Matrix exponential expm(A) via Padé approximation with scaling-
/// and-squaring (Higham 2005). Works for any square matrix
/// (symmetric or not). For symmetric A could go via eig but Padé
/// is more general and still bit-stable.
Value expm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Matrix logarithm logm(A) for symmetric positive-definite A only
/// (general logm requires complex Schur, deferred to Phase 2b).
/// Computed via eigendecomposition: logm(A) = V * diag(log(eig)) * V'.
Value logm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Matrix square root sqrtm(A) for symmetric positive-semidefinite A
/// only (general sqrtm needs complex Schur). Via eigendecomposition:
/// sqrtm(A) = V * diag(sqrt(eig)) * V'.
Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Schur decomposition. For symmetric A this is equivalent to
/// the eigendecomposition: A = U*T*U' where T is diagonal (real
/// eigenvalues) and U is orthogonal. Returns (U, T).
/// General (non-symmetric) Schur returns quasi-triangular T with
/// 2×2 blocks for complex eigenpairs -- deferred to Phase 2b.
std::tuple<Value, Value>
schur_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Hessenberg reduction of a square matrix A: returns (P, H) such
/// that A = P*H*P', P orthogonal, H upper-Hessenberg (zeros below
/// the first sub-diagonal). Reduction via successive Householder
/// reflectors. Foundation for general eig and Schur (Phase 2c).
std::tuple<Value, Value>
hess(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Hessenberg-only output -- matches MATLAB single-output hess(A).
Value hess_H_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

//// MATLAB-parity matrix predicates (predicates.cpp). All comparisons
//// are exact (== 0); even 1e-300 in an off-band entry returns false.
////
//// isbanded(A, lower, upper) — outside-band entries are zero.
//// isdiag/istril/istriu       — degenerate cases of isbanded.
//// issymmetric(A [, skew])    — A == A.'  (transpose, no conj).
//// ishermitian(A [, skew])    — A == A'   (conjugate transpose).
//// 'skew' opt: A == -A.'  /  A == -A'.
Value isbanded(const Value &A, long lower, long upper, std::pmr::memory_resource *mr = nullptr);
Value isdiag(const Value &A, std::pmr::memory_resource *mr = nullptr);
Value istril(const Value &A, std::pmr::memory_resource *mr = nullptr);
Value istriu(const Value &A, std::pmr::memory_resource *mr = nullptr);
Value issymmetric(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);
Value ishermitian(const Value &A, bool skew = false, std::pmr::memory_resource *mr = nullptr);

/// bandwidth(A) — (lower, upper) bandwidths. Single-output form returns
/// just the lower bandwidth (MATLAB convention: x = bandwidth(A)
/// captures the first output).
std::pair<Value, Value>
bandwidth(const Value &A, std::pmr::memory_resource *mr = nullptr);
Value bandwidthOpt(const Value &A, const std::string &which, std::pmr::memory_resource *mr = nullptr);

/// vecnorm(A [, p [, dim]]) — vector p-norm along dim.
///   defaults: p = 2, dim = first non-singleton dimension.
///   p = Inf  → max(|A|), p = -Inf → min(|A|).
Value vecnorm(const Value &A, double p = 2.0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

//// rref(A [, tol]) — reduced row echelon form. Returns (R, jb) where
//// jb is the 1-based pivot column indices. Default tol =
//// max(M, N) * eps(norm(A, inf)). Real-only in v1.
std::pair<Value, Value>
rref(const Value &A, bool have_tol, double tol_user, std::pmr::memory_resource *mr = nullptr);

/// rcond(A) — reciprocal 1-norm condition estimate. Cheap path:
/// 1 / (norm(A,1) * norm(inv(A),1)). Returns 0 for singular A.
/// KNOWN GAP: matches MATLAB on well-conditioned cases; differs from
/// LAPACK's dgecon on near-singular matrices.
Value rcond(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// planerot([x; y]) — Givens rotation: returns (G, y_out) such that
/// G*[x; y] = [r; 0] where r = hypot(x, y). Real-only.
std::pair<Value, Value>
planerot(const Value &xy, std::pmr::memory_resource *mr = nullptr);

/// lsqminnorm(A, B [, tol]) — minimum-norm least-squares solution
/// to A*X = B for rank-deficient A. Implementation: pinv(A, tol)*B.
/// 'rankWarn' / 'RegularizationFactor' name-value args deferred.
Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user, std::pmr::memory_resource *mr = nullptr);

/// balance(A) — diagonal-similarity scaling for eigenvalue computations
/// (Parlett-Reinsch 1969). v1 implements only the scaling phase
/// (permutation phase deferred; behaves like balance(A, 'noperm')).
/// Returned as a struct with B (balanced matrix), d_col (column of
/// scalings), perm_col (column of permutation indices, 1:n in v1).
/// Dispatch in balance_reg picks 1/2/3-output forms.
struct BalanceResult { Value B; Value d_col; Value perm_col; };
BalanceResult balance_impl(const Value &A, bool noperm, std::pmr::memory_resource *mr = nullptr);

/// ldl(A) — block LDL' factorization. v1 implements Crout LDL'
/// without pivoting (works for PD/ND and most indefinite matrices).
/// Returns (L, D, P) where L is unit lower-triangular (or upper if
/// upper_form), D is diagonal, P is identity (no pivoting in v1).
/// p_as_vector=true returns P as a 1×n vector of permutation indices.
/// KNOWN GAP: complex Hermitian, sparse, Bunch-Kaufman 2×2 pivoting,
/// and the [L,D,P,C] sparse-with-scaling form deferred.
std::tuple<Value, Value, Value>
ldl(const Value &A, bool upper_form, bool p_as_vector, std::pmr::memory_resource *mr = nullptr);

//// size(x) returns a row vector of dimensions.
//// @param asVector  when true, returns [rows, cols] or [rows, cols, pages].
////                  For nargout > 1 form, call sizePair or sizeTriple below.
Value size(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// size(x, dim) — scalar = dim'th dimension (1-based).
Value size(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// size(x) into separate rows/cols pair (MATLAB [r, c] = size(x)).
std::tuple<Value, Value> sizePair(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// length(x) = max of all dimensions; 0 if empty.
Value length(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// numel(x) = total element count.
Value numel(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// ndims(x) = number of dimensions (2 for matrix, 3 for 3D array).
Value ndims(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// Reshape preserving column-major element order. totalNumel must match:
//// numel(x) == rows * cols * (pages == 0 ? 1 : pages). pages == 0 means 2D output.
//// For dimension inference (MATLAB's [] placeholder), resolve in caller
//// before invoking — this function requires concrete dims.
Value reshape(const Value &x, size_t rows, size_t cols, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// ND reshape — accepts a flat dim list of arbitrary rank (≥ 1). Same
/// elem-count check as the 2D/3D form. CELL/STRING reshape past 3D is
/// not yet supported (throws m:reshape:cellND). Pointer + size so the
/// same overload composes with std::vector / std::pmr::vector / arrays.
Value reshapeND(const Value &x, const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr = nullptr);

/// 2D matrix transpose (no complex conjugation). Throws Error on 3D input.
Value transpose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Page-wise transpose. Each (rows × cols) page is transposed in place;
/// for 1-D / 2-D inputs falls back to plain transpose. Real-only at the
/// element level; complex elements are transposed without conjugation.
Value pagetranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// peaks(n) — MATLAB demo surface: sample points of the function
///   z(x,y) = 3*(1-x)^2*exp(-x^2-(y+1)^2)
///          - 10*(x/5 - x^3 - y^5)*exp(-x^2 - y^2)
///          - exp(-(x+1)^2 - y^2) / 3
/// on the n×n grid (x, y) = linspace(-3, 3, n). Default n = 49.
Value peaks(size_t n, std::pmr::memory_resource *mr = nullptr);

/// Triple-output surface generators (multi-out — caller takes the first
/// component as primary, the rest via nargout). All return three (n+1)
/// × (n+1) matrices (or rows×(n+1) for cylinder). Matches MATLAB to ULP.
struct Surface3 { Value X; Value Y; Value Z; };

/// sphere(n) — unit sphere on an (n+1) × (n+1) grid (n=20 default).
Surface3 sphere(size_t n, std::pmr::memory_resource *mr = nullptr);

/// cylinder(R, n) — surface of revolution of profile R along z ∈ [0, 1].
/// Output is length(R) × (n+1). Default n = 20. Default R = [1 1] (unit
/// cylinder of unit height).
Surface3 cylinder(const Value &R, size_t n, std::pmr::memory_resource *mr = nullptr);

/// ellipsoid(xc, yc, zc, xr, yr, zr, n) — axis-aligned ellipsoid centered
/// at (xc, yc, zc) with semi-axes (xr, yr, zr). Output (n+1) × (n+1).
Surface3 ellipsoid(double xc, double yc, double zc, double xr, double yr, double zr, size_t n, std::pmr::memory_resource *mr = nullptr);

/// Page-wise conjugate transpose. Identical to pagetranspose for real
/// inputs; for complex inputs, conjugates each element while transposing.
Value pagectranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Page-wise matrix multiply: treats axes 1-2 as M×K / K×N matrices,
/// axes ≥3 as batch dims. Output shape is [M, N, ...broadcast(batchX, batchY)].
/// DOUBLE only. Inner dim mismatch throws.
///
/// Transpose flags map MATLAB strings: "none" = no op, "transpose" =
/// per-page transpose, "ctranspose" = per-page conjugate-transpose
/// (identical to transpose for real input; complex input not yet
/// supported).
enum class TranspOp { None, Transpose, CTranspose };
Value pagemtimes(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);
Value pagemtimes(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr = nullptr);

/// Main diagonal of a matrix as a column vector, or vector → diagonal matrix.
Value diag(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// Sort along first non-singleton dimension; returns (sorted, indices).
//// Indices are 1-based permutation. For 3D input, operates per-slice.
std::tuple<Value, Value> sort(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// sortrows(M) — lex-sort the rows of a 2D matrix in ascending order
/// across all columns (column 1 most significant). Stable sort.
/// Returns (sorted, idx) where idx is the 1-based original row order.
/// `cols` form: each entry is a 1-based column index; negative entries
/// flip direction for that key (descending). Empty `cols` means "all
/// columns ascending" (same as the 1-arg form).
/// Promotes integer/logical input to DOUBLE.
std::tuple<Value, Value> sortrows(const Value &x, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> sortrows(const Value &x, const int *cols, std::size_t nCols, std::pmr::memory_resource *mr = nullptr);

/// Linear indices of non-zero (or true) entries. Result is a row vector
/// when x is a row, column vector otherwise.
Value find(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// nnz(x) — number of non-zero elements. NaN counts as non-zero
/// (NaN != 0). For COMPLEX, an element is non-zero iff real or imag
/// part is non-zero. Returns DOUBLE scalar.
Value nnz(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// nonzeros(x) — column vector of non-zero elements in column-major
/// order. Output type matches input type (DOUBLE/SINGLE/COMPLEX/INT*/
/// LOGICAL preserved).
Value nonzeros(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// Horizontal concatenation (along columns).
Value horzcat(const Value *values, size_t count, std::pmr::memory_resource *mr = nullptr);

/// Vertical concatenation (along rows).
Value vertcat(const Value *values, size_t count, std::pmr::memory_resource *mr = nullptr);

//// meshgrid(x, y) returns (X, Y) matrices of size [ny, nx].
std::tuple<Value, Value> meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// meshgrid(x, y, z) returns three (X, Y, Z) 3-D arrays of size
/// [ny, nx, nz].
std::tuple<Value, Value, Value>
meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// ndgrid(x, y) — N-D companion to meshgrid. Each output has shape
/// [numel(x), numel(y), ...] (first-arg axes-major) — the opposite
/// of meshgrid's MATLAB convention. Output type DOUBLE.
std::tuple<Value, Value>
ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// 3-input ndgrid(x, y, z) — outputs have shape [numel(x), numel(y), numel(z)].
std::tuple<Value, Value, Value>
ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// kron(A, B) — Kronecker product. Output is (rA*rB) × (cA*cB);
/// the (i, j)-th block (rB × cB) equals A[i, j] · B. Vector inputs
/// are treated as matrices of their natural orientation. DOUBLE only
/// for now (integer/logical/single promoted; complex throws).
Value kron(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// Cumulative ops keep the input shape; sum/prod/max/min along the
/// chosen dim. Two-arg form auto-detects the first non-singleton dim;
/// three-arg form takes an explicit 1-based dim (0 = auto).
Value cumsum (const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cumsum (const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value cumprod(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value cummax (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value cummin (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// diff(x[, n[, dim]]) — n-th order discrete difference along dim.
/// out[i] = x[i+1] - x[i]. Output shape: input with dim[d-1] decremented
/// by n (clamped to 0). n=0 returns a copy. Default dim = first non-
/// singleton. Scalar input returns 1×0 empty (MATLAB convention).
Value diff(const Value &x, int n = 1, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// Logical reductions: collapse the chosen dim to a single 0/1 value.
/// Empty slices: any → false, all → true (matches MATLAB).
/// Output type is LOGICAL.
Value anyOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value allOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// Elementwise xor — both inputs treated as boolean (non-zero = true).
/// Output type is LOGICAL. Standard broadcasting rules apply.
Value xorOf(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// Cross product of 3-element vectors. Row vector output.
Value cross(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// Dot product of two vectors of equal length.
Value dot(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
