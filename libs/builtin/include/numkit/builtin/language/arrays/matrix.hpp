// libs/builtin/include/numkit/builtin/language/arrays/matrix.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>
#include <vector>

namespace numkit::builtin {

// ── Constructors ──────────────────────────────────────────────────────
/// All-zero matrix. pages == 0 → 2D matrix.
Value zeros(std::pmr::memory_resource *mr, size_t rows, size_t cols = 1, size_t pages = 0);
Value ones(std::pmr::memory_resource *mr, size_t rows, size_t cols = 1, size_t pages = 0);

/// Identity matrix. rows, cols may differ — produces rectangular "identity".
Value eye(std::pmr::memory_resource *mr, size_t rows, size_t cols);

/// Magic square of order N (N×N matrix where rows, columns, both
/// diagonals all sum to the magic constant N·(N²+1)/2).
/// Three branches by N's parity, matching MATLAB R2025b:
///   - N odd  (N >= 3) : Siamese / de la Loubère method
///   - N ≡ 0 mod 4     : doubly-even (4×4-block diagonal swap pattern)
///   - N ≡ 2 mod 4     : singly-even (Strachey -- 4 odd-magic quadrants
///                                    + corner swaps)
/// Edge cases: N == 0 → 0×0; N == 1 → [1]; N == 2 → MATLAB's [1 3; 4 2]
/// (not strictly magic; preserved for parity).
Value magic(std::pmr::memory_resource *mr, size_t N);

/// Toeplitz matrix from first column c (length m) and optional first
/// row r (length n). T[i, j] = c[i-j] if i >= j else r[j-i].
/// Single-arg form: r is taken as conj(c) (real input → r = c).
/// MATLAB convention: if c[0] != r[0], r[0] is silently overridden by c[0].
Value toeplitz(std::pmr::memory_resource *mr,
               const double *c, std::size_t m,
               const double *r, std::size_t n);

/// Hankel matrix from first column c (length m) and optional last
/// row r (length n). H[i, j] = c[i+j] for i+j < m, else r[i+j-m+1].
/// Single-arg form: r is all zeros (anti-triangular Hankel).
/// MATLAB convention: if c[end] != r[0], r[0] is silently overridden.
Value hankel(std::pmr::memory_resource *mr,
             const double *c, std::size_t m,
             const double *r, std::size_t n);

/// Vandermonde matrix V[i, j] = v[i]^(n-1-j) where n = numel(v).
/// Returns n×n matrix; columns from highest to lowest power
/// (matches MATLAB R2025b layout).
Value vander(std::pmr::memory_resource *mr, const double *v, std::size_t n);

/// Companion matrix of monic polynomial p (length n+1).
/// Returns n×n matrix whose top row is [-p[1]/p[0], ..., -p[n]/p[0]]
/// and whose subdiagonal is all ones (eigenvalues = roots of p).
Value compan(std::pmr::memory_resource *mr, const double *p, std::size_t pn);

/// Pascal matrix of order n. Default form (k = 0): symmetric with
/// P[i, j] = C(i+j, i). MATLAB also defines k=1 (lower-triangular
/// Cholesky factor) and k=2 (cube-root of identity); only k=0 is
/// implemented in this revision.
Value pascal(std::pmr::memory_resource *mr, size_t n);

/// Hilbert matrix of order n. H[i, j] = 1 / (i + j - 1) (1-indexed).
Value hilb(std::pmr::memory_resource *mr, size_t n);

/// Inverse Hilbert matrix of order n via the closed-form formula
/// involving binomials. Exact integer entries up to n ≈ 13; for
/// larger n the result loses accuracy due to floating-point overflow
/// in the binomial coefficients.
Value invhilb(std::pmr::memory_resource *mr, size_t n);

/// Wilkinson's eigenvalue test matrix: symmetric tridiagonal with
/// subdiagonal of ones and main diagonal = |(1:n) - (n+1)/2|.
Value wilkinson(std::pmr::memory_resource *mr, size_t n);

/// Hadamard matrix of order n via the Sylvester construction.
/// Requires n to be a power of 2 (1, 2, 4, 8, ...). Other valid
/// MATLAB orders (12·2^k, 20·2^k via Paley constructions) are
/// deferred -- see implementation note.
Value hadamard(std::pmr::memory_resource *mr, size_t n);

/// Rosser's 8×8 eigenvalue test matrix (hardcoded constants from
/// MATLAB R2025b's gallery / rosser).
Value rosser(std::pmr::memory_resource *mr);

/// Matrix inverse via LU. inv(A) ≡ A \ eye(n) -- prefer mldivide /
/// linsolve / `\` for solving A·x = b; inv exists for the cases where
/// the inverse itself is needed as a matrix.
/// @throws Error if A is non-square or singular.
Value inv(std::pmr::memory_resource *mr, const Value &A);

/// Solve A·X = B via LU (square A) or Householder QR (tall A,
/// least-squares). Wrapper over the same la_solve backend that powers
/// MATLAB's mldivide / `\`. The optional 3rd argument `opts` is
/// accepted for MATLAB-compatibility but ignored in this revision
/// (LU/QR auto-detection handles the same cases).
/// @throws Error on singular / rank-deficient / wide A.
Value linsolve(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// Page-wise inverse of a 3D array A (m×n×p). Each m×n page is
/// independently inverted via LU. Output shape matches input.
/// @throws Error if any page is non-square or singular.
Value pageinv(std::pmr::memory_resource *mr, const Value &A);

/// Sum of the diagonal elements of A. Equivalent to sum(diag(A)).
/// Works for any 2D matrix (square or rectangular).
Value trace(std::pmr::memory_resource *mr, const Value &A);

/// Determinant via LU factorisation with partial pivoting.
/// det(A) = sign(P) * prod(diag(U)) where A = P·L·U.
/// @throws Error if A is non-square.
Value det(std::pmr::memory_resource *mr, const Value &A);

/// Cholesky factorisation of a symmetric positive-definite matrix A.
/// Returns upper-triangular R such that R' * R = A
/// (matches MATLAB R2025b's chol(A) default).
/// @throws Error if A is non-square or not positive-definite.
Value chol(std::pmr::memory_resource *mr, const Value &A);

/// Top k rows of A in sort order. Default sort: descending by all
/// columns lexicographically. Single-arg form (k only) sorts on every
/// column. Single-arg form requires the matrix to have at least k rows.
Value topkrows(std::pmr::memory_resource *mr, const Value &A, std::size_t k);

/// LU decomposition of an n×n matrix A with partial pivoting:
/// returns (L, U, P) where L is unit-lower-triangular, U is
/// upper-triangular, P is the permutation matrix, and P*A == L*U.
/// MATLAB single-output form `LU = lu(A)` returns L+U combined
/// (zero diagonal of L implicit, P baked into L's row order).
/// @throws Error if A is non-square.
std::tuple<Value, Value, Value>
lu_decompose(std::pmr::memory_resource *mr, const Value &A);

/// Combined L+U output: returns a single matrix whose strict lower
/// triangle is L (unit diagonal implicit) and whose upper triangle
/// (including diagonal) is U, with rows already permuted -- matches
/// MATLAB's single-output `lu(A)` form.
Value lu_combined(std::pmr::memory_resource *mr, const Value &A);

/// QR decomposition of an m×n matrix A (m >= n) via Householder
/// reflections: returns (Q, R) where Q is m×m orthogonal and R is
/// m×n upper-triangular, with A == Q*R. Full-size form (not "econ").
/// MATLAB single-output form `R = qr(A)` returns just R.
/// @throws Error if A has more columns than rows.
std::tuple<Value, Value>
qr_decompose(std::pmr::memory_resource *mr, const Value &A);

/// R-only output -- matches MATLAB's single-output `qr(A)` form.
Value qr_R_only(std::pmr::memory_resource *mr, const Value &A);

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
svd_decompose(std::pmr::memory_resource *mr, const Value &A);

/// Singular values only -- matches MATLAB's single-output svd(A).
Value svd_values(std::pmr::memory_resource *mr, const Value &A);

/// Numerical rank: count of singular values above tolerance.
/// Default tol = max(size(A)) * eps(max(svd(A))). Two-arg form
/// rank(A, tol) takes user tolerance.
Value rank_of(std::pmr::memory_resource *mr, const Value &A, double tol = -1.0);

/// Pseudoinverse (Moore-Penrose) via SVD: pinv(A) = V * S^+ * U'
/// where S^+ inverts non-zero singular values above tolerance.
Value pinv(std::pmr::memory_resource *mr, const Value &A, double tol = -1.0);

/// 2-norm condition number: cond(A) = sigma_max / sigma_min via SVD.
/// Returns Inf for singular A.
Value cond_2norm(std::pmr::memory_resource *mr, const Value &A);

/// Orthonormal basis for the range of A (n columns of U from SVD
/// where corresponding sigma > tolerance). Output is m × rank(A).
Value orth(std::pmr::memory_resource *mr, const Value &A, double tol = -1.0);

/// Orthonormal basis for the null space of A (n - rank(A) columns of
/// V from SVD where corresponding sigma is below tolerance). Output
/// is n × (n - rank(A)).
Value null_basis(std::pmr::memory_resource *mr, const Value &A, double tol = -1.0);

/// Estimate of the 2-norm (largest singular value). Same as svd(A)(1)
/// for now (no power-iteration shortcut yet -- correctness over
/// performance).
Value normest(std::pmr::memory_resource *mr, const Value &A);

/// Eigenvalues + eigenvectors of a symmetric real matrix via
/// classical Jacobi rotations. A must be square; if not symmetric
/// within tol, throws (general eig requires Hessenberg + Francis QR
/// iteration; deferred as Phase 2b).
///
/// Returns (V, D) such that A*V == V*D, V orthogonal, D diagonal.
/// MATLAB single-output form `e = eig(A)` returns eigenvalues as a
/// column vector (ascending order for symmetric A).
std::tuple<Value, Value>
eig_symmetric(std::pmr::memory_resource *mr, const Value &A);

/// Eigenvalues only -- matches MATLAB's single-output eig(A).
Value eig_values(std::pmr::memory_resource *mr, const Value &A);

/// Matrix exponential expm(A) via Padé approximation with scaling-
/// and-squaring (Higham 2005). Works for any square matrix
/// (symmetric or not). For symmetric A could go via eig but Padé
/// is more general and still bit-stable.
Value expm(std::pmr::memory_resource *mr, const Value &A);

/// Matrix logarithm logm(A) for symmetric positive-definite A only
/// (general logm requires complex Schur, deferred to Phase 2b).
/// Computed via eigendecomposition: logm(A) = V * diag(log(eig)) * V'.
Value logm_sym(std::pmr::memory_resource *mr, const Value &A);

/// Matrix square root sqrtm(A) for symmetric positive-semidefinite A
/// only (general sqrtm needs complex Schur). Via eigendecomposition:
/// sqrtm(A) = V * diag(sqrt(eig)) * V'.
Value sqrtm_sym(std::pmr::memory_resource *mr, const Value &A);

/// Schur decomposition. For symmetric A this is equivalent to
/// the eigendecomposition: A = U*T*U' where T is diagonal (real
/// eigenvalues) and U is orthogonal. Returns (U, T).
/// General (non-symmetric) Schur returns quasi-triangular T with
/// 2×2 blocks for complex eigenpairs -- deferred to Phase 2b.
std::tuple<Value, Value>
schur_sym(std::pmr::memory_resource *mr, const Value &A);

// ── Shape queries ────────────────────────────────────────────────────
/// size(x) returns a row vector of dimensions.
/// @param asVector  when true, returns [rows, cols] or [rows, cols, pages].
///                  For nargout > 1 form, call sizePair or sizeTriple below.
Value size(std::pmr::memory_resource *mr, const Value &x);

/// size(x, dim) — scalar = dim'th dimension (1-based).
Value size(std::pmr::memory_resource *mr, const Value &x, int dim);

/// size(x) into separate rows/cols pair (MATLAB [r, c] = size(x)).
std::tuple<Value, Value> sizePair(std::pmr::memory_resource *mr, const Value &x);

/// length(x) = max of all dimensions; 0 if empty.
Value length(std::pmr::memory_resource *mr, const Value &x);

/// numel(x) = total element count.
Value numel(std::pmr::memory_resource *mr, const Value &x);

/// ndims(x) = number of dimensions (2 for matrix, 3 for 3D array).
Value ndims(std::pmr::memory_resource *mr, const Value &x);

// ── Shape transformations ────────────────────────────────────────────
/// Reshape preserving column-major element order. totalNumel must match:
/// numel(x) == rows * cols * (pages == 0 ? 1 : pages). pages == 0 means 2D output.
/// For dimension inference (MATLAB's [] placeholder), resolve in caller
/// before invoking — this function requires concrete dims.
Value reshape(std::pmr::memory_resource *mr, const Value &x, size_t rows, size_t cols, size_t pages = 0);

/// ND reshape — accepts a flat dim list of arbitrary rank (≥ 1). Same
/// elem-count check as the 2D/3D form. CELL/STRING reshape past 3D is
/// not yet supported (throws m:reshape:cellND). Pointer + size so the
/// same overload composes with std::vector / std::pmr::vector / arrays.
Value reshapeND(std::pmr::memory_resource *mr, const Value &x,
                const size_t *dims, std::size_t nDims);

/// 2D matrix transpose (no complex conjugation). Throws Error on 3D input.
Value transpose(std::pmr::memory_resource *mr, const Value &x);

/// Page-wise transpose. Each (rows × cols) page is transposed in place;
/// for 1-D / 2-D inputs falls back to plain transpose. Real-only at the
/// element level; complex elements are transposed without conjugation.
Value pagetranspose(std::pmr::memory_resource *mr, const Value &x);

/// peaks(n) — MATLAB demo surface: sample points of the function
///   z(x,y) = 3*(1-x)^2*exp(-x^2-(y+1)^2)
///          - 10*(x/5 - x^3 - y^5)*exp(-x^2 - y^2)
///          - exp(-(x+1)^2 - y^2) / 3
/// on the n×n grid (x, y) = linspace(-3, 3, n). Default n = 49.
Value peaks(std::pmr::memory_resource *mr, size_t n);

/// Triple-output surface generators (multi-out — caller takes the first
/// component as primary, the rest via nargout). All return three (n+1)
/// × (n+1) matrices (or rows×(n+1) for cylinder). Matches MATLAB to ULP.
struct Surface3 { Value X; Value Y; Value Z; };

/// sphere(n) — unit sphere on an (n+1) × (n+1) grid (n=20 default).
Surface3 sphere(std::pmr::memory_resource *mr, size_t n);

/// cylinder(R, n) — surface of revolution of profile R along z ∈ [0, 1].
/// Output is length(R) × (n+1). Default n = 20. Default R = [1 1] (unit
/// cylinder of unit height).
Surface3 cylinder(std::pmr::memory_resource *mr, const Value &R, size_t n);

/// ellipsoid(xc, yc, zc, xr, yr, zr, n) — axis-aligned ellipsoid centered
/// at (xc, yc, zc) with semi-axes (xr, yr, zr). Output (n+1) × (n+1).
Surface3 ellipsoid(std::pmr::memory_resource *mr,
                   double xc, double yc, double zc,
                   double xr, double yr, double zr,
                   size_t n);

/// Page-wise conjugate transpose. Identical to pagetranspose for real
/// inputs; for complex inputs, conjugates each element while transposing.
Value pagectranspose(std::pmr::memory_resource *mr, const Value &x);

/// Page-wise matrix multiply: treats axes 1-2 as M×K / K×N matrices,
/// axes ≥3 as batch dims. Output shape is [M, N, ...broadcast(batchX, batchY)].
/// DOUBLE only. Inner dim mismatch throws.
///
/// Transpose flags map MATLAB strings: "none" = no op, "transpose" =
/// per-page transpose, "ctranspose" = per-page conjugate-transpose
/// (identical to transpose for real input; complex input not yet
/// supported).
enum class TranspOp { None, Transpose, CTranspose };
Value pagemtimes(std::pmr::memory_resource *mr, const Value &x, const Value &y);
Value pagemtimes(std::pmr::memory_resource *mr,
                  const Value &x, TranspOp tx,
                  const Value &y, TranspOp ty);

/// Main diagonal of a matrix as a column vector, or vector → diagonal matrix.
Value diag(std::pmr::memory_resource *mr, const Value &x);

// ── Sort / find ──────────────────────────────────────────────────────
/// Sort along first non-singleton dimension; returns (sorted, indices).
/// Indices are 1-based permutation. For 3D input, operates per-slice.
std::tuple<Value, Value> sort(std::pmr::memory_resource *mr, const Value &x);

/// sortrows(M) — lex-sort the rows of a 2D matrix in ascending order
/// across all columns (column 1 most significant). Stable sort.
/// Returns (sorted, idx) where idx is the 1-based original row order.
/// `cols` form: each entry is a 1-based column index; negative entries
/// flip direction for that key (descending). Empty `cols` means "all
/// columns ascending" (same as the 1-arg form).
/// Promotes integer/logical input to DOUBLE.
std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x);
std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x,
                                    const int *cols, std::size_t nCols);

