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

// NOTE: inv migrated to libs/linalg (numkit/linalg/properties.hpp).

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

// NOTE: trace / det migrated to libs/linalg (numkit/linalg/properties.hpp).

// NOTE: chol migrated to libs/linalg (numkit/linalg/decompositions.hpp).

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

// NOTE: lu_decompose / lu_combined / qr_decompose / qr_R_only /
//       svd_decompose / svd_values / rank_of / pinv migrated to
//       libs/linalg (numkit/linalg/decompositions.hpp,
//       numkit/linalg/properties.hpp, numkit/linalg/pseudo_subspace.hpp).

// NOTE: cond_2norm migrated to libs/linalg (numkit/linalg/properties.hpp).

// NOTE: orth / null_basis migrated to libs/linalg (numkit/linalg/pseudo_subspace.hpp).

// NOTE: normest migrated to libs/linalg (numkit/linalg/properties.hpp).

// ── Eigenvalues / eigenvectors / matrix functions ───────────────────
//
// NOTE: eig family (eig_symmetric, eig_values, eig_general_values,
//       eig_general_VD), sylvester_sym, schur_sym, hess / hess_H_only,
//       expm, logm_sym, sqrtm_sym migrated to libs/linalg
//       (numkit/linalg/{eig,matrix_functions}.hpp).
// NOTE: subspace migrated to libs/linalg (numkit/linalg/pseudo_subspace.hpp).
// NOTE: norm_value / norm_inf / norm_fro migrated to libs/linalg
//       (numkit/linalg/norms.hpp).

/// @brief Characteristic polynomial of a matrix (`p = poly(A)`).
///
/// Souriau-Faddeev-LeVerrier. `p(λ) = λ^n + p(2)·λ^(n-1) + … + p(n+1)`
/// and `roots(p) == eig(A)`. Square inputs only.
///
/// Stays in builtin because the `poly()` builtin function dispatches
/// here for matrix input; linalg has its own copy in
/// numkit::linalg::poly_of_matrix.
Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr = nullptr);

// ── Matrix predicates ────────────────────────────────────────────────
//
// NOTE: isbanded / isdiag / istril / istriu / issymmetric / ishermitian /
//       bandwidth / bandwidthOpt migrated to libs/linalg
//       (numkit/linalg/predicates.hpp).

// NOTE: vecnorm migrated to libs/linalg (numkit/linalg/norms.hpp).

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

// NOTE: rcond migrated to libs/linalg (numkit/linalg/properties.hpp).

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

// NOTE: BalanceResult / balance_impl migrated to libs/linalg
//       (numkit/linalg/balance.hpp).
// NOTE: ldl migrated to libs/linalg (numkit/linalg/ldl.hpp).

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

// NOTE: cross / dot / kron migrated to libs/linalg (see
// numkit/linalg/vector_ops.hpp). Engine registration also moved
// from BuiltinLibrary::install → LinalgLibrary::install.

} // namespace numkit::builtin