/// Linear indices of non-zero (or true) entries. Result is a row vector
/// when x is a row, column vector otherwise.
Value find(std::pmr::memory_resource *mr, const Value &x);

/// nnz(x) — number of non-zero elements. NaN counts as non-zero
/// (NaN != 0). For COMPLEX, an element is non-zero iff real or imag
/// part is non-zero. Returns DOUBLE scalar.
Value nnz(std::pmr::memory_resource *mr, const Value &x);

/// nonzeros(x) — column vector of non-zero elements in column-major
/// order. Output type matches input type (DOUBLE/SINGLE/COMPLEX/INT*/
/// LOGICAL preserved).
Value nonzeros(std::pmr::memory_resource *mr, const Value &x);

// ── Concatenation ────────────────────────────────────────────────────
/// Horizontal concatenation (along columns).
Value horzcat(std::pmr::memory_resource *mr, const Value *values, size_t count);

/// Vertical concatenation (along rows).
Value vertcat(std::pmr::memory_resource *mr, const Value *values, size_t count);

// ── Grids ────────────────────────────────────────────────────────────
/// meshgrid(x, y) returns (X, Y) matrices of size [ny, nx].
std::tuple<Value, Value> meshgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// meshgrid(x, y, z) returns three (X, Y, Z) 3-D arrays of size
/// [ny, nx, nz].
std::tuple<Value, Value, Value>
meshgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y,
         const Value &z);

/// ndgrid(x, y) — N-D companion to meshgrid. Each output has shape
/// [numel(x), numel(y), ...] (first-arg axes-major) — the opposite
/// of meshgrid's MATLAB convention. Output type DOUBLE.
std::tuple<Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y);

/// 3-input ndgrid(x, y, z) — outputs have shape [numel(x), numel(y), numel(z)].
std::tuple<Value, Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &z);

/// kron(A, B) — Kronecker product. Output is (rA*rB) × (cA*cB);
/// the (i, j)-th block (rB × cB) equals A[i, j] · B. Vector inputs
/// are treated as matrices of their natural orientation. DOUBLE only
/// for now (integer/logical/single promoted; complex throws).
Value kron(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Reductions and products ──────────────────────────────────────────
//
// Cumulative ops keep the input shape; sum/prod/max/min along the
// chosen dim. Two-arg form auto-detects the first non-singleton dim;
// three-arg form takes an explicit 1-based dim (0 = auto).
Value cumsum (std::pmr::memory_resource *mr, const Value &x);
Value cumsum (std::pmr::memory_resource *mr, const Value &x, int dim);
Value cumprod(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value cummax (std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value cummin (std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// diff(x[, n[, dim]]) — n-th order discrete difference along dim.
// out[i] = x[i+1] - x[i]. Output shape: input with dim[d-1] decremented
// by n (clamped to 0). n=0 returns a copy. Default dim = first non-
// singleton. Scalar input returns 1×0 empty (MATLAB convention).
Value diff(std::pmr::memory_resource *mr, const Value &x, int n = 1, int dim = 0);

// Logical reductions: collapse the chosen dim to a single 0/1 value.
// Empty slices: any → false, all → true (matches MATLAB).
// Output type is LOGICAL.
Value anyOf(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value allOf(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// Elementwise xor — both inputs treated as boolean (non-zero = true).
// Output type is LOGICAL. Standard broadcasting rules apply.
Value xorOf(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// Cross product of 3-element vectors. Row vector output.
Value cross(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// Dot product of two vectors of equal length.
Value dot(std::pmr::memory_resource *mr, const Value &a, const Value &b);

} // namespace numkit::builtin
