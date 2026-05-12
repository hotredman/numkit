// libs/builtin/src/lang/arrays/matrix.cpp

#include <numkit/builtin/language/arrays/matrix.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"
#include "language/operators/backends/binary_ops_loops.hpp"
#include "language/operators/la_solve.hpp"
#include "math/arithmetic/cumsum.hpp"
#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/builtin/language/arrays/manip.hpp>     // flip()

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <type_traits>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Constructors ──────────────────────────────────────────────────────
Value zeros(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
}

Value ones(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

// ND overloads: caller passes a flat dim list. For nd <= 3 these just
// route to the legacy 2D/3D ctors via createMatrixND; nd > 3 hits the
// Value::matrixND ctor and the SBO Dims storage.
Value zerosND(const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr)
{
    return createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
}

Value onesND(const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr)
{
    auto m = createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

Value eye(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto m = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < std::min(rows, cols); ++i)
        m.elem(i, i) = 1.0;
    return m;
}

namespace {

// Siamese / de la Loubère method for odd N >= 3.
// Fills positions [0..N²-1] starting at (0, N/2) and stepping
// (-1, +1) mod N; on collision step (+1, 0) instead.
void magicOdd(double *p, size_t N)
{
    size_t r = 0;
    size_t c = N / 2;
    for (size_t k = 1; k <= N * N; ++k) {
        p[r * N + c] = static_cast<double>(k);
        const size_t nr = (r == 0) ? (N - 1) : (r - 1);
        const size_t nc = (c + 1) % N;
        if (p[nr * N + nc] != 0.0) {
            r = (r + 1) % N;          // collision: drop down
        } else {
            r = nr;
            c = nc;
        }
    }
}

// Doubly-even (N ≡ 0 mod 4): start with the natural 1..N² fill and
// swap each cell whose (i mod 4, j mod 4) is on either of the two
// 4×4-block diagonals.
void magicDoublyEven(double *p, size_t N)
{
    const size_t total = N * N;
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j) {
            const size_t k = i * N + j + 1;       // 1-based natural fill
            const size_t mi = i % 4;
            const size_t mj = j % 4;
            const bool diag = (mi == mj) || (mi + mj == 3);
            p[i * N + j] = static_cast<double>(diag ? (total + 1 - k) : k);
        }
}

// Singly-even (N ≡ 2 mod 4, N >= 6) via Strachey's method.
// This mirrors MATLAB R2025b's magic.m verbatim:
//   p = N/2;  K = (N-2)/4;
//   M = [Q  Q+2p²; Q+3p²  Q+p²]   where Q = magic(p)
//   For columns j in {0..K-1, N-K+1..N-1}:
//     swap rows r ∈ {0..p-1} with rows r+p (column-by-column).
//   Then for row mid = K (0-indexed = (p-1)/2):
//     swap (mid, 0) ↔ (mid+p, 0)   -- undo the previous swap on this cell
//     swap (mid, K) ↔ (mid+p, K)   -- and apply the strached swap instead
void magicSinglyEven(double *p, size_t N)
{
    const size_t P = N / 2;          // odd
    const size_t S = P * P;
    const size_t K = (N - 2) / 4;    // num "full" left/right cols to swap

    // Build one (P×P) odd-magic and tile into four quadrants.
    std::vector<double> sq(P * P, 0.0);
    magicOdd(sq.data(), P);

    for (size_t i = 0; i < P; ++i) {
        for (size_t j = 0; j < P; ++j) {
            const double s = sq[i * P + j];
            p[(i)     * N + (j)]     = s;                    // A (top-left)
            p[(i)     * N + (j + P)] = s + 2.0 * S;          // C (top-right)
            p[(i + P) * N + (j)]     = s + 3.0 * S;          // D (bottom-left)
            p[(i + P) * N + (j + P)] = s + 1.0 * S;          // B (bottom-right)
        }
    }

    // Bulk column swaps: leftmost K and rightmost K-1 columns get
    // top-half / bottom-half rows swapped. (For N=6, K=1 → swap col 0
    // only; right side has K-1=0 cols, none.)
    auto swapRowsAtCol = [&](size_t col) {
        for (size_t r = 0; r < P; ++r)
            std::swap(p[r * N + col], p[(r + P) * N + col]);
    };
    for (size_t c = 0; c < K; ++c)
        swapRowsAtCol(c);
    for (size_t c = N - K + 1; c < N && K > 0; ++c)
        swapRowsAtCol(c);

    // Middle-row fix: for row mid = K (0-indexed), undo column-0 swap
    // and apply column-K swap instead. (MATLAB: i=k+1, j=[1, i].)
    const size_t mid = K;
    if (mid < P) {
        std::swap(p[mid * N + 0], p[(mid + P) * N + 0]);  // undo
        std::swap(p[mid * N + K], p[(mid + P) * N + K]);  // apply
    }
}

} // anonymous namespace

// ── inv / linsolve / pageinv ─────────────────────────────────────────

namespace {

// Solve A_buf (m×n column-major) against B_buf (m×nrhs col-major) and
// write the result (n×nrhs) into outX. Common helper for inv /
// linsolve / pageinv. Returns false on singular / rank-deficient /
// wide A.
bool laSolveWrap(const double *A_buf, std::size_t m, std::size_t n, const double *B_buf, std::size_t nrhs, double *outX, std::pmr::memory_resource *mr)
{
    return detail::la_solve(A_buf, m, n, B_buf, nrhs, outX, mr);
}

// Build an n×n identity into a contiguous column-major buffer.
void fillIdentity(double *buf, std::size_t n)
{
    std::fill(buf, buf + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        buf[i + i * n] = 1.0;
}

} // anonymous namespace

Value inv(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("inv: input must be a 2D matrix",
                    0, 0, "inv", "", "m:inv:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("inv: matrix must be square",
                    0, 0, "inv", "", "m:inv:notSquare");
    if (m == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    ScratchVec<double> I_buf(n * n, &scratch);
    // Copy A as column-major (Value::matrix is already col-major).
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());
    fillIdentity(I_buf.data(), n);

    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (!laSolveWrap(A_buf.data(), m, n, I_buf.data(), n, out.doubleDataMut(), &scratch))
        throw Error("inv: matrix is singular to working precision",
                    0, 0, "inv", "", "m:inv:singular");
    return out;
}

Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("linsolve: A and B must be 2D matrices",
                    0, 0, "linsolve", "", "m:linsolve:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t mb = static_cast<std::size_t>(B.dims().dim(0));
    const std::size_t nrhs = static_cast<std::size_t>(B.dims().dim(1));
    if (m != mb)
        throw Error("linsolve: A and B must have the same number of rows",
                    0, 0, "linsolve", "", "m:linsolve:badDims");

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    ScratchVec<double> B_buf(m * nrhs, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());
    std::copy(B.doubleData(), B.doubleData() + m * nrhs, B_buf.begin());

    auto out = Value::matrix(n, nrhs, ValueType::DOUBLE, mr);
    if (!laSolveWrap(A_buf.data(), m, n, B_buf.data(), nrhs, out.doubleDataMut(), &scratch))
        throw Error("linsolve: A is singular or rank-deficient",
                    0, 0, "linsolve", "", "m:linsolve:singular");
    return out;
}

Value pageinv(const Value &A, std::pmr::memory_resource *mr)
{
    const auto &dims = A.dims();
    const int nd = dims.ndim();
    if (nd < 2 || nd > 3)
        throw Error("pageinv: input must be 2D or 3D",
                    0, 0, "pageinv", "", "m:pageinv:badDim");
    const std::size_t m = static_cast<std::size_t>(dims.dim(0));
    const std::size_t n = static_cast<std::size_t>(dims.dim(1));
    if (m != n)
        throw Error("pageinv: each page must be square",
                    0, 0, "pageinv", "", "m:pageinv:notSquare");
    const std::size_t pages = (nd == 2) ? 1 : static_cast<std::size_t>(dims.dim(2));
    const std::size_t pageStride = m * n;

    auto out = (nd == 2)
        ? Value::matrix(m, n, ValueType::DOUBLE, mr)
        : createMatrix({m, n, pages}, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(pageStride, &scratch);
    ScratchVec<double> I_buf(n * n, &scratch);
    const double *src = A.doubleData();
    double *dst = out.doubleDataMut();

    for (std::size_t p = 0; p < pages; ++p) {
        std::copy(src + p * pageStride,
                  src + (p + 1) * pageStride,
                  A_buf.begin());
        fillIdentity(I_buf.data(), n);
        if (!laSolveWrap(A_buf.data(), m, n, I_buf.data(), n, dst + p * pageStride, &scratch))
            throw Error("pageinv: page is singular",
                        0, 0, "pageinv", "", "m:pageinv:singular");
    }
    return out;
}

// ── SVD (one-sided Jacobi) ───────────────────────────────────────────

namespace {

// One-sided Jacobi SVD: rotates columns of A_work (which contains a
// copy of A) until off-diagonal of A_work^T * A_work is below tol.
// On return:
//   - A_work has orthogonal columns; ||A_work(:,k)|| = sigma_k
//   - V_work holds the right singular vectors
// Caller normalises columns and assembles U, S, V.
//
// Loop convergence: standard Jacobi sweep until the largest off-
// diagonal is < tol*sqrt(diag_i*diag_j). Bounded by max_sweeps to
// prevent pathological non-convergence.
void jacobiSvdInplace(double *A, std::size_t m, std::size_t n,
                      double *V, std::size_t maxSweeps, double tol)
{
    // Initialise V = I.
    std::fill(V, V + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) V[i + i * n] = 1.0;

    auto colDot = [&](std::size_t p, std::size_t q) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i)
            s += A[i + p * m] * A[i + q * m];
        return s;
    };
    auto rotateCols = [&](double *M, std::size_t leadDim, std::size_t nrows,
                          std::size_t p, std::size_t q, double c, double s) {
        for (std::size_t i = 0; i < nrows; ++i) {
            const double mip = M[i + p * leadDim];
            const double miq = M[i + q * leadDim];
            M[i + p * leadDim] = c * mip - s * miq;
            M[i + q * leadDim] = s * mip + c * miq;
        }
    };

    for (std::size_t sweep = 0; sweep < maxSweeps; ++sweep) {
        double off = 0.0;
        for (std::size_t p = 0; p + 1 < n; ++p) {
            for (std::size_t q = p + 1; q < n; ++q) {
                const double alpha = colDot(p, p);
                const double beta  = colDot(q, q);
                const double gamma = colDot(p, q);

                const double scale = std::sqrt(alpha * beta);
                if (std::fabs(gamma) <= tol * scale) continue;
                off += gamma * gamma;

                // Compute the Jacobi rotation (c, s) that diagonalises
                // the 2×2 [[alpha gamma]; [gamma beta]] block.
                double c, s;
                if (alpha == beta) {
                    // 45-degree rotation when diagonal is symmetric.
                    c = 0.7071067811865476;  // cos(pi/4) = sqrt(2)/2
                    s = (gamma >= 0.0 ? 1.0 : -1.0) * c;
                } else {
                    const double tau = (beta - alpha) / (2.0 * gamma);
                    const double t = (tau >= 0.0)
                        ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
                        : 1.0 / (tau - std::sqrt(1.0 + tau * tau));
                    c = 1.0 / std::sqrt(1.0 + t * t);
                    s = t * c;
                }

                // Apply to columns p, q of A and V.
                rotateCols(A, m, m, p, q, c, s);
                rotateCols(V, n, n, p, q, c, s);
            }
        }
        if (off < tol * tol) break;
    }
}

} // anonymous namespace

std::tuple<Value, Value, Value>
svd_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("svd: input must be a 2D matrix",
                    0, 0, "svd", "", "m:svd:notMatrix");
    const std::size_t m_in = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n_in = static_cast<std::size_t>(A.dims().dim(1));

    if (m_in == 0 || n_in == 0) {
        return std::make_tuple(
            Value::matrix(m_in, m_in, ValueType::DOUBLE, mr),
            Value::matrix(m_in, n_in, ValueType::DOUBLE, mr),
            Value::matrix(n_in, n_in, ValueType::DOUBLE, mr));
    }

    // For m < n we transpose: SVD(A) = (U, S, V) implies SVD(A^T) =
    // (V, S^T, U). We run the algorithm on the tall (or square) form.
    const bool transposed = (m_in < n_in);
    const std::size_t m = transposed ? n_in : m_in;
    const std::size_t n = transposed ? m_in : n_in;

    ScratchArena scratch(mr);
    ScratchVec<double> A_work(m * n, &scratch);
    ScratchVec<double> V_work(n * n, &scratch);

    // Copy (and transpose if needed) into A_work.
    const double *A_data = A.doubleData();
    if (!transposed) {
        std::copy(A_data, A_data + m * n, A_work.begin());
    } else {
        // A is m_in × n_in; we want A^T (n_in × m_in) in column-major
        // = m × n where m = n_in, n = m_in.
        for (std::size_t j = 0; j < n_in; ++j)
            for (std::size_t i = 0; i < m_in; ++i)
                A_work[j + i * n_in] = A_data[i + j * m_in];
    }

    // Run Jacobi SVD: tol ~ 1e-13 typical.
    jacobiSvdInplace(A_work.data(), m, n, V_work.data(),
                     /*maxSweeps=*/64,
                     /*tol=*/1e-13);

    // Read singular values + normalise U columns.
    ScratchVec<double> sigma(n, &scratch);
    ScratchVec<std::size_t> order(n, &scratch);
    for (std::size_t k = 0; k < n; ++k) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i)
            s += A_work[i + k * m] * A_work[i + k * m];
        sigma[k] = std::sqrt(s);
        order[k] = k;
    }
    // Sort indices by descending sigma.
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return sigma[a] > sigma[b]; });

    // Assemble U (m×m), S (m×n), V (n×n) in the post-transpose frame
    // (we'll swap U <-> V at the end if we transposed).
    auto Uout = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Sout = Value::matrix(m, n, ValueType::DOUBLE, mr);
    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *U = Uout.doubleDataMut();
    double *S = Sout.doubleDataMut();
    double *V = Vout.doubleDataMut();
    std::fill(U, U + m * m, 0.0);
    std::fill(S, S + m * n, 0.0);
    std::fill(V, V + n * n, 0.0);

    // Place singular values + U columns + V columns in sorted order.
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t src = order[k];
        S[k + k * m] = sigma[src];
        if (sigma[src] > 0.0) {
            const double inv_s = 1.0 / sigma[src];
            for (std::size_t i = 0; i < m; ++i)
                U[i + k * m] = A_work[i + src * m] * inv_s;
        } else {
            // Degenerate column -- fill with zeros (will be filled by
            // orthogonal completion below).
            for (std::size_t i = 0; i < m; ++i) U[i + k * m] = 0.0;
        }
        for (std::size_t i = 0; i < n; ++i)
            V[i + k * n] = V_work[i + src * n];
    }

    // For m > n, U has only n filled columns; complete to m via
    // Gram-Schmidt against the standard basis (any orthogonal
    // completion will do; this is simple and stable for small m).
    for (std::size_t k = n; k < m; ++k) {
        // Find a basis vector e_i not yet covered, orthogonalise, normalise.
        for (std::size_t i = 0; i < m; ++i) {
            // candidate = e_i
            ScratchVec<double> v(m, 0.0, &scratch);
            v[i] = 1.0;
            // Subtract projections onto already-filled columns.
            for (std::size_t kk = 0; kk < k; ++kk) {
                double dot = 0.0;
                for (std::size_t r = 0; r < m; ++r)
                    dot += U[r + kk * m] * v[r];
                for (std::size_t r = 0; r < m; ++r)
                    v[r] -= dot * U[r + kk * m];
            }
            double nv = 0.0;
            for (std::size_t r = 0; r < m; ++r) nv += v[r] * v[r];
            if (nv > 1e-20) {
                nv = std::sqrt(nv);
                for (std::size_t r = 0; r < m; ++r)
                    U[r + k * m] = v[r] / nv;
                break;
            }
        }
    }

    if (!transposed) {
        return std::make_tuple(std::move(Uout), std::move(Sout), std::move(Vout));
    }
    // Transposed case: caller wanted SVD of A_in (m_in × n_in) where
    // m_in < n_in. We computed SVD of A_in^T = U_t * S_t * V_t', so
    // A_in = (V_t * S_t' * U_t')'. The output (U, S, V) for A_in
    // therefore has U_in = V_t (n_in × n_in -> wait, we want m_in × m_in)
    // ... actually:
    //   A_in    = m_in × n_in  (m_in < n_in)
    //   A_in^T  = n_in × m_in  (m × n with m = n_in, n = m_in)
    //   We computed A_in^T = U_t (n_in × n_in) * S_t (n_in × m_in) * V_t' (m_in × m_in)
    //   Transpose: A_in = V_t (m_in × m_in) * S_t' (m_in × n_in) * U_t' (n_in × n_in)
    //   So U_in = V_t (m_in × m_in), V_in = U_t (n_in × n_in)
    //   S_in = S_t' = m_in × n_in (we need to transpose the diagonal layout)
    auto S_out_tr = Value::matrix(m_in, n_in, ValueType::DOUBLE, mr);
    double *St = S_out_tr.doubleDataMut();
    std::fill(St, St + m_in * n_in, 0.0);
    const std::size_t k_diag = std::min(m_in, n_in);
    for (std::size_t k = 0; k < k_diag; ++k)
        St[k + k * m_in] = S[k + k * m];
    return std::make_tuple(std::move(Vout), std::move(S_out_tr), std::move(Uout));
}

Value svd_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(S.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(S.dims().dim(1));
    const std::size_t k = std::min(m, n);
    auto sv = Value::matrix(k, 1, ValueType::DOUBLE, mr);
    const double *S_data = S.doubleData();
    double *out = sv.doubleDataMut();
    for (std::size_t i = 0; i < k; ++i)
        out[i] = S_data[i + i * m];
    return sv;
}

// ── Characteristic polynomial + general eig via roots ───────────────

Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("poly: input must be a 2D matrix",
                    0, 0, "poly", "", "m:poly:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("poly: matrix must be square (use poly(roots) for vector input)",
                    0, 0, "poly", "", "m:poly:notSquare");
    if (n == 0) {
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = 1.0;
        return out;
    }

    // Souriau-Faddeev-LeVerrier: char poly p(λ) = λ^n + c[1]*λ^{n-1} + ... + c[n].
    //   M = I, c[0] = 1
    //   for k = 1..n:
    //     M = A * M + c[k-1] * I       (NOT this form; see corrected below)
    //
    // Corrected (standard form, e.g. Faddeev 1959):
    //   M_0 = 0  ;  c[0] = 1
    //   for k = 1..n:
    //     M_k = A * (M_{k-1} + c[k-1] * I)
    //          = A * M_{k-1} + c[k-1] * A
    //     c[k] = -trace(M_k) / k
    // After the loop, c[1..n] are the coefficients (after the leading 1).

    ScratchArena scratch(mr);
    ScratchVec<double> M(n * n, 0.0, &scratch);
    ScratchVec<double> Mnext(n * n, &scratch);

    auto out = Value::matrix(1, n + 1, ValueType::DOUBLE, mr);
    double *c = out.doubleDataMut();
    c[0] = 1.0;

    const double *Adata = A.doubleData();

    for (std::size_t k = 1; k <= n; ++k) {
        // Mnext = A * (M + c[k-1] * I)
        //       = A * M + c[k-1] * A
        // First: Mnext = A * M
        std::fill(Mnext.begin(), Mnext.end(), 0.0);
        for (std::size_t j = 0; j < n; ++j)
            for (std::size_t kk = 0; kk < n; ++kk) {
                const double mkj = M[kk + j * n];
                if (mkj == 0.0) continue;
                for (std::size_t i = 0; i < n; ++i)
                    Mnext[i + j * n] += Adata[i + kk * n] * mkj;
            }
        // Add c[k-1] * A
        const double cprev = c[k - 1];
        for (std::size_t i = 0; i < n * n; ++i)
            Mnext[i] += cprev * Adata[i];

        // c[k] = -trace(Mnext) / k
        double tr = 0.0;
        for (std::size_t i = 0; i < n; ++i) tr += Mnext[i + i * n];
        c[k] = -tr / static_cast<double>(k);

        std::swap(M, Mnext);
    }
    return out;
}

Value eig_general_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto p = poly_of_matrix(A, mr);
    return roots(p, mr);
}

std::tuple<Value, Value>
eig_general_VD(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("eig: input must be a 2D matrix",
                    0, 0, "eig", "", "m:eig:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (n != static_cast<std::size_t>(A.dims().dim(1)))
        throw Error("eig: matrix must be square",
                    0, 0, "eig", "", "m:eig:notSquare");

    auto eig_vals = eig_general_values(A, mr);
    const std::size_t k = eig_vals.numel();
    if (k != n)
        throw Error("eig: char-poly returned wrong number of eigenvalues",
                    0, 0, "eig", "", "m:eig:internalError");

    // Verify all real -- complex eigvecs need Francis QR (deferred).
    if (eig_vals.isComplex()) {
        const Complex *ev = eig_vals.complexData();
        for (std::size_t i = 0; i < k; ++i) {
            if (std::fabs(ev[i].imag()) > 1e-9 * (1.0 + std::fabs(ev[i].real())))
                throw Error("eig: [V, D] form for matrices with complex "
                            "eigenvalues requires Francis QR iteration "
                            "(deferred to Phase 2c-3-future). For "
                            "eigenvalues only, use 'e = eig(A)' (single output).",
                            0, 0, "eig", "", "m:eig:complexEigvecs");
        }
    }

    // Extract real eigenvalues into ScratchVec.
    ScratchArena scratch(mr);
    ScratchVec<double> evals(n, &scratch);
    if (eig_vals.isComplex()) {
        const Complex *ev = eig_vals.complexData();
        for (std::size_t i = 0; i < n; ++i) evals[i] = ev[i].real();
    } else {
        const double *ev = eig_vals.doubleData();
        for (std::size_t i = 0; i < n; ++i) evals[i] = ev[i];
    }
    // Sort ascending (matches symmetric eig output convention).
    std::sort(evals.begin(), evals.end());

    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Dout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *V = Vout.doubleDataMut();
    double *D = Dout.doubleDataMut();
    std::fill(V, V + n * n, 0.0);
    std::fill(D, D + n * n, 0.0);

    const double *Adata = A.doubleData();

    for (std::size_t k2 = 0; k2 < n; ++k2) {
        const double lam = evals[k2];
        D[k2 + k2 * n] = lam;
        // Build (A - lam*I).
        auto Ali = Value::matrix(n, n, ValueType::DOUBLE, mr);
        double *AL = Ali.doubleDataMut();
        for (std::size_t i = 0; i < n * n; ++i) AL[i] = Adata[i];
        for (std::size_t i = 0; i < n; ++i) AL[i + i * n] -= lam;
        // Right null vector = last column of V from svd(Ali).
        // Singular values are descending; smallest = last index.
        auto [Us, Ss, Vs] = svd_decompose(Ali, mr);
        const std::size_t nv = static_cast<std::size_t>(Vs.dims().dim(0));
        const double *Vsdata = Vs.doubleData();
        // Eigenvector = Vs(:, n-1) (the column corresponding to smallest sigma).
        for (std::size_t i = 0; i < n; ++i)
            V[i + k2 * n] = Vsdata[i + (nv - 1) * nv];
    }
    return std::make_tuple(std::move(Vout), std::move(Dout));
}

// ── Sylvester equation (symmetric A, B) ─────────────────────────────

Value sylvester_sym(const Value &A, const Value &B, const Value &C, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2 || C.dims().ndim() != 2)
        throw Error("sylvester: A, B, C must be 2D matrices",
                    0, 0, "sylvester", "", "m:sylvester:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t m = static_cast<std::size_t>(B.dims().dim(0));
    if (A.dims().dim(0) != A.dims().dim(1))
        throw Error("sylvester: A must be square",
                    0, 0, "sylvester", "", "m:sylvester:badA");
    if (B.dims().dim(0) != B.dims().dim(1))
        throw Error("sylvester: B must be square",
                    0, 0, "sylvester", "", "m:sylvester:badB");
    if (C.dims().dim(0) != static_cast<int>(n) ||
        C.dims().dim(1) != static_cast<int>(m))
        throw Error("sylvester: C must be n × m where A is n×n, B is m×m",
                    0, 0, "sylvester", "", "m:sylvester:badC");

    auto [Va, Da] = eig_symmetric(A, mr);   // throws if non-sym
    auto [Vb, Db] = eig_symmetric(B, mr);   // throws if non-sym

    const double *Vad = Va.doubleData();
    const double *Dad = Da.doubleData();
    const double *Vbd = Vb.doubleData();
    const double *Dbd = Db.doubleData();
    const double *Cd  = C.doubleData();

    // Y = Va' * C * Vb (n × m).
    ScratchArena scratch(mr);
    ScratchVec<double> Y(n * m, &scratch);
    ScratchVec<double> tmp(n * m, &scratch);
    // tmp = Va' * C: tmp[i, j] = sum_k Va[k, i] * C[k, j]
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vad[k + i * n] * Cd[k + j * n];
            tmp[i + j * n] = s;
        }
    // Y = tmp * Vb: Y[i, j] = sum_k tmp[i, k] * Vb[k, j]
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                s += tmp[i + k * n] * Vbd[k + j * m];
            Y[i + j * n] = s;
        }

    // y_ij /= (d_a_i + d_b_j)
    for (std::size_t i = 0; i < n; ++i) {
        const double dai = Dad[i + i * n];
        for (std::size_t j = 0; j < m; ++j) {
            const double dbj = Dbd[j + j * m];
            const double denom = dai + dbj;
            if (std::fabs(denom) < 1e-300)
                throw Error("sylvester: A and -B share an eigenvalue (no unique solution)",
                            0, 0, "sylvester", "", "m:sylvester:singular");
            Y[i + j * n] /= denom;
        }
    }

    // X = Va * Y * Vb'
    auto out = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *X = out.doubleDataMut();
    // tmp = Va * Y: tmp[i, j] = sum_k Va[i, k] * Y[k, j]
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vad[i + k * n] * Y[k + j * n];
            tmp[i + j * n] = s;
        }
    // X = tmp * Vb': X[i, j] = sum_k tmp[i, k] * Vb[j, k]
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                s += tmp[i + k * n] * Vbd[j + k * m];
            X[i + j * n] = s;
        }
    return out;
}

// ── norm (vector + matrix forms) ─────────────────────────────────────

namespace {

bool isVectorShape(const Value &x)
{
    if (x.dims().ndim() != 2) return false;
    return x.dims().dim(0) == 1 || x.dims().dim(1) == 1;
}

} // anonymous namespace

Value norm_value(const Value &x, double p, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::scalar(0.0, mr);

    if (isVectorShape(x)) {
        const std::size_t n = x.numel();
        const double *d = x.doubleData();
        if (p == 2.0) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += d[i] * d[i];
            return Value::scalar(std::sqrt(s), mr);
        } else if (p == 1.0) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += std::fabs(d[i]);
            return Value::scalar(s, mr);
        } else {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += std::pow(std::fabs(d[i]), p);
            return Value::scalar(std::pow(s, 1.0 / p), mr);
        }
    }

    // Matrix forms.
    if (x.dims().ndim() != 2)
        throw Error("norm: input must be vector or 2D matrix",
                    0, 0, "norm", "", "m:norm:badShape");
    const std::size_t m = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(x.dims().dim(1));
    const double *d = x.doubleData();

    if (p == 2.0) {
        // Largest singular value.
        auto sv = svd_values(x, mr);
        if (sv.numel() == 0) return Value::scalar(0.0, mr);
        return Value::scalar(sv.doubleData()[0], mr);
    }
    if (p == 1.0) {
        double mx = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t i = 0; i < m; ++i) s += std::fabs(d[i + j * m]);
            mx = std::max(mx, s);
        }
        return Value::scalar(mx, mr);
    }
    throw Error("norm: matrix p-norms only support 1, 2, inf, 'fro'",
                0, 0, "norm", "", "m:norm:badP");
}

Value norm_inf(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::scalar(0.0, mr);
    if (isVectorShape(x)) {
        const std::size_t n = x.numel();
        const double *d = x.doubleData();
        double mx = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            mx = std::max(mx, std::fabs(d[i]));
        return Value::scalar(mx, mr);
    }
    // Matrix inf-norm: max row sum.
    if (x.dims().ndim() != 2)
        throw Error("norm: input must be vector or 2D matrix",
                    0, 0, "norm", "", "m:norm:badShape");
    const std::size_t m = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(x.dims().dim(1));
    const double *d = x.doubleData();
    double mx = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += std::fabs(d[i + j * m]);
        mx = std::max(mx, s);
    }
    return Value::scalar(mx, mr);
}

Value norm_fro(const Value &x, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (n == 0) return Value::scalar(0.0, mr);
    const double *d = x.doubleData();
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += d[i] * d[i];
    return Value::scalar(std::sqrt(s), mr);
}

Value subspace(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("subspace: inputs must be 2D matrices",
                    0, 0, "subspace", "", "m:subspace:notMatrix");
    const std::size_t mA = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t mB = static_cast<std::size_t>(B.dims().dim(0));
    if (mA != mB)
        throw Error("subspace: inputs must have the same number of rows",
                    0, 0, "subspace", "", "m:subspace:dimMismatch");

    auto Qa = orth(A, -1.0, mr);
    auto Qb = orth(B, -1.0, mr);
    const std::size_t na = static_cast<std::size_t>(Qa.dims().dim(1));
    const std::size_t nb = static_cast<std::size_t>(Qb.dims().dim(1));
    if (na == 0 || nb == 0) return Value::scalar(0.0, mr);

    // M = Qa' * Qb (na × nb).
    ScratchArena scratch(mr);
    auto Mout = Value::matrix(na, nb, ValueType::DOUBLE, mr);
    double *M = Mout.doubleDataMut();
    const double *Qad = Qa.doubleData();
    const double *Qbd = Qb.doubleData();
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < mA; ++k)
                s += Qad[k + i * mA] * Qbd[k + j * mB];
            M[i + j * na] = s;
        }

    // SVD of M -- singular values are cosines of principal angles.
    auto s = svd_values(Mout, mr);
    const std::size_t k = s.numel();
    if (k == 0) return Value::scalar(0.0, mr);
    const double *sd = s.doubleData();
    double smin = sd[0];
    for (std::size_t i = 1; i < k; ++i)
        if (sd[i] < smin) smin = sd[i];
    if (smin > 1.0) smin = 1.0;
    if (smin < 0.0) smin = 0.0;
    return Value::scalar(std::acos(smin), mr);
}

// ── Hessenberg reduction (Phase 2c foundation) ──────────────────────

namespace {

// In-place Hessenberg reduction via Householder reflectors.
// On entry: A is n×n column-major. On exit: A's strict lower
// (below first sub-diagonal) is zeroed; upper triangle + sub-diag
// holds H. P (n×n) accumulates the orthogonal transformation:
// A_orig = P * H_final * P'.
//
// PMR HARD RULE: scratch via the supplied memory_resource. Caller
// is the public hess() / hess_H_only() which forwards its mr.
void hessReduceInplace(double *A, std::size_t n, double *P,
                       std::pmr::memory_resource *mr)
{
    std::fill(P, P + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) P[i + i * n] = 1.0;
    if (n < 3) return;  // no work needed for n=1, 2

    ScratchArena scratch(mr);
    ScratchVec<double> v_storage(n, &scratch);
    double *v = v_storage.data();

    for (std::size_t k = 0; k + 2 < n; ++k) {
        // Build Householder for column k, rows k+1..n-1.
        double norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i)
            norm_sq += A[i + k * n] * A[i + k * n];
        if (norm_sq == 0.0) continue;
        const double xk = A[k + 1 + k * n];
        const double norm = std::sqrt(norm_sq);
        const double alpha = (xk >= 0.0) ? -norm : norm;
        v[k + 1] = xk - alpha;
        for (std::size_t i = k + 2; i < n; ++i) v[i] = A[i + k * n];
        double v_norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i) v_norm_sq += v[i] * v[i];
        if (v_norm_sq == 0.0) continue;
        const double tau = 2.0 / v_norm_sq;

        // Apply H from LEFT: A[k+1:n, k:n] = (I - tau*v*v^T) * A[k+1:n, k:n]
        for (std::size_t j = k; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k + 1; i < n; ++i)
                dot += v[i] * A[i + j * n];
            const double s = tau * dot;
            for (std::size_t i = k + 1; i < n; ++i)
                A[i + j * n] -= s * v[i];
        }
        // Apply H from RIGHT: A[:, k+1:n] = A[:, k+1:n] * (I - tau*v*v^T)
        for (std::size_t i = 0; i < n; ++i) {
            double dot = 0.0;
            for (std::size_t j = k + 1; j < n; ++j)
                dot += A[i + j * n] * v[j];
            const double s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= s * v[j];
        }
        // Accumulate P: P[:, k+1:n] = P[:, k+1:n] * (I - tau*v*v^T)
        for (std::size_t i = 0; i < n; ++i) {
            double dot = 0.0;
            for (std::size_t j = k + 1; j < n; ++j)
                dot += P[i + j * n] * v[j];
            const double s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j)
                P[i + j * n] -= s * v[j];
        }
        // Set sub-diagonal entry to alpha; zero entries below.
        A[k + 1 + k * n] = alpha;
        for (std::size_t i = k + 2; i < n; ++i)
            A[i + k * n] = 0.0;
    }
}

} // anonymous namespace

std::tuple<Value, Value>
hess(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("hess: input must be a 2D matrix",
                    0, 0, "hess", "", "m:hess:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("hess: matrix must be square",
                    0, 0, "hess", "", "m:hess:notSquare");
    auto Hout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Pout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (n == 0) return std::make_tuple(std::move(Pout), std::move(Hout));
    std::copy(A.doubleData(), A.doubleData() + n * n, Hout.doubleDataMut());
    hessReduceInplace(Hout.doubleDataMut(), n, Pout.doubleDataMut(), mr);
    return std::make_tuple(std::move(Pout), std::move(Hout));
}

Value hess_H_only(const Value &A, std::pmr::memory_resource *mr)
{
    auto [P, H] = hess(A, mr);
    return H;
}

// ── Matrix functions: expm / logm / sqrtm / schur ────────────────────

namespace {

// Multiply two n×n column-major matrices: C = A * B (no aliasing).
void matMul(const double *A, const double *B, double *C, std::size_t n)
{
    std::fill(C, C + n * n, 0.0);
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t k = 0; k < n; ++k) {
            const double bkj = B[k + j * n];
            if (bkj == 0.0) continue;
            for (std::size_t i = 0; i < n; ++i)
                C[i + j * n] += A[i + k * n] * bkj;
        }
}

// 1-norm of an n×n matrix.
double mat1Norm(const double *A, std::size_t n)
{
    double mx = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += std::fabs(A[i + j * n]);
        mx = std::max(mx, s);
    }
    return mx;
}

} // anonymous namespace

Value expm(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("expm: input must be a 2D matrix",
                    0, 0, "expm", "", "m:expm:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("expm: matrix must be square",
                    0, 0, "expm", "", "m:expm:notSquare");
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Padé(6) scaling-and-squaring. Choose s such that ||A/2^s||_1 < 0.5.
    ScratchArena scratch(mr);
    ScratchVec<double> A_s(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + n * n, A_s.begin());

    const double a_norm = mat1Norm(A_s.data(), n);
    int s = 0;
    if (a_norm > 0.5) {
        s = static_cast<int>(std::ceil(std::log2(a_norm / 0.5)));
        if (s < 0) s = 0;
        const double scale = 1.0 / std::pow(2.0, s);
        for (std::size_t i = 0; i < n * n; ++i) A_s[i] *= scale;
    }

    // Padé(6) coefficients (Higham, table 10.3).
    static constexpr double pade_b[7] = {
        720.0, 360.0, 120.0, 30.0, 5.0, 1.0 / 6.0, 0.0  // unused last
    };
    // Compute powers A^2, A^4, A^6.
    ScratchVec<double> A2(n * n, &scratch);
    ScratchVec<double> A4(n * n, &scratch);
    ScratchVec<double> A6(n * n, &scratch);
    matMul(A_s.data(), A_s.data(), A2.data(), n);
    matMul(A2.data(), A2.data(), A4.data(), n);
    matMul(A2.data(), A4.data(), A6.data(), n);

    // U = A * (b1*I + b3*A^2 + b5*A^4 + (1/6)*A^6) where b's from Padé(6)
    // Simpler: use closed-form Padé(6):
    //   U = A * (b6*A^6 + b4*A^4 + b2*A^2 + b0*I) with even Padé-6 coefs
    //   V = b7*A^6 + b5*A^4 + b3*A^2 + b1*I
    // Padé(6) coefficients from Higham:
    //   c = [1, 1/2, 5/44, 1/66, 1/792, 1/15840, 1/665280]
    static constexpr double c[7] = {
        1.0, 1.0/2.0, 5.0/44.0, 1.0/66.0, 1.0/792.0, 1.0/15840.0, 1.0/665280.0
    };
    // Wait -- the simplest correct Padé(6) is Higham's table 10.4:
    //   p_m(x) = sum_{k=0..m} (2m-k)! m! / ((2m)! k! (m-k)!) x^k
    // For m=6 the coefficients are:
    //   numerator   p(x) = 1 + x/2 + 5x^2/44 + x^3/66 + x^4/792 + x^5/15840 + x^6/665280
    //   denominator q(x) = 1 - x/2 + 5x^2/44 - x^3/66 + x^4/792 - x^5/15840 + x^6/665280
    //   exp(x) ≈ p(x) / q(x)

    ScratchVec<double> P(n * n, &scratch);  // numerator
    ScratchVec<double> Q(n * n, &scratch);  // denominator
    std::fill(P.begin(), P.end(), 0.0);
    std::fill(Q.begin(), Q.end(), 0.0);
    // Initialize with identity (* c[0]).
    for (std::size_t i = 0; i < n; ++i) {
        P[i + i * n] = c[0];
        Q[i + i * n] = c[0];
    }
    // Add c[k] * A^k for k = 1..6.
    // A^1
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[1] * A_s[i];
        Q[i] -= c[1] * A_s[i];
    }
    // A^2
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[2] * A2[i];
        Q[i] += c[2] * A2[i];
    }
    // A^3 (= A * A^2)
    ScratchVec<double> A3(n * n, &scratch);
    matMul(A_s.data(), A2.data(), A3.data(), n);
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[3] * A3[i];
        Q[i] -= c[3] * A3[i];
    }
    // A^4
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[4] * A4[i];
        Q[i] += c[4] * A4[i];
    }
    // A^5 (= A * A^4)
    ScratchVec<double> A5(n * n, &scratch);
    matMul(A_s.data(), A4.data(), A5.data(), n);
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[5] * A5[i];
        Q[i] -= c[5] * A5[i];
    }
    // A^6
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[6] * A6[i];
        Q[i] += c[6] * A6[i];
    }

    // Solve Q * X = P for X (i.e. X = Q^-1 * P).
    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (!detail::la_solve(Q.data(), n, n, P.data(), n, out.doubleDataMut(), &scratch))
        throw Error("expm: Padé denominator is singular",
                    0, 0, "expm", "", "m:expm:singular");

    // Square s times.
    if (s > 0) {
        ScratchVec<double> tmp(n * n, &scratch);
        double *X = out.doubleDataMut();
        for (int k = 0; k < s; ++k) {
            matMul(X, X, tmp.data(), n);
            std::copy(tmp.begin(), tmp.end(), X);
        }
    }
    return out;
}

namespace {

// Apply scalar function f to symmetric A's eigenvalues and reconstruct:
//   result = V * diag(f(eig)) * V'
Value applyScalarFnSym(const Value &A, double (*f)(double), const char *fnName, const char *errId, std::pmr::memory_resource *mr)
{
    auto [V, D] = eig_symmetric(A, mr);
    const std::size_t n = static_cast<std::size_t>(D.dims().dim(0));
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    const double *Vdata = V.doubleData();
    const double *Ddata = D.doubleData();

    ScratchArena scratch(mr);
    ScratchVec<double> fD(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        const double e = Ddata[i + i * n];
        const double fe = f(e);
        if (!std::isfinite(fe))
            throw Error(std::string(fnName)
                        + ": eigenvalue out of domain (got "
                        + std::to_string(e) + ")",
                        0, 0, fnName, "", errId);
        fD[i] = fe;
    }

    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *R = out.doubleDataMut();
    // R[i,j] = sum_k V[i,k] * fD[k] * V[j,k]
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vdata[i + k * n] * fD[k] * Vdata[j + k * n];
            R[i + j * n] = s;
        }
    return out;
}

} // anonymous namespace

Value logm_sym(const Value &A, std::pmr::memory_resource *mr)
{
    return applyScalarFnSym(A, [](double x) { return std::log(x); }, "logm", "m:logm:negativeEigenvalue", mr);
}

Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr)
{
    return applyScalarFnSym(A, [](double x) { return std::sqrt(x); }, "sqrtm", "m:sqrtm:negativeEigenvalue", mr);
}

std::tuple<Value, Value>
schur_sym(const Value &A, std::pmr::memory_resource *mr)
{
    // For symmetric A, Schur decomposition is the same as eig:
    // A = U * T * U' where T is diagonal (real eigenvalues), U orthogonal.
    return eig_symmetric(A, mr);
}

// ── Symmetric eigenvalue problem (classical Jacobi) ─────────────────

namespace {

// Classical Jacobi for SYMMETRIC A. On entry A is n×n symmetric (we
// write to upper triangle and ignore lower). On exit A's diagonal
// holds the eigenvalues (unsorted) and V holds the eigenvectors
// (orthogonal, A_orig * V = V * diag(eigvals)).
void jacobiSymInplace(double *A, std::size_t n, double *V,
                      std::size_t maxSweeps, double tol)
{
    std::fill(V, V + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) V[i + i * n] = 1.0;
    if (n <= 1) return;

    auto offSum = [&]() {
        double s = 0.0;
        for (std::size_t p = 0; p + 1 < n; ++p)
            for (std::size_t q = p + 1; q < n; ++q)
                s += A[p + q * n] * A[p + q * n];
        return s;
    };

    for (std::size_t sweep = 0; sweep < maxSweeps; ++sweep) {
        if (offSum() < tol * tol) break;
        for (std::size_t p = 0; p + 1 < n; ++p) {
            for (std::size_t q = p + 1; q < n; ++q) {
                const double Apq = A[p + q * n];
                if (std::fabs(Apq) < 1e-30) continue;
                const double App = A[p + p * n];
                const double Aqq = A[q + q * n];
                double c, s;
                if (App == Aqq) {
                    c = 0.7071067811865476;  // sqrt(2)/2
                    s = (Apq >= 0.0 ? 1.0 : -1.0) * c;
                } else {
                    const double tau = (Aqq - App) / (2.0 * Apq);
                    const double t = (tau >= 0.0)
                        ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
                        : 1.0 / (tau - std::sqrt(1.0 + tau * tau));
                    c = 1.0 / std::sqrt(1.0 + t * t);
                    s = t * c;
                }

                // Apply J^T*A*J: rotates rows p,q AND cols p,q.
                // Update diagonal entries:
                A[p + p * n] = c * c * App - 2.0 * c * s * Apq + s * s * Aqq;
                A[q + q * n] = s * s * App + 2.0 * c * s * Apq + c * c * Aqq;
                A[p + q * n] = 0.0;
                A[q + p * n] = 0.0;
                // Update other entries in rows p, q (which by symmetry
                // also updates columns p, q on the upper triangle).
                for (std::size_t r = 0; r < n; ++r) {
                    if (r == p || r == q) continue;
                    // We track only the upper triangle: entry (min(r,X), max(r,X)).
                    auto get = [&](std::size_t a, std::size_t b) -> double & {
                        return (a < b) ? A[a + b * n] : A[b + a * n];
                    };
                    const double Arp = get(r, p);
                    const double Arq = get(r, q);
                    get(r, p) = c * Arp - s * Arq;
                    get(r, q) = s * Arp + c * Arq;
                }
                // Apply V = V * J (rotates cols p, q of V).
                for (std::size_t r = 0; r < n; ++r) {
                    const double Vrp = V[r + p * n];
                    const double Vrq = V[r + q * n];
                    V[r + p * n] = c * Vrp - s * Vrq;
                    V[r + q * n] = s * Vrp + c * Vrq;
                }
            }
        }
    }

    // Mirror upper triangle into lower for clean output.
    for (std::size_t p = 0; p + 1 < n; ++p)
        for (std::size_t q = p + 1; q < n; ++q)
            A[q + p * n] = A[p + q * n];
}

bool isSymmetric(const Value &A, double tol)
{
    if (A.dims().ndim() != 2) return false;
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n) return false;
    const double *p = A.doubleData();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) {
            const double d = std::fabs(p[i + j * n] - p[j + i * n]);
            const double s = std::max(std::fabs(p[i + j * n]),
                                       std::fabs(p[j + i * n]));
            if (d > tol * (1.0 + s)) return false;
        }
    return true;
}

} // anonymous namespace

std::tuple<Value, Value>
eig_symmetric(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("eig: input must be a 2D matrix",
                    0, 0, "eig", "", "m:eig:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("eig: matrix must be square",
                    0, 0, "eig", "", "m:eig:notSquare");
    if (!isSymmetric(A, 1e-10))
        throw Error("eig: only symmetric matrices supported in this revision "
                    "(general eig via Hessenberg + Francis QR is deferred to Phase 2b)",
                    0, 0, "eig", "", "m:eig:notSymmetric");
    if (n == 0) {
        return std::make_tuple(
            Value::matrix(0, 0, ValueType::DOUBLE, mr),
            Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }

    ScratchArena scratch(mr);
    ScratchVec<double> A_work(n * n, &scratch);
    ScratchVec<double> V_work(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + n * n, A_work.begin());
    jacobiSymInplace(A_work.data(), n, V_work.data(),
                     /*maxSweeps=*/64, /*tol=*/1e-13);

    // Sort eigenvalues ASCENDING (MATLAB convention for symmetric eig).
    ScratchVec<std::size_t> order(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) {
                  return A_work[a + a * n] < A_work[b + b * n];
              });

    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Dout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *V = Vout.doubleDataMut();
    double *D = Dout.doubleDataMut();
    std::fill(D, D + n * n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t src = order[k];
        D[k + k * n] = A_work[src + src * n];
        for (std::size_t i = 0; i < n; ++i)
            V[i + k * n] = V_work[i + src * n];
    }
    return std::make_tuple(std::move(Vout), std::move(Dout));
}

Value eig_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto [V, D] = eig_symmetric(A, mr);
    const std::size_t n = static_cast<std::size_t>(D.dims().dim(0));
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    const double *Ddata = D.doubleData();
    double *o = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) o[i] = Ddata[i + i * n];
    return out;
}

// ── SVD-dependent: rank / null / pinv / cond / orth / normest ────────

namespace {

// Compute default tolerance for rank-cutoff: max(m,n) * eps(sigma_max).
double defaultRankTol(std::size_t m, std::size_t n, double sigma_max)
{
    return static_cast<double>(std::max(m, n))
         * sigma_max
         * std::numeric_limits<double>::epsilon();
}

} // anonymous namespace

Value rank_of(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    const std::size_t k = sv.numel();
    const double *s = sv.doubleData();
    if (k == 0) return Value::scalar(0.0, mr);
    const double sigma_max = s[0];
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;
    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (s[i] > cutoff) ++r;
    return Value::scalar(static_cast<double>(r), mr);
}

Value pinv(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    // Extract sigma diagonal.
    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));

    double sigma_max = 0.0;
    for (std::size_t i = 0; i < k; ++i)
        sigma_max = std::max(sigma_max, S_data[i + i * Srows]);
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    // Build S^+ as n × m diagonal with reciprocals of sigma_i above cutoff.
    ScratchArena scratch(mr);
    ScratchVec<double> Splus(n * m, 0.0, &scratch);
    for (std::size_t i = 0; i < k; ++i) {
        const double sig = S_data[i + i * Srows];
        if (sig > cutoff)
            Splus[i + i * n] = 1.0 / sig;
    }

    // pinv(A) = V * S^+ * U' -- output is n × m.
    auto out = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *P = out.doubleDataMut();
    std::fill(P, P + n * m, 0.0);

    const double *Vdata = V.doubleData();
    const double *Udata = U.doubleData();
    const std::size_t Vrows = static_cast<std::size_t>(V.dims().dim(0));
    const std::size_t Urows = static_cast<std::size_t>(U.dims().dim(0));

    // out[i, j] = sum_a sum_b V[i, a] * Splus[a, b] * U[j, b]
    // Splus is diagonal, so sum_b reduces to b == a:
    // out[i, j] = sum_a V[i, a] * Splus[a, a] * U[j, a]
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t a = 0; a < k; ++a) {
                const double sp = Splus[a + a * n];
                if (sp == 0.0) continue;
                s += Vdata[i + a * Vrows] * sp * Udata[j + a * Urows];
            }
            P[i + j * n] = s;
        }
    }
    return out;
}

Value cond_2norm(const Value &A, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    const std::size_t k = sv.numel();
    if (k == 0) return Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
    const double *s = sv.doubleData();
    const double sigma_max = s[0];
    const double sigma_min = s[k - 1];
    if (sigma_min <= 0.0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    return Value::scalar(sigma_max / sigma_min, mr);
}

Value orth(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));
    const double sigma_max = (k > 0) ? S_data[0] : 0.0;
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (S_data[i + i * Srows] > cutoff) ++r;

    auto out = Value::matrix(m, static_cast<std::size_t>(r), ValueType::DOUBLE, mr);
    if (r == 0) return out;
    double *Q = out.doubleDataMut();
    const double *Udata = U.doubleData();
    const std::size_t Urows = static_cast<std::size_t>(U.dims().dim(0));
    for (std::size_t j = 0; j < static_cast<std::size_t>(r); ++j)
        for (std::size_t i = 0; i < m; ++i)
            Q[i + j * m] = Udata[i + j * Urows];
    return out;
}

Value null_basis(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));
    const double sigma_max = (k > 0) ? S_data[0] : 0.0;
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (S_data[i + i * Srows] > cutoff) ++r;

    const std::size_t null_dim = n - static_cast<std::size_t>(r);
    auto out = Value::matrix(n, null_dim, ValueType::DOUBLE, mr);
    if (null_dim == 0) return out;
    double *N = out.doubleDataMut();
    const double *Vdata = V.doubleData();
    const std::size_t Vrows = static_cast<std::size_t>(V.dims().dim(0));
    // Last (n - r) columns of V correspond to zero singular values.
    for (std::size_t j = 0; j < null_dim; ++j) {
        const std::size_t src = static_cast<std::size_t>(r) + j;
        for (std::size_t i = 0; i < n; ++i)
            N[i + j * n] = Vdata[i + src * Vrows];
    }
    return out;
}

Value normest(const Value &A, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    if (sv.numel() == 0) return Value::scalar(0.0, mr);
    return Value::scalar(sv.doubleData()[0], mr);
}

// ── lu / qr decompositions ───────────────────────────────────────────

namespace {

// In-place LU with partial pivoting on a column-major n×n matrix.
// On return:
//   - LU contains L (unit-lower-triangular, below diagonal) and U
//     (upper, including diagonal) packed
//   - piv[k] = row originally at position piv[k] swapped into row k
// Returns false on singular A.
bool luPivotInplace(double *LU, std::int32_t *piv, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot = k;
        double pmax = std::fabs(LU[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(LU[i + k * n]);
            if (v > pmax) { pmax = v; pivot = i; }
        }
        if (pmax == 0.0) return false;
        piv[k] = static_cast<std::int32_t>(pivot);
        if (pivot != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(LU[k + j * n], LU[pivot + j * n]);
        }
        const double inv_pivot = 1.0 / LU[k + k * n];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double factor = LU[i + k * n] * inv_pivot;
            LU[i + k * n] = factor;
            for (std::size_t j = k + 1; j < n; ++j)
                LU[i + j * n] -= factor * LU[k + j * n];
        }
    }
    return true;
}

} // anonymous namespace

std::tuple<Value, Value, Value>
lu_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("lu: input must be a 2D matrix",
                    0, 0, "lu", "", "m:lu:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("lu: square matrix required for [L,U,P] form",
                    0, 0, "lu", "", "m:lu:notSquare");

    ScratchArena scratch(mr);
    ScratchVec<double> LU(m * n, &scratch);
    ScratchVec<std::int32_t> piv(n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, LU.begin());
    if (!luPivotInplace(LU.data(), piv.data(), n))
        throw Error("lu: matrix is singular",
                    0, 0, "lu", "", "m:lu:singular");

    auto Lout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Uout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Pout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *L = Lout.doubleDataMut();
    double *U = Uout.doubleDataMut();
    double *P = Pout.doubleDataMut();
    std::fill(L, L + n * n, 0.0);
    std::fill(U, U + n * n, 0.0);
    std::fill(P, P + n * n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        L[i + i * n] = 1.0;       // unit diagonal of L
        for (std::size_t j = 0; j < i; ++j)
            L[i + j * n] = LU[i + j * n];
        for (std::size_t j = i; j < n; ++j)
            U[i + j * n] = LU[i + j * n];
    }
    // Build P from piv: start with identity, apply swaps in order.
    // P*A == L*U means P represents the row-permutation we did.
    std::vector<std::size_t> perm(n);
    for (std::size_t i = 0; i < n; ++i) perm[i] = i;
    for (std::size_t k = 0; k < n; ++k)
        std::swap(perm[k], perm[piv[k]]);
    for (std::size_t i = 0; i < n; ++i)
        P[i + perm[i] * n] = 1.0;

    return std::make_tuple(std::move(Lout), std::move(Uout), std::move(Pout));
}

Value lu_combined(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("lu: input must be a 2D matrix",
                    0, 0, "lu", "", "m:lu:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("lu: square matrix required",
                    0, 0, "lu", "", "m:lu:notSquare");
    ScratchArena scratch(mr);
    ScratchVec<std::int32_t> piv(n, &scratch);
    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *LU = out.doubleDataMut();
    std::copy(A.doubleData(), A.doubleData() + m * n, LU);
    if (!luPivotInplace(LU, piv.data(), n))
        throw Error("lu: matrix is singular",
                    0, 0, "lu", "", "m:lu:singular");
    return out;
}

namespace {

// Householder QR with explicit Q construction. Decomposes m×n A
// (m >= n) into Q (m×m orthogonal) and R (m×n upper-triangular)
// such that A = Q*R. Q built by applying Householder reflectors
// to identity from the back (LAPACK DORG2R style).
void qrFullHouseholder(const double *A_in, std::size_t m, std::size_t n, double *Qout, double *Rout, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<double> R_work(m * n, &scratch);
    ScratchVec<double> V(m * n, 0.0, &scratch);     // Householder vectors per column
    ScratchVec<double> tau(n, 0.0, &scratch);
    std::copy(A_in, A_in + m * n, R_work.begin());

    for (std::size_t k = 0; k < n; ++k) {
        // Build Householder for column k.
        double norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) {
            const double e = R_work[i + k * m];
            norm_sq += e * e;
        }
        if (norm_sq == 0.0) {
            tau[k] = 0.0;
            continue;
        }
        const double xk = R_work[k + k * m];
        const double norm = std::sqrt(norm_sq);
        const double alpha = (xk >= 0.0) ? -norm : norm;
        V[k + k * m] = xk - alpha;
        for (std::size_t i = k + 1; i < m; ++i)
            V[i + k * m] = R_work[i + k * m];
        double v_norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i)
            v_norm_sq += V[i + k * m] * V[i + k * m];
        if (v_norm_sq == 0.0) {
            R_work[k + k * m] = alpha;
            tau[k] = 0.0;
            continue;
        }
        tau[k] = 2.0 / v_norm_sq;
        // Apply H_k to R_work[k:m, k+1:n]
        for (std::size_t j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i)
                dot += V[i + k * m] * R_work[i + j * m];
            const double s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i)
                R_work[i + j * m] -= s * V[i + k * m];
        }
        // Set diagonal entry of R; zero entries below diagonal in column k.
        R_work[k + k * m] = alpha;
        for (std::size_t i = k + 1; i < m; ++i)
            R_work[i + k * m] = 0.0;
    }

    // Copy R (m×n, R_work upper triangle).
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i)
            Rout[i + j * m] = (i <= j) ? R_work[i + j * m] : 0.0;

    // Build Q = H_1 * H_2 * ... * H_n by applying Householders from the
    // back to identity. Q is m×m. Apply H_k from the LEFT to bottom-
    // right (m-k)×m block of Q.
    std::fill(Qout, Qout + m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i)
        Qout[i + i * m] = 1.0;
    for (std::size_t kk = n; kk-- > 0;) {
        const std::size_t k = kk;
        if (tau[k] == 0.0) continue;
        // For each column j in [0, m), update Q[k:m, j]:
        //   w = sum_{i=k..m-1} V[i,k] * Q[i,j]
        //   Q[i,j] -= tau[k] * V[i,k] * w  for i in k..m-1
        for (std::size_t j = 0; j < m; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i)
                dot += V[i + k * m] * Qout[i + j * m];
            const double s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i)
                Qout[i + j * m] -= s * V[i + k * m];
        }
    }
}

} // anonymous namespace

std::tuple<Value, Value>
qr_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("qr: input must be a 2D matrix",
                    0, 0, "qr", "", "m:qr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m < n)
        throw Error("qr: number of rows must be >= number of columns "
                    "(wide matrices via row-pivoted QR are deferred)",
                    0, 0, "qr", "", "m:qr:wide");
    auto Q = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto R = Value::matrix(m, n, ValueType::DOUBLE, mr);
    qrFullHouseholder(A.doubleData(), m, n, Q.doubleDataMut(), R.doubleDataMut(), mr);
    return std::make_tuple(std::move(Q), std::move(R));
}

Value qr_R_only(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("qr: input must be a 2D matrix",
                    0, 0, "qr", "", "m:qr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m < n)
        throw Error("qr: number of rows must be >= number of columns",
                    0, 0, "qr", "", "m:qr:wide");
    ScratchArena scratch(mr);
    ScratchVec<double> Q_unused(m * m, &scratch);
    auto R = Value::matrix(m, n, ValueType::DOUBLE, mr);
    qrFullHouseholder(A.doubleData(), m, n, Q_unused.data(), R.doubleDataMut(), mr);
    return R;
}

// ── trace / det / chol / topkrows ────────────────────────────────────

Value trace(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("trace: input must be a 2D matrix",
                    0, 0, "trace", "", "m:trace:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);
    double s = 0.0;
    const double *p = A.doubleData();
    for (std::size_t i = 0; i < k; ++i)
        s += p[i + i * m];
    return Value::scalar(s, mr);
}

namespace {

// In-place LU with partial pivoting on a column-major n×n matrix.
// Returns false on zero pivot (singular). On return, sign holds
// (-1)^(number of row swaps).
bool luPartialPivotInplace(double *A, std::size_t n, int &sign)
{
    sign = 1;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot = k;
        double pmax = std::fabs(A[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(A[i + k * n]);
            if (v > pmax) { pmax = v; pivot = i; }
        }
        if (pmax == 0.0) return false;
        if (pivot != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(A[k + j * n], A[pivot + j * n]);
            sign = -sign;
        }
        const double inv_pivot = 1.0 / A[k + k * n];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double factor = A[i + k * n] * inv_pivot;
            A[i + k * n] = factor;
            for (std::size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= factor * A[k + j * n];
        }
    }
    return true;
}

} // anonymous namespace

Value det(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("det: input must be a 2D matrix",
                    0, 0, "det", "", "m:det:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("det: matrix must be square",
                    0, 0, "det", "", "m:det:notSquare");
    if (m == 0)
        return Value::scalar(1.0, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());

    int sign = 1;
    if (!luPartialPivotInplace(A_buf.data(), n, sign))
        return Value::scalar(0.0, mr);

    long double prod = static_cast<long double>(sign);
    for (std::size_t i = 0; i < n; ++i)
        prod *= static_cast<long double>(A_buf[i + i * n]);
    return Value::scalar(static_cast<double>(prod), mr);
}

Value chol(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("chol: input must be a 2D matrix",
                    0, 0, "chol", "", "m:chol:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("chol: matrix must be square",
                    0, 0, "chol", "", "m:chol:notSquare");
    if (m == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Build upper-triangular R such that R' * R = A. Standard
    // Cholesky in column-major (MATLAB chol returns R upper).
    auto R = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *r = R.doubleDataMut();
    std::fill(r, r + n * n, 0.0);
    const double *a = A.doubleData();

    for (std::size_t j = 0; j < n; ++j) {
        // R(j,j) = sqrt(A(j,j) - sum(R(0..j-1, j)^2))
        double s = a[j + j * n];
        for (std::size_t k = 0; k < j; ++k)
            s -= r[k + j * n] * r[k + j * n];
        if (s <= 0.0)
            throw Error("chol: matrix is not positive-definite",
                        0, 0, "chol", "", "m:chol:notPosDef");
        r[j + j * n] = std::sqrt(s);
        const double inv_diag = 1.0 / r[j + j * n];
        // R(j, i) for i > j: (A(j,i) - sum(R(0..j-1, j)*R(0..j-1, i))) / R(j,j)
        for (std::size_t i = j + 1; i < n; ++i) {
            double t = a[j + i * n];
            for (std::size_t k = 0; k < j; ++k)
                t -= r[k + j * n] * r[k + i * n];
            r[j + i * n] = t * inv_diag;
        }
    }
    return R;
}

Value topkrows(const Value &A, std::size_t k, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("topkrows: input must be a 2D matrix",
                    0, 0, "topkrows", "", "m:topkrows:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t kk = std::min(k, m);

    // Build a row-index vector and sort it by lexicographic-descending
    // comparison of A's rows on every column.
    ScratchArena scratch(mr);
    ScratchVec<std::size_t> idx(m, &scratch);
    for (std::size_t i = 0; i < m; ++i) idx[i] = i;
    const double *p = A.doubleData();
    std::sort(idx.begin(), idx.end(),
              [&](std::size_t a, std::size_t b) {
                  for (std::size_t j = 0; j < n; ++j) {
                      const double va = p[a + j * m];
                      const double vb = p[b + j * m];
                      if (va > vb) return true;
                      if (va < vb) return false;
                  }
                  return false;  // tie
              });

    auto out = Value::matrix(kk, n, ValueType::DOUBLE, mr);
    double *q = out.doubleDataMut();
    for (std::size_t i = 0; i < kk; ++i)
        for (std::size_t j = 0; j < n; ++j)
            q[i + j * kk] = p[idx[i] + j * m];
    return out;
}

// ── Toeplitz / Hankel / Vandermonde / Companion ─────────────────────

Value toeplitz(const double *c, std::size_t m, const double *r, std::size_t n, std::pmr::memory_resource *mr)
{
    if (m == 0 || n == 0)
        return Value::matrix(m, n, ValueType::DOUBLE, mr);
    auto M = Value::matrix(m, n, ValueType::DOUBLE, mr);
    // T[i, j] = c[i-j]  (i >= j)
    //        = r[j-i]  (i <  j)
    // MATLAB silently overrides r[0] with c[0] when both are given;
    // caller's r[0] is ignored.
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < m; ++i)
            M.elem(i, j) = (i >= j) ? c[i - j] : r[j - i];
    return M;
}

Value hankel(const double *c, std::size_t m, const double *r, std::size_t n, std::pmr::memory_resource *mr)
{
    if (m == 0 || n == 0)
        return Value::matrix(m, n, ValueType::DOUBLE, mr);
    auto M = Value::matrix(m, n, ValueType::DOUBLE, mr);
    // H[i, j] = c[i + j]                       if i + j <  m
    //         = r[i + j - m + 1]               otherwise
    // (i, j 0-indexed; r index also 0-indexed -- offset reflects the
    // overlap cell c[m-1] == r[0] which MATLAB enforces by overriding
    // r[0]).
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < m; ++i) {
            const size_t s = i + j;
            M.elem(i, j) = (s < m) ? c[s] : r[s - m + 1];
        }
    return M;
}

Value vander(const double *v, std::size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // V[i, j] = v[i] ^ (n - 1 - j) -- highest power on the LEFT.
    // Build per-row to keep the powers in a single multiply.
    for (size_t i = 0; i < n; ++i) {
        const double x = v[i];
        // Last column = v^0 = 1; walk right→left multiplying by x.
        M.elem(i, n - 1) = 1.0;
        for (size_t k = 1; k < n; ++k)
            M.elem(i, n - 1 - k) = M.elem(i, n - k) * x;
    }
    return M;
}

Value compan(const double *p, std::size_t pn, std::pmr::memory_resource *mr)
{
    if (pn < 2)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (p[0] == 0.0)
        throw Error("compan: leading coefficient must be non-zero",
                    0, 0, "compan", "", "m:compan:zeroLead");

    const std::size_t n = pn - 1;
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    const double inv = 1.0 / p[0];
    // Top row: -p[1]/p[0], -p[2]/p[0], ..., -p[n]/p[0]
    for (std::size_t j = 0; j < n; ++j)
        M.elem(0, j) = -p[j + 1] * inv;
    // Subdiagonal: ones at (i, i-1) for i = 1..n-1
    for (std::size_t i = 1; i < n; ++i)
        M.elem(i, i - 1) = 1.0;
    return M;
}

// ── Pascal / Hilbert / Wilkinson / Hadamard / Rosser ────────────────

Value pascal(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Default symmetric form: P[i, j] = C(i+j, i). Build via the
    // recurrence P[i,j] = P[i-1,j] + P[i,j-1] with P[0,*]=P[*,0]=1.
    for (size_t i = 0; i < n; ++i) {
        M.elem(i, 0) = 1.0;
        M.elem(0, i) = 1.0;
    }
    for (size_t i = 1; i < n; ++i)
        for (size_t j = 1; j < n; ++j)
            M.elem(i, j) = M.elem(i - 1, j) + M.elem(i, j - 1);
    return M;
}

Value hilb(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            M.elem(i, j) = 1.0 / static_cast<double>(i + j + 1);
    return M;
}

Value invhilb(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Closed-form (1-indexed): H⁻¹[i,j] =
    //   (-1)^(i+j) * (i+j-1) * C(n+i-1, n-j) * C(n+j-1, n-i) * C(i+j-2, i-1)²
    // Computed via long-double to delay overflow on n ≈ 13.
    auto binom = [](long n_, long k_) -> long double {
        if (k_ < 0 || k_ > n_) return 0.0L;
        if (k_ > n_ - k_) k_ = n_ - k_;
        long double r = 1.0L;
        for (long t = 1; t <= k_; ++t)
            r = r * static_cast<long double>(n_ - t + 1) / static_cast<long double>(t);
        return r;
    };
    for (size_t i0 = 0; i0 < n; ++i0)
        for (size_t j0 = 0; j0 < n; ++j0) {
            const long i = static_cast<long>(i0 + 1);
            const long j = static_cast<long>(j0 + 1);
            const long N = static_cast<long>(n);
            const long sgn = ((i + j) % 2 == 0) ? 1 : -1;
            const long double v =
                static_cast<long double>(sgn) *
                static_cast<long double>(i + j - 1) *
                binom(N + i - 1, N - j) *
                binom(N + j - 1, N - i) *
                binom(i + j - 2, i - 1) *
                binom(i + j - 2, i - 1);
            M.elem(i0, j0) = static_cast<double>(v);
        }
    return M;
}

Value wilkinson(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Diagonal: |(1:n) - (n+1)/2|; subdiagonal/superdiagonal: ones.
    const double mid = (static_cast<double>(n) + 1.0) / 2.0;
    for (size_t i = 0; i < n; ++i) {
        M.elem(i, i) = std::abs(static_cast<double>(i + 1) - mid);
        if (i > 0) {
            M.elem(i, i - 1) = 1.0;
            M.elem(i - 1, i) = 1.0;
        }
    }
    return M;
}

Value hadamard(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (n == 1) {
        auto M = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        M.elem(0, 0) = 1.0;
        return M;
    }
    // Verify n is a power of 2 (Sylvester only). 12·2^k and 20·2^k
    // are valid MATLAB orders too -- those use Paley I/II constructions
    // and are deferred (see header).
    if ((n & (n - 1)) != 0)
        throw Error("hadamard: only powers of 2 are supported in this revision (12·2^k and 20·2^k via Paley are deferred)",
                    0, 0, "hadamard", "", "m:hadamard:badN");

    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Sylvester recursion: H_1 = [1]; H_{2k} = [Hk Hk; Hk -Hk].
    M.elem(0, 0) = 1.0;
    for (size_t k = 1; k < n; k <<= 1) {
        // Quadrant copies for size doubling from k → 2k.
        for (size_t i = 0; i < k; ++i)
            for (size_t j = 0; j < k; ++j) {
                const double v = M.elem(i, j);
                M.elem(i,     j + k) =  v;
                M.elem(i + k, j)     =  v;
                M.elem(i + k, j + k) = -v;
            }
    }
    return M;
}

Value rosser(std::pmr::memory_resource *mr)
{
    // Hardcoded 8×8 Rosser test matrix (MATLAB R2025b: rosser()).
    static constexpr double R[64] = {
         611,   196, -192,  407,   -8,  -52,  -49,   29,
         196,   899,  113, -192,  -71,  -43,   -8,  -44,
        -192,   113,  899,  196,   61,   49,    8,   52,
         407,  -192,  196,  611,    8,   44,   59,  -23,
          -8,   -71,   61,    8,  411, -599,  208,  208,
         -52,   -43,   49,   44, -599,  411,  208,  208,
         -49,    -8,    8,   59,  208,  208,   99, -911,
          29,   -44,   52,  -23,  208,  208, -911,   99
    };
    auto M = Value::matrix(8, 8, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 8; ++i)
        for (size_t j = 0; j < 8; ++j)
            M.elem(i, j) = R[i * 8 + j];
    return M;
}

Value magic(size_t N, std::pmr::memory_resource *mr)
{
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (N == 1) {
        auto m = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        m.doubleDataMut()[0] = 1.0;
        return m;
    }
    if (N == 2) {
        // MATLAB's magic(2) is conventional [1 3; 4 2] (not strictly magic).
        auto m = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *p = m.doubleDataMut();
        // column-major layout: column 0 = {1, 4}, column 1 = {3, 2}
        p[0] = 1.0; p[1] = 4.0; p[2] = 3.0; p[3] = 2.0;
        return m;
    }

    auto m = Value::matrix(N, N, ValueType::DOUBLE, mr);
    // Build into a row-major scratch then transpose into column-major
    // storage. Algorithms above are written in row-major for clarity.
    std::vector<double> buf(N * N, 0.0);
    if (N % 2 == 1) {
        magicOdd(buf.data(), N);
    } else if (N % 4 == 0) {
        magicDoublyEven(buf.data(), N);
    } else {
        magicSinglyEven(buf.data(), N);
    }
    // Row-major buf[i*N + j] → column-major out[j*N + i]
    double *out = m.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j)
            out[j * N + i] = buf[i * N + j];
    return m;
}

// ── Shape queries ────────────────────────────────────────────────────
Value size(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dims = x.dims();
    // Output ndim: at least 2 (MATLAB convention — a row vector reports
    // [1, n], not [n]). Otherwise the actual rank, including any extra
    // dims past 3.
    const int n = std::max(2, dims.ndim());
    auto sv = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *out = sv.doubleDataMut();
    for (int i = 0; i < n; ++i)
        out[i] = static_cast<double>(dims.dim(i));
    return sv;
}

Value size(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.dims().dimSize(dim - 1)), mr);
}

std::tuple<Value, Value> sizePair(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dims = x.dims();
    return std::make_tuple(
        Value::scalar(static_cast<double>(dims.rows()), mr),
        Value::scalar(static_cast<double>(dims.cols()), mr));
}

Value length(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isEmpty() || x.numel() == 0)
        return Value::scalar(0.0, mr);
    const auto &dims = x.dims();
    const double len = static_cast<double>(std::max({dims.rows(), dims.cols(), dims.pages()}));
    return Value::scalar(len, mr);
}

Value numel(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.numel()), mr);
}

Value ndims(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.dims().ndims()), mr);
}

// ── Shape transformations ────────────────────────────────────────────
Value reshape(const Value &x, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    const size_t newNumel = rows * cols * (pages == 0 ? 1 : pages);
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    DimsArg d{rows, cols, pages};

    // CELL and STRING store element-wise, not in the raw buffer — memcpy
    // wouldn't copy Value members.
    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        const bool is3D = d.pages > 0;
        Value r = (x.type() == ValueType::CELL)
            ? (is3D ? Value::cell3D(d.rows, d.cols, d.pages)
                    : Value::cell(d.rows, d.cols))
            : (is3D ? Value::stringArray3D(d.rows, d.cols, d.pages)
                    : Value::stringArray(d.rows, d.cols));
        auto &src = x.cellDataVec();
        auto &dst = r.cellDataVec();
        for (size_t i = 0; i < src.size() && i < dst.size(); ++i)
            dst[i] = src[i];
        return r;
    }

    auto r = createMatrix(d, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

// ND reshape. Same elem-count check, then route to matrixND for nd > 3.
// CELL/STRING ND not supported yet (matches the 2D/3D behaviour: only
// CELL/STRING currently handles 2D and 3D shapes via cell3D/stringArray3D).
Value reshapeND(const Value &x, const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr)
{
    size_t newNumel = 1;
    for (std::size_t i = 0; i < nDims; ++i) newNumel *= dims[i];
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        if (nDims > 3)
            throw Error("reshape: ND CELL/STRING (>3) not yet supported",
                         0, 0, "reshape", "", "m:reshape:cellND");
        // Fall through to legacy path for 2D / 3D cell.
        const size_t r = nDims > 0 ? dims[0] : 1;
        const size_t c = nDims > 1 ? dims[1] : 1;
        const size_t p = nDims > 2 ? dims[2] : 0;
        return reshape(x, r, c, p, mr);
    }

    auto r = createMatrixND(dims, nDims, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

Value transpose(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    auto r = Value::matrix(cols, rows, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            r.elem(j, i) = x(i, j);
    return r;
}

// ── pagetranspose / pagectranspose ───────────────────────────────────
namespace {

// Per-page transpose helper. `conjugate` flips the sign of imaginary
// parts when input is COMPLEX. For DOUBLE / SINGLE inputs the flag is
// ignored at the element level (no-op).
template <typename T>
Value pageTransposeT(const Value &x, ValueType ty, bool conjugate, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const size_t M = d.rows(), N = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1u;

    auto out = (P == 1u)
        ? Value::matrix(N, M, ty, mr)
        : Value::matrix3d(N, M, P, ty, mr);

    const T *src = static_cast<const T *>(x.rawData());
    T *dst       = static_cast<T *>(out.rawDataMut());
    const size_t pageInElems  = M * N;
    const size_t pageOutElems = N * M;

    for (size_t p = 0; p < P; ++p) {
        const T *sp = src + p * pageInElems;
        T *dp       = dst + p * pageOutElems;
        for (size_t j = 0; j < N; ++j) {
            for (size_t i = 0; i < M; ++i) {
                if constexpr (std::is_same_v<T, Complex>) {
                    Complex v = sp[j * M + i];
                    dp[i * N + j] = conjugate ? std::conj(v) : v;
                } else {
                    (void)conjugate;
                    dp[i * N + j] = sp[j * M + i];
                }
            }
        }
    }
    return out;
}

Value pageTransposeAny(const Value &x, bool conjugate, std::pmr::memory_resource *mr)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return pageTransposeT<double>(x, ValueType::DOUBLE, conjugate, mr);
    case ValueType::SINGLE:  return pageTransposeT<float>(x, ValueType::SINGLE, conjugate, mr);
    case ValueType::COMPLEX: return pageTransposeT<Complex>(x, ValueType::COMPLEX, conjugate, mr);
    case ValueType::INT8:    return pageTransposeT<int8_t>(x, ValueType::INT8, conjugate, mr);
    case ValueType::INT16:   return pageTransposeT<int16_t>(x, ValueType::INT16, conjugate, mr);
    case ValueType::INT32:   return pageTransposeT<int32_t>(x, ValueType::INT32, conjugate, mr);
    case ValueType::INT64:   return pageTransposeT<int64_t>(x, ValueType::INT64, conjugate, mr);
    case ValueType::UINT8:   return pageTransposeT<uint8_t>(x, ValueType::UINT8, conjugate, mr);
    case ValueType::UINT16:  return pageTransposeT<uint16_t>(x, ValueType::UINT16, conjugate, mr);
    case ValueType::UINT32:  return pageTransposeT<uint32_t>(x, ValueType::UINT32, conjugate, mr);
    case ValueType::UINT64:  return pageTransposeT<uint64_t>(x, ValueType::UINT64, conjugate, mr);
    case ValueType::LOGICAL: return pageTransposeT<uint8_t>(x, ValueType::LOGICAL, conjugate, mr);
    default:
        throw Error("pagetranspose: unsupported input type",
                     0, 0, "pagetranspose", "", "m:pagetranspose:badType");
    }
}

} // namespace

Value pagetranspose(const Value &x, std::pmr::memory_resource *mr)
{
    return pageTransposeAny(x, /*conjugate=*/false, mr);
}

Value pagectranspose(const Value &x, std::pmr::memory_resource *mr)
{
    return pageTransposeAny(x, /*conjugate=*/true, mr);
}

// ── sphere / cylinder / ellipsoid ───────────────────────────────────
Surface3 sphere(size_t n, std::pmr::memory_resource *mr)
{
    // Match MATLAB's parametrisation exactly:
    //   theta = (-n:2:n)/n * pi          → linspace(-π,  π,  n+1)
    //   phi   = (-n:2:n)/n * pi/2        → linspace(-π/2, π/2, n+1)
    //   cosphi(end-points) := 0          (clamp at the poles)
    //   sintheta(end-points) := 0        (clamp θ at ±π)
    //   X = cos(phi) * cos(theta)
    //   Y = cos(phi) * sin(theta)
    //   Z = sin(phi) * ones(1, n+1)
    constexpr double kPi = 3.14159265358979323846;
    const size_t m = n + 1;
    auto X = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Z = Value::matrix(m, m, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();

    if (n == 0) return { std::move(X), std::move(Y), std::move(Z) };

    std::vector<double> cosPhi(m), sinPhi(m);
    for (size_t i = 0; i < m; ++i) {
        const double phi = (2.0 * static_cast<double>(i) - static_cast<double>(n))
                              / static_cast<double>(n) * (kPi / 2.0);
        cosPhi[i] = std::cos(phi);
        sinPhi[i] = std::sin(phi);
    }
    cosPhi[0] = 0.0;
    cosPhi[m - 1] = 0.0;

    std::vector<double> cosTh(m), sinTh(m);
    for (size_t j = 0; j < m; ++j) {
        const double theta = (2.0 * static_cast<double>(j) - static_cast<double>(n))
                                / static_cast<double>(n) * kPi;
        cosTh[j] = std::cos(theta);
        sinTh[j] = std::sin(theta);
    }
    sinTh[0] = 0.0;
    sinTh[m - 1] = 0.0;

    for (size_t j = 0; j < m; ++j) {
        for (size_t i = 0; i < m; ++i) {
            const size_t k = j * m + i;
            xd[k] = cosPhi[i] * cosTh[j];
            yd[k] = cosPhi[i] * sinTh[j];
            zd[k] = sinPhi[i];
        }
    }
    return { std::move(X), std::move(Y), std::move(Z) };
}

Surface3 cylinder(const Value &R, size_t n, std::pmr::memory_resource *mr)
{
    // Profile R is a 1-D vector of radii along z. Output is length(R) ×
    // (n+1). z is linspace(0, 1, length(R)) repeated across columns.
    constexpr double kPi = 3.14159265358979323846;
    const size_t rows = R.numel();
    if (rows == 0) {
        auto Z = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return { Z, Z, Z };
    }

    const size_t cols = n + 1;
    auto X = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    auto Z = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();

    const double *Rd = R.doubleData();
    for (size_t j = 0; j < cols; ++j) {
        const double theta = 2.0 * kPi * static_cast<double>(j) / static_cast<double>(n);
        const double ct = std::cos(theta), st = std::sin(theta);
        for (size_t i = 0; i < rows; ++i) {
            const size_t k = j * rows + i;
            xd[k] = Rd[i] * ct;
            yd[k] = Rd[i] * st;
            zd[k] = (rows == 1) ? 0.0
                                 : static_cast<double>(i) / static_cast<double>(rows - 1);
        }
    }
    return { std::move(X), std::move(Y), std::move(Z) };
}

Surface3 ellipsoid(double xc, double yc, double zc, double xr, double yr, double zr, size_t n, std::pmr::memory_resource *mr)
{
    // Same parametrisation as sphere, scaled by (xr, yr, zr) and shifted.
    auto sph = sphere(n, mr);
    const size_t total = sph.X.numel();
    double *xd = sph.X.doubleDataMut();
    double *yd = sph.Y.doubleDataMut();
    double *zd = sph.Z.doubleDataMut();
    for (size_t k = 0; k < total; ++k) {
        xd[k] = xc + xr * xd[k];
        yd[k] = yc + yr * yd[k];
        zd[k] = zc + zr * zd[k];
    }
    return sph;
}

// ── peaks ────────────────────────────────────────────────────────────
Value peaks(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto Z = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *zd = Z.doubleDataMut();

    // x, y = linspace(-3, 3, n) on a meshgrid: X varies along columns,
    // Y along rows (so row i → y, col j → x — matches MATLAB peaks).
    const double step = (n > 1) ? 6.0 / static_cast<double>(n - 1) : 0.0;
    for (size_t j = 0; j < n; ++j) {  // column index → x axis
        const double x = (n > 1) ? -3.0 + step * static_cast<double>(j) : 0.0;
        for (size_t i = 0; i < n; ++i) {  // row index → y axis
            const double y = (n > 1) ? -3.0 + step * static_cast<double>(i) : 0.0;
            // MATLAB peaks formula (Press et al., adapted from MATLAB source).
            const double xm1   = 1.0 - x;
            const double yp1   = y + 1.0;
            const double term1 = 3.0 * xm1 * xm1
                                  * std::exp(-(x * x) - yp1 * yp1);
            const double term2 = -10.0 * (x / 5.0 - x * x * x - y * y * y * y * y)
                                  * std::exp(-(x * x) - (y * y));
            const double xp1   = x + 1.0;
            const double term3 = -std::exp(-(xp1 * xp1) - (y * y)) / 3.0;
            zd[j * n + i] = term1 + term2 + term3;
        }
    }
    return Z;
}

// ── pagemtimes: page-wise matrix multiply ──────────────────────────────
//
// MATLAB R2020b+ batched matmul. Treats axes 1-2 of each operand as the
// matrix (M×K, K×N) and axes ≥3 as a batch index. Output batch shape is
// the NumPy broadcast of the two batch shapes. Supports DOUBLE and
// SINGLE (mixed → SINGLE, matching MATLAB's promotion rule).
//
//   Z = pagemtimes(X, Y)                     // tx = ty = None
//   Z = pagemtimes(X, "transpose", Y, "none")
//
// 'transpose' / 'ctranspose' transpose each X (or Y) page before
// multiply. For real input the two flags are identical (no imaginary
// component to conjugate). 2D × 2D collapses to ordinary matmul. One
// operand may be 2D (broadcast across the other's batch dims).

namespace {

// Per-page matmul kernel, parameterised by element type. The DOUBLE
// specialisation hands off to the SIMD-aware matmulDoubleLoop in the
// backend; the SINGLE one uses the same (j, k, i) ordering as a
// portable inline loop.
template <typename T>
inline void runPageMatmul(const T *, const T *, T *,
                          size_t, size_t, size_t);

template <>
inline void runPageMatmul<double>(const double *a, const double *b, double *c,
                                  size_t M, size_t N, size_t K)
{
    detail::matmulDoubleLoop(a, b, c, M, N, K);
}

template <>
inline void runPageMatmul<float>(const float *a, const float *b, float *c,
                                 size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        float *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = 0.0f;
        for (size_t k = 0; k < K; ++k) {
            const float bkj = b[j * K + k];
            const float *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <>
inline void runPageMatmul<Complex>(const Complex *a, const Complex *b, Complex *c,
                                   size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        Complex *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = Complex(0.0, 0.0);
        for (size_t k = 0; k < K; ++k) {
            const Complex bkj = b[j * K + k];
            const Complex *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <typename T> constexpr ValueType pagemtimesElemMType();
template <> constexpr ValueType pagemtimesElemMType<double >() { return ValueType::DOUBLE;  }
template <> constexpr ValueType pagemtimesElemMType<float  >() { return ValueType::SINGLE;  }
template <> constexpr ValueType pagemtimesElemMType<Complex>() { return ValueType::COMPLEX; }

// Read element i of `src` as T. For T = Complex, real-typed sources
// upgrade to (real, 0); for T ∈ {double, float}, complex sources are
// rejected upstream so we never reach the if-branch with COMPLEX input.
template <typename T>
inline T readElemAsT(const Value &src, size_t i, bool typeMatches)
{
    if constexpr (std::is_same_v<T, Complex>) {
        if (typeMatches) return src.complexData()[i];
        return Complex(src.elemAsDouble(i), 0.0);
    } else {
        if (typeMatches) return static_cast<const T *>(src.rawData())[i];
        return static_cast<T>(src.elemAsDouble(i));
    }
}

// Conjugate a value if T is Complex; identity for real T.
template <typename T>
inline T conjIfComplex(T v)
{
    if constexpr (std::is_same_v<T, Complex>) return std::conj(v);
    else return v;
}

// Materialise one page from `src` into typed scratch `dst`, optionally
// transposing (and conjugating, for ctranspose on Complex). Direct copy
// (no per-element conversion) when src already holds the target type
// AND no transpose is needed.
template <typename T>
void materialisePage(T *dst, const Value &src, size_t pageOff,
                     size_t rowDim, size_t colDim, TranspOp tr)
{
    const size_t pageElems = rowDim * colDim;
    const size_t base = pageOff * pageElems;
    const bool typeMatches = (src.type() == pagemtimesElemMType<T>());

    if (tr == TranspOp::None) {
        if (typeMatches) {
            std::memcpy(dst, static_cast<const T *>(src.rawData()) + base,
                        pageElems * sizeof(T));
        } else {
            for (size_t i = 0; i < pageElems; ++i)
                dst[i] = readElemAsT<T>(src, base + i, false);
        }
        return;
    }
    // Transpose: dst is colDim × rowDim col-major;
    // dst[r * colDim + c] = src[c * rowDim + r] (then conjugate if ctranspose+Complex).
    const bool needsConj = (tr == TranspOp::CTranspose);
    for (size_t r = 0; r < rowDim; ++r) {
        for (size_t c = 0; c < colDim; ++c) {
            const size_t srcOff = base + c * rowDim + r;
            T v = readElemAsT<T>(src, srcOff, typeMatches);
            if (needsConj) v = conjIfComplex<T>(v);
            dst[r * colDim + c] = v;
        }
    }
}

template <typename T>
Value pagemtimesImpl(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr)
{
    const auto &xd = x.dims();
    const auto &yd = y.dims();
    const int xnd = xd.ndim();
    const int ynd = yd.ndim();
    if (xnd < 2 || ynd < 2)
        throw Error("pagemtimes: each input must have at least 2 dimensions",
                     0, 0, "pagemtimes", "", "m:pagemtimes:rank");

    const size_t xRowDim = xd.dim(0), xColDim = xd.dim(1);
    const size_t yRowDim = yd.dim(0), yColDim = yd.dim(1);

    const size_t M  = (tx == TranspOp::None) ? xRowDim : xColDim;
    const size_t Kx = (tx == TranspOp::None) ? xColDim : xRowDim;
    const size_t Ky = (ty == TranspOp::None) ? yRowDim : yColDim;
    const size_t N  = (ty == TranspOp::None) ? yColDim : yRowDim;
    if (Kx != Ky)
        throw Error("pagemtimes: inner matrix dimensions must agree",
                     0, 0, "pagemtimes", "", "m:pagemtimes:innerdim");
    const size_t K = Kx;

    constexpr int kMaxNd = Dims::kMaxRank;
    const int xb = std::max(0, xnd - 2);
    const int yb = std::max(0, ynd - 2);
    const int outBatchNd = std::max(xb, yb);
    size_t xBatch[kMaxNd], yBatch[kMaxNd], outBatch[kMaxNd];
    for (int i = 0; i < outBatchNd; ++i) {
        xBatch[i] = (i < xb) ? xd.dim(2 + i) : 1;
        yBatch[i] = (i < yb) ? yd.dim(2 + i) : 1;
        if (xBatch[i] != yBatch[i] && xBatch[i] != 1 && yBatch[i] != 1)
            throw Error("pagemtimes: batch dimensions must broadcast "
                         "(each axis must match or be 1)",
                         0, 0, "pagemtimes", "", "m:pagemtimes:dimagree");
        outBatch[i] = std::max(xBatch[i], yBatch[i]);
    }

    size_t batchN = 1;
    for (int i = 0; i < outBatchNd; ++i) batchN *= outBatch[i];

    const int outNd = 2 + outBatchNd;
    size_t outDimArr[kMaxNd];
    outDimArr[0] = M;
    outDimArr[1] = N;
    for (int i = 0; i < outBatchNd; ++i) outDimArr[2 + i] = outBatch[i];
    auto z = createForDims(Dims(outDimArr, outNd), pagemtimesElemMType<T>(), mr);
    if (M == 0 || N == 0 || batchN == 0)
        return z;

    T *zData = static_cast<T *>(z.rawDataMut());
    const size_t xPageStride = xRowDim * xColDim;
    const size_t yPageStride = yRowDim * yColDim;
    const size_t zPageStride = M * N;

    // Direct-pass when source already matches T and no transpose is
    // needed; otherwise materialise into typed scratch (one per call,
    // reused across all batch pages).
    const bool xDirect = (x.type() == pagemtimesElemMType<T>()) && (tx == TranspOp::None);
    const bool yDirect = (y.type() == pagemtimesElemMType<T>()) && (ty == TranspOp::None);
    ScratchArena scratch(mr);
    ScratchVec<T> scratchX(&scratch), scratchY(&scratch);
    if (!xDirect) scratchX.resize(xPageStride);
    if (!yDirect) scratchY.resize(yPageStride);

    auto getXPage = [&](size_t pageOff) -> const T * {
        if (xDirect)
            return static_cast<const T *>(x.rawData()) + pageOff * xPageStride;
        materialisePage(scratchX.data(), x, pageOff, xRowDim, xColDim, tx);
        return scratchX.data();
    };
    auto getYPage = [&](size_t pageOff) -> const T * {
        if (yDirect)
            return static_cast<const T *>(y.rawData()) + pageOff * yPageStride;
        materialisePage(scratchY.data(), y, pageOff, yRowDim, yColDim, ty);
        return scratchY.data();
    };

    if (outBatchNd == 0) {
        runPageMatmul<T>(getXPage(0), getYPage(0), zData, M, N, K);
        return z;
    }

    size_t xBatchStride[kMaxNd], yBatchStride[kMaxNd];
    {
        size_t sx = 1, sy = 1;
        for (int i = 0; i < outBatchNd; ++i) {
            xBatchStride[i] = sx;
            yBatchStride[i] = sy;
            sx *= xBatch[i];
            sy *= yBatch[i];
        }
    }

    size_t coords[kMaxNd] = {0};
    Dims outBatchDims(outBatch, outBatchNd);
    size_t pageIdx = 0;
    do {
        size_t xOff = 0, yOff = 0;
        for (int i = 0; i < outBatchNd; ++i) {
            const size_t xc = (xBatch[i] == 1) ? 0 : coords[i];
            const size_t yc = (yBatch[i] == 1) ? 0 : coords[i];
            xOff += xc * xBatchStride[i];
            yOff += yc * yBatchStride[i];
        }
        runPageMatmul<T>(getXPage(xOff), getYPage(yOff),
                         zData + pageIdx * zPageStride,
                         M, N, K);
        ++pageIdx;
    } while (incrementCoords(coords, outBatchDims));

    return z;
}

} // namespace

Value pagemtimes(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    return pagemtimes(x, TranspOp::None, y, TranspOp::None, mr);
}

Value pagemtimes(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr)
{
    // MATLAB type promotion: COMPLEX wins over real; SINGLE wins over
    // DOUBLE. Integer/logical/char inputs are rejected — pagemtimes
    // requires floating or complex inputs.
    auto isFloatLike = [](ValueType t) {
        return t == ValueType::DOUBLE || t == ValueType::SINGLE || t == ValueType::COMPLEX;
    };
    if (!isFloatLike(x.type()) || !isFloatLike(y.type()))
        throw Error("pagemtimes: inputs must be 'single', 'double', or complex",
                     0, 0, "pagemtimes", "", "m:pagemtimes:type");
    if (x.isComplex() || y.isComplex())
        return pagemtimesImpl<Complex>(x, tx, y, ty, mr);
    if (x.type() == ValueType::SINGLE || y.type() == ValueType::SINGLE)
        return pagemtimesImpl<float  >(x, tx, y, ty, mr);
    return     pagemtimesImpl<double >(x, tx, y, ty, mr);
}

Value diag(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.dims().isVector()) {
        const size_t n = x.numel();
        auto r = Value::matrix(n, n, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < n; ++i)
            r.elem(i, i) = x.doubleData()[i];
        return r;
    }
    const size_t n = std::min(x.dims().rows(), x.dims().cols());
    auto r = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = x(i, i);
    return r;
}

// ── Sort / find ──────────────────────────────────────────────────────
std::tuple<Value, Value> sort(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isScalar())
        return std::make_tuple(x, Value::scalar(1.0, mr));

    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const int sortDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
    const size_t N = (sortDim == 0) ? R : (sortDim == 1) ? C : P;

    auto r = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                             : Value::matrix(R, C, ValueType::DOUBLE, mr);
    auto idx = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                               : Value::matrix(R, C, ValueType::DOUBLE, mr);

    const size_t slice0 = (sortDim == 0) ? 1 : R;
    const size_t slice1 = (sortDim == 1) ? 1 : C;
    const size_t slice2 = (sortDim == 2) ? 1 : P;
    ScratchArena scratch(mr);
    ScratchVec<std::pair<double, size_t>> buf(N, &scratch);

    for (size_t pp = 0; pp < slice2; ++pp)
        for (size_t c = 0; c < slice1; ++c)
            for (size_t rr = 0; rr < slice0; ++rr) {
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    buf[k] = {x.doubleData()[pIdx * R * C + cIdx * R + rIdx], k};
                }
                std::sort(buf.begin(), buf.end(),
                          [](const auto &a, const auto &b) { return a.first < b.first; });
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    const size_t lin = pIdx * R * C + cIdx * R + rIdx;
                    r.doubleDataMut()[lin] = buf[k].first;
                    idx.doubleDataMut()[lin] = static_cast<double>(buf[k].second + 1);
                }
            }
    return std::make_tuple(std::move(r), std::move(idx));
}

// ── sortrows ─────────────────────────────────────────────────────────
namespace {

// Promote to a 2D DOUBLE matrix for row-tuple ops. Returns a copy if the
// type or shape differs; for already-2D-DOUBLE input returns by value
// (cheap COW in the engine).
Value toDoubleMatrix2D(const Value &x, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": input must be 2D",
                     0, 0, fn, "", std::string("m:") + fn + ":bad2D");
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    if (x.type() == ValueType::DOUBLE) {
        // Return a fresh DOUBLE matrix identical to x — cheap, avoids
        // touching the input through a shared buffer later.
        auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
        if (x.numel() > 0)
            std::memcpy(r.doubleDataMut(), x.doubleData(),
                        x.numel() * sizeof(double));
        return r;
    }
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < x.numel(); ++i)
        dst[i] = x.elemAsDouble(i);
    return r;
}

std::tuple<Value, Value>
sortRowsImpl(const Value &x, const int *cols, std::size_t nCols, std::pmr::memory_resource *mr)
{
    auto m = toDoubleMatrix2D(x, "sortrows", mr);
    const size_t R = m.dims().rows();
    const size_t C = m.dims().cols();

    if (R == 0) {
        // Empty rows — return as-is and an empty 0×1 idx column.
        auto idx = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return std::make_tuple(std::move(m), std::move(idx));
    }

    ScratchArena scratch(mr);

    // Validate cols list. nCols==0 ⇒ all columns ascending in order.
    ScratchVec<int> sortKeys(&scratch);
    if (nCols == 0) {
        sortKeys.reserve(C);
        for (size_t c = 1; c <= C; ++c)
            sortKeys.push_back(static_cast<int>(c));
    } else {
        sortKeys.assign(cols, cols + nCols);
        for (int rawCol : sortKeys) {
            const int absC = (rawCol < 0) ? -rawCol : rawCol;
            if (rawCol == 0 || static_cast<size_t>(absC) > C)
                throw Error("sortrows: column index out of range",
                             0, 0, "sortrows", "", "m:sortrows:badCol");
        }
    }

    auto perm = ScratchVec<size_t>(R, &scratch);
    for (size_t i = 0; i < R; ++i) perm[i] = i;

    const double *src = m.doubleData();
    std::stable_sort(perm.begin(), perm.end(),
        [&](size_t a, size_t b) {
            return detail::rowLexCmpByCols(src, C, R, a, b,
                                            sortKeys.data(), sortKeys.size()) < 0;
        });

    auto sorted = detail::collectRowsByIndex(mr, m, perm.data(), perm.size());
    auto idx = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *idxP = idx.doubleDataMut();
    for (size_t i = 0; i < R; ++i)
        idxP[i] = static_cast<double>(perm[i] + 1);
    return std::make_tuple(std::move(sorted), std::move(idx));
}

} // namespace

std::tuple<Value, Value> sortrows(const Value &x, std::pmr::memory_resource *mr)
{
    return sortRowsImpl(x, nullptr, 0, mr);
}

std::tuple<Value, Value> sortrows(const Value &x, const int *cols, std::size_t nCols, std::pmr::memory_resource *mr)
{
    return sortRowsImpl(x, cols, nCols, mr);
}

Value find(const Value &x, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto indices = ScratchVec<double>(&scratch);
    if (x.isLogical()) {
        const uint8_t *ld = x.logicalData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (ld[i])
                indices.push_back(static_cast<double>(i + 1));
    } else {
        const double *dd = x.doubleData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (dd[i] != 0.0)
                indices.push_back(static_cast<double>(i + 1));
    }
    const bool rowResult = !x.dims().is3D() && x.dims().rows() == 1;
    auto r = rowResult ? Value::matrix(1, indices.size(), ValueType::DOUBLE, mr)
                       : Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    if (!indices.empty())
        std::memcpy(r.doubleDataMut(), indices.data(), indices.size() * sizeof(double));
    return r;
}

// ── nnz / nonzeros ───────────────────────────────────────────────────
namespace {

// Type-aware predicate: element at linear index i non-zero?
// NaN counts as non-zero (NaN != 0). For COMPLEX both parts checked.
template <typename T>
inline bool isNonzeroElemT(const T *p, size_t i) { return p[i] != T{0}; }

inline bool isNonzeroComplex(const Complex *p, size_t i)
{
    return p[i].real() != 0.0 || p[i].imag() != 0.0;
}

template <typename Visit>
void forEachNonzero(const Value &x, Visit visit)
{
    const size_t n = x.numel();
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        for (size_t i = 0; i < n; ++i) if (p[i]) visit(i);
        break;
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroComplex(p, i)) visit(i);
        break;
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    default:
        throw Error("nnz/nonzeros: unsupported element type",
                     0, 0, "nnz", "", "m:nnz:badType");
    }
}

template <typename T>
T *typedDstFor(Value &r, ValueType outType)
{
    switch (outType) {
    case ValueType::LOGICAL: return reinterpret_cast<T *>(r.logicalDataMut());
    case ValueType::DOUBLE:  return reinterpret_cast<T *>(r.doubleDataMut());
    case ValueType::SINGLE:  return reinterpret_cast<T *>(r.singleDataMut());
    case ValueType::COMPLEX: return reinterpret_cast<T *>(r.complexDataMut());
    case ValueType::INT8:    return reinterpret_cast<T *>(r.int8DataMut());
    case ValueType::INT16:   return reinterpret_cast<T *>(r.int16DataMut());
    case ValueType::INT32:   return reinterpret_cast<T *>(r.int32DataMut());
    case ValueType::INT64:   return reinterpret_cast<T *>(r.int64DataMut());
    case ValueType::UINT8:   return reinterpret_cast<T *>(r.uint8DataMut());
    case ValueType::UINT16:  return reinterpret_cast<T *>(r.uint16DataMut());
    case ValueType::UINT32:  return reinterpret_cast<T *>(r.uint32DataMut());
    case ValueType::UINT64:  return reinterpret_cast<T *>(r.uint64DataMut());
    default: return nullptr;
    }
}

template <typename T, typename Reader>
Value collectTypedNonzeros(const Value &x, ValueType outType, Reader read, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<T> vals(&scratch);
    forEachNonzero(x, [&](size_t i) { vals.push_back(read(i)); });
    auto r = Value::matrix(vals.size(), 1, outType, mr);
    if (!vals.empty()) {
        T *dst = typedDstFor<T>(r, outType);
        std::memcpy(dst, vals.data(), vals.size() * sizeof(T));
    }
    return r;
}

} // namespace

Value nnz(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0)
        return Value::scalar(0.0, mr);
    size_t count = 0;
    forEachNonzero(x, [&](size_t) { ++count; });
    return Value::scalar(static_cast<double>(count), mr);
}

Value nonzeros(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) {
        // Empty input → 0×1 column of the source type (DOUBLE if unknown).
        const ValueType outT = (x.type() == ValueType::EMPTY) ? ValueType::DOUBLE : x.type();
        return Value::matrix(0, 1, outT, mr);
    }
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        return collectTypedNonzeros<uint8_t>(x, ValueType::LOGICAL, [&](size_t i) -> uint8_t { return p[i]; }, mr);
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        return collectTypedNonzeros<double>(x, ValueType::DOUBLE, [&](size_t i) -> double { return p[i]; }, mr);
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        return collectTypedNonzeros<float>(x, ValueType::SINGLE, [&](size_t i) -> float { return p[i]; }, mr);
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        return collectTypedNonzeros<Complex>(x, ValueType::COMPLEX, [&](size_t i) -> Complex { return p[i]; }, mr);
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        return collectTypedNonzeros<int8_t>(x, ValueType::INT8, [&](size_t i) -> int8_t { return p[i]; }, mr);
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        return collectTypedNonzeros<int16_t>(x, ValueType::INT16, [&](size_t i) -> int16_t { return p[i]; }, mr);
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        return collectTypedNonzeros<int32_t>(x, ValueType::INT32, [&](size_t i) -> int32_t { return p[i]; }, mr);
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        return collectTypedNonzeros<int64_t>(x, ValueType::INT64, [&](size_t i) -> int64_t { return p[i]; }, mr);
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        return collectTypedNonzeros<uint8_t>(x, ValueType::UINT8, [&](size_t i) -> uint8_t { return p[i]; }, mr);
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        return collectTypedNonzeros<uint16_t>(x, ValueType::UINT16, [&](size_t i) -> uint16_t { return p[i]; }, mr);
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        return collectTypedNonzeros<uint32_t>(x, ValueType::UINT32, [&](size_t i) -> uint32_t { return p[i]; }, mr);
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        return collectTypedNonzeros<uint64_t>(x, ValueType::UINT64, [&](size_t i) -> uint64_t { return p[i]; }, mr);
    }
    default:
        throw Error("nonzeros: unsupported element type",
                     0, 0, "nonzeros", "", "m:nonzeros:badType");
    }
}

// ── Concatenation ────────────────────────────────────────────────────
Value horzcat(Span<const Value> values, std::pmr::memory_resource *mr)
{
    if (values.empty())
        return Value::empty();
    return Value::horzcat(values.data(), values.size(), mr);
}

Value vertcat(Span<const Value> values, std::pmr::memory_resource *mr)
{
    if (values.empty())
        return Value::empty();
    return Value::vertcat(values.data(), values.size(), mr);
}

// ── Grids ────────────────────────────────────────────────────────────
std::tuple<Value, Value> meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel();
    auto X = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < ny; ++r)
        for (size_t c = 0; c < nx; ++c) {
            X.elem(r, c) = x.doubleData()[c];
            Y.elem(r, c) = y.doubleData()[r];
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

// 3-arg meshgrid: returns three [ny, nx, nz] 3-D arrays. See BUGS.md #23.
std::tuple<Value, Value, Value>
meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel(), nz = z.numel();
    auto X = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    auto Y = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    auto Z = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();
    for (size_t p = 0; p < nz; ++p) {
        const double zp = z.elemAsDouble(p);
        for (size_t c = 0; c < nx; ++c) {
            const double xc = x.elemAsDouble(c);
            for (size_t r = 0; r < ny; ++r) {
                const size_t idx = r + c * ny + p * (nx * ny);
                xd[idx] = xc;
                yd[idx] = y.elemAsDouble(r);
                zd[idx] = zp;
            }
        }
    }
    return std::make_tuple(std::move(X), std::move(Y), std::move(Z));
}

// ── ndgrid ──────────────────────────────────────────────────────────
std::tuple<Value, Value>
ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel();
    // Output shape: [nx, ny] — first arg is row dim (axes-major).
    auto X = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < nx; ++r)
        for (size_t c = 0; c < ny; ++c) {
            X.elem(r, c) = x.elemAsDouble(r);
            Y.elem(r, c) = y.elemAsDouble(c);
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

std::tuple<Value, Value, Value>
ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel(), nz = z.numel();
    auto X = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Y = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Z = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    for (size_t p = 0; p < nz; ++p)
        for (size_t c = 0; c < ny; ++c)
            for (size_t r = 0; r < nx; ++r) {
                X.elem(r, c, p) = x.elemAsDouble(r);
                Y.elem(r, c, p) = y.elemAsDouble(c);
                Z.elem(r, c, p) = z.elemAsDouble(p);
            }
    return std::make_tuple(std::move(X), std::move(Y), std::move(Z));
}

// ── kron ────────────────────────────────────────────────────────────
Value kron(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        throw Error("kron: complex inputs are not supported",
                     0, 0, "kron", "", "m:kron:complex");
    if (a.dims().is3D() || a.dims().ndim() > 2
        || b.dims().is3D() || b.dims().ndim() > 2)
        throw Error("kron: inputs must be 2D",
                     0, 0, "kron", "", "m:kron:rank");

    const size_t rA = a.dims().rows(), cA = a.dims().cols();
    const size_t rB = b.dims().rows(), cB = b.dims().cols();
    const size_t rOut = rA * rB, cOut = cA * cB;

    auto out = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
    if (rOut == 0 || cOut == 0) return out;

    double *dst = out.doubleDataMut();
    for (size_t ja = 0; ja < cA; ++ja)
        for (size_t ia = 0; ia < rA; ++ia) {
            const double av = a.elemAsDouble(ia + ja * rA);
            for (size_t jb = 0; jb < cB; ++jb) {
                const size_t jOut = ja * cB + jb;
                for (size_t ib = 0; ib < rB; ++ib) {
                    const size_t iOut = ia * rB + ib;
                    const double bv = b.elemAsDouble(ib + jb * rB);
                    dst[jOut * rOut + iOut] = av * bv;
                }
            }
        }
    return out;
}

// ── Reductions and products ──────────────────────────────────────────
Value cumsum(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        cumsumScan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }
    const size_t R = x.dims().rows(), C = x.dims().cols();
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    // Per-column inclusive scan — column data is contiguous.
    for (size_t c = 0; c < C; ++c)
        cumsumScan(src + c * R, dst + c * R, R);
    return r;
}

// cumsum along an explicit dim. Output shape equals input shape (this is
// not a reduction). Vector / scalar input ignores dim and walks linearly.
Value cumsum(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim <= 0) return cumsum(x, mr);
    if (x.dims().isVector() || x.isScalar()) return cumsum(x, mr);

    const int d = detail::resolveDim(x, dim, "cumsum");
    const auto &dd = x.dims();

    // ND fallback for rank ≥ 4: per-slice scan along axis d-1. Inner
    // block size B = prod(dim[0..d-2]); outer count O = prod(dim[d..]).
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error("cumsum: rank exceeds 32",
                         0, 0, "cumsum", "", "m:cumsum:tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, dd.ndim(), ValueType::DOUBLE, mr);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                cumsumScan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc += src[base + k * B];
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                       : Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // dim=1: scan down each column. Column data is contiguous so
        // route through the SIMD prefix-sum kernel.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                cumsumScan(src + base, dst + base, R);
            }
    } else if (d == 2) {
        // Walk across columns for each (row, page). Stride = R.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                const size_t pageBase = pp * R * C;
                for (size_t c = 0; c < C; ++c) {
                    s += src[pageBase + c * R + rr];
                    dst[pageBase + c * R + rr] = s;
                }
            }
    } else if (d == 3) {
        // Walk through pages for each (row, col). Stride = R*C.
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                for (size_t pp = 0; pp < P; ++pp) {
                    s += src[pp * R * C + c * R + rr];
                    dst[pp * R * C + c * R + rr] = s;
                }
            }
    }
    return r;
}

// ── Generic cumulative kernel for cumprod / cummax / cummin ─────────
//
// Op is a binary functor (double, double) -> double. Init is the value
// the running accumulator starts at; for cumulative ops we instead seed
// with the first element of the slice, but seeding behavior is still
// captured by Op (e.g. cumprod could just use init=1.0 multiplicative).
// We use the seed-with-first style so cummax / cummin work without
// needing to know +/- infinity for the initial value.
namespace {

template <typename Op>
void cumKernel(const Value &x, int d, Op op, double *dst)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const double *src = x.doubleData();

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                if (R == 0) continue;
                double acc = src[base];
                dst[base] = acc;
                for (size_t rr = 1; rr < R; ++rr) {
                    acc = op(acc, src[base + rr]);
                    dst[base + rr] = acc;
                }
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                const size_t pageBase = pp * R * C;
                if (C == 0) continue;
                double acc = src[pageBase + rr];
                dst[pageBase + rr] = acc;
                for (size_t c = 1; c < C; ++c) {
                    acc = op(acc, src[pageBase + c * R + rr]);
                    dst[pageBase + c * R + rr] = acc;
                }
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                if (P == 0) continue;
                double acc = src[c * R + rr];
                dst[c * R + rr] = acc;
                for (size_t pp = 1; pp < P; ++pp) {
                    acc = op(acc, src[pp * R * C + c * R + rr]);
                    dst[pp * R * C + c * R + rr] = acc;
                }
            }
    }
}

template <typename Op>
Value cumImpl(const Value &x, int dim, Op op, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    if (x.dims().isVector() || x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(),
                                ValueType::DOUBLE, mr);
        if (x.numel() == 0) return r;
        double acc = x.doubleData()[0];
        r.doubleDataMut()[0] = acc;
        for (size_t i = 1; i < x.numel(); ++i) {
            acc = op(acc, x.doubleData()[i]);
            r.doubleDataMut()[i] = acc;
        }
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    auto r = dd.is3D() ? Value::matrix3d(dd.rows(), dd.cols(), dd.pages(),
                                          ValueType::DOUBLE, mr)
                       : Value::matrix(dd.rows(), dd.cols(),
                                        ValueType::DOUBLE, mr);
    cumKernel(x, d, op, r.doubleDataMut());
    return r;
}

} // namespace

// cumprod / cummax / cummin: SIMD prefix-op kernels in
// backends/MStdCumSum_{simd,portable}.cpp handle vector input and the
// dim=1 (column) path where access is contiguous. For dim=2/3 the
// strided access pattern doesn't benefit from SIMD; cumImpl's scalar
// cumKernel still handles those (with the same Op as before).
namespace {

using ScanFn = void (*)(const double *, double *, std::size_t);

template <typename Op>
Value cumScanDispatch(const Value &x, int dim, ScanFn scan, Op scalarOp, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        scan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();

    // ND fallback (rank ≥ 4): per-slice scan along axis d-1.
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error(std::string(fn) + ": rank exceeds 32",
                         0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, dd.ndim(), ValueType::DOUBLE, mr);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                scan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc = scalarOp(acc, src[base + k * B]);
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                       : Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // Per-column scan — column data is contiguous, route through SIMD.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                scan(src + base, dst + base, R);
            }
    } else {
        // dim=2/3: strided access; reuse the existing scalar cumKernel.
        cumKernel(x, d, scalarOp, dst);
    }
    return r;
}

} // namespace

Value cumprod(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return cumScanDispatch(x, dim, cumprodScan, [](double a, double b) { return a * b; }, "cumprod", mr);
}

Value cummax(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    // NaN propagation: MATLAB cummax skips NaN if 'omitnan' is passed
    // and propagates otherwise. Default = 'omitnan' since R2018a; we
    // skip NaN here, treating them as identity.
    return cumScanDispatch(x, dim, cummaxScan, [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::max(a, b);
                           }, "cummax", mr);
}

Value cummin(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return cumScanDispatch(x, dim, cumminScan, [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::min(a, b);
                           }, "cummin", mr);
}

// ── diff: discrete difference ────────────────────────────────────────
namespace {

// One pass of forward differences along axis `d` (1-based). Source has
// dim[d-1] = sliceLen; output has dim[d-1] = sliceLen - 1. Column-major
// strides (innerStride = prod(dim[0..d-2])).
void diffOnceDouble(const double *src, double *dst,
                    const Dims &srcDims, int d)
{
    const int nd = srcDims.ndim();
    const size_t sliceLen = srcDims.dim(d - 1);
    if (sliceLen < 2) return;  // out has zero elements

    size_t innerStride = 1;
    for (int i = 0; i < d - 1; ++i) innerStride *= srcDims.dim(i);
    size_t outerCount = 1;
    for (int i = d; i < nd; ++i) outerCount *= srcDims.dim(i);
    const size_t outSliceLen = sliceLen - 1;

    if (innerStride == 1) {
        // Contiguous along the diff axis — simple linear pass per outer block.
        for (size_t o = 0; o < outerCount; ++o) {
            const double *s = src + o * sliceLen;
            double *t = dst + o * outSliceLen;
            for (size_t k = 0; k < outSliceLen; ++k)
                t[k] = s[k + 1] - s[k];
        }
    } else {
        for (size_t o = 0; o < outerCount; ++o)
            for (size_t b = 0; b < innerStride; ++b) {
                const size_t srcBase = o * innerStride * sliceLen + b;
                const size_t dstBase = o * innerStride * outSliceLen + b;
                for (size_t k = 0; k < outSliceLen; ++k)
                    dst[dstBase + k * innerStride] =
                        src[srcBase + (k + 1) * innerStride] -
                        src[srcBase + k * innerStride];
            }
    }
}

Value makeDiffOutput(const Dims &srcDims, int d, size_t step, std::pmr::memory_resource *mr)
{
    const int nd = srcDims.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("diff: rank exceeds 32",
                     0, 0, "diff", "", "m:diff:tooManyDims");
    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = srcDims.dim(i);
    outDims[d - 1] = (outDims[d - 1] >= step) ? outDims[d - 1] - step : 0;
    return Value::matrixND(outDims, nd, ValueType::DOUBLE, mr);
}

Value copyToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    size_t dims[kMaxNd];
    for (int i = 0; i < nd; ++i) dims[i] = dd.dim(i);
    auto r = Value::matrixND(dims, nd, ValueType::DOUBLE, mr);
    if (x.type() == ValueType::DOUBLE) {
        std::memcpy(r.doubleDataMut(), x.doubleData(),
                    x.numel() * sizeof(double));
    } else {
        double *dst = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = x.elemAsDouble(i);
    }
    return r;
}

} // namespace

Value diff(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (n < 0)
        throw Error("diff: order n must be non-negative",
                     0, 0, "diff", "", "m:diff:badOrder");

    if (n == 0) {
        // Identity copy preserving DOUBLE shape.
        return copyToDouble(x, mr);
    }

    // Scalar: MATLAB returns 1×0 empty.
    if (x.isScalar())
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    const int d = detail::resolveDim(x, dim, "diff");
    const auto &dd = x.dims();
    const size_t sliceLen = (d >= 1 && d <= dd.ndim()) ? dd.dim(d - 1) : 1;

    // If n collapses or exceeds the dim, return correctly-shaped empty.
    if (sliceLen <= static_cast<size_t>(n))
        return makeDiffOutput(dd, d, sliceLen, mr);

    // Promote integer/logical to DOUBLE first (consistent with cumsum).
    Value cur = copyToDouble(x, mr);

    for (int pass = 0; pass < n; ++pass) {
        const auto &curDims = cur.dims();
        auto out = makeDiffOutput(curDims, d, 1, mr);
        diffOnceDouble(cur.doubleData(), out.doubleDataMut(), curDims, d);
        cur = std::move(out);
    }
    return cur;
}

// ── any / all moved to backends/MStdLogicalReductions_{simd,portable}.cpp
//
// MATLAB's any(X) returns true if ANY element is non-zero (NaN counts
// as true since NaN != 0). all(X) returns true if ALL elements are
// non-zero. Empty: any → false, all → true (vacuously). The SIMD
// backend scans LOGICAL bytes and DOUBLE lanes directly with early
// exit (Phase P1 of project_perf_optimization_plan.md).

namespace {

// Used by xor below — small inputs, no need for a SIMD path.
Value promoteToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::DOUBLE) return x;
    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.elemAsDouble(i);
    return r;
}

} // namespace

// ── xor (elementwise logical) ────────────────────────────────────────
Value xorOf(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    auto ad = promoteToDouble(a, mr);
    auto bd = promoteToDouble(b, mr);
    auto d = elementwiseDouble(ad, bd,
        [](double aa, double bb) {
            return ((aa != 0.0) != (bb != 0.0)) ? 1.0 : 0.0;
        }, mr);
    if (d.isScalar()) return Value::logicalScalar(d.toScalar() != 0.0, mr);
    auto r = createLike(d, ValueType::LOGICAL, mr);
    for (size_t i = 0; i < d.numel(); ++i)
        r.logicalDataMut()[i] = (d.doubleData()[i] != 0.0) ? 1 : 0;
    return r;
}

Value cross(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    // MATLAB: cross(A, B) operates along the first dimension with
    // size 3. Common shapes: 1x3, 3x1, 3xN, Nx3. The result has the
    // same shape as the inputs. See BUGS.md #18.
    const auto &da = a.dims();
    const auto &db = b.dims();
    if (da.rows() != db.rows() || da.cols() != db.cols())
        throw Error("cross: A and B must have the same shape",
                     0, 0, "cross", "", "m:cross:shapeMismatch");

    const size_t nr = da.rows();
    const size_t nc = da.cols();

    // Pick the dimension to cross along: first one of size 3.
    int crossDim;
    if (nr == 3)      crossDim = 0; // cross along rows (each column is a 3-vec)
    else if (nc == 3) crossDim = 1; // cross along cols (each row is a 3-vec)
    else
        throw Error("cross: A and B must have at least one dimension of length 3",
                     0, 0, "cross", "", "m:cross:badSize");

    auto out = Value::matrix(nr, nc, ValueType::DOUBLE, mr);
    const double *ad = a.doubleData();
    const double *bd = b.doubleData();
    double *od = out.doubleDataMut();

    if (crossDim == 0) {
        // 3xN: column-major storage, so col c starts at c*3.
        const size_t batches = nc;
        for (size_t c = 0; c < batches; ++c) {
            const size_t base = c * 3;
            const double a0 = ad[base], a1 = ad[base + 1], a2 = ad[base + 2];
            const double b0 = bd[base], b1 = bd[base + 1], b2 = bd[base + 2];
            od[base    ] = a1 * b2 - a2 * b1;
            od[base + 1] = a2 * b0 - a0 * b2;
            od[base + 2] = a0 * b1 - a1 * b0;
        }
    } else {
        // Nx3: column-major, so element (r, k) at r + k*nr.
        const size_t batches = nr;
        for (size_t r = 0; r < batches; ++r) {
            const double a0 = ad[r], a1 = ad[r + nr], a2 = ad[r + 2 * nr];
            const double b0 = bd[r], b1 = bd[r + nr], b2 = bd[r + 2 * nr];
            od[r           ] = a1 * b2 - a2 * b1;
            od[r +     nr  ] = a2 * b0 - a0 * b2;
            od[r + 2 * nr  ] = a0 * b1 - a1 * b0;
        }
    }
    return out;
}

Value dot(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    if (a.numel() != b.numel())
        throw Error("dot: vectors must have same length",
                     0, 0, "dot", "", "m:dot:lengthMismatch");
    double s = 0;
    for (size_t i = 0; i < a.numel(); ++i)
        s += a.doubleData()[i] * b.doubleData()[i];
    return Value::scalar(s, mr);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void zeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    // Strip trailing class-name (e.g. 'uint8') or 'like' form before
    // parsing dims. Default type is DOUBLE.
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    // Value::matrix*/matrixND zero-fill the buffer for any type, so
    // createMatrixND with the requested type IS the zeros() output.
    outs[0] = createMatrixND(d.data(), d.size(), t, mr);
}

// Fill `v` with one in its declared type (1 / 1.0 / true).
namespace { inline void fillOnes(Value &v, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    switch (t) {
      case ValueType::DOUBLE:  { auto *p = v.doubleDataMut();  std::fill(p, p + n, 1.0); break; }
      case ValueType::SINGLE:  { auto *p = v.singleDataMut();  std::fill(p, p + n, 1.0f); break; }
      case ValueType::LOGICAL: { auto *p = v.logicalDataMut(); std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::INT8:    { auto *p = v.int8DataMut();    std::fill(p, p + n, int8_t(1)); break; }
      case ValueType::INT16:   { auto *p = v.int16DataMut();   std::fill(p, p + n, int16_t(1)); break; }
      case ValueType::INT32:   { auto *p = v.int32DataMut();   std::fill(p, p + n, int32_t(1)); break; }
      case ValueType::INT64:   { auto *p = v.int64DataMut();   std::fill(p, p + n, int64_t(1)); break; }
      case ValueType::UINT8:   { auto *p = v.uint8DataMut();   std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::UINT16:  { auto *p = v.uint16DataMut();  std::fill(p, p + n, uint16_t(1)); break; }
      case ValueType::UINT32:  { auto *p = v.uint32DataMut();  std::fill(p, p + n, uint32_t(1)); break; }
      case ValueType::UINT64:  { auto *p = v.uint64DataMut();  std::fill(p, p + n, uint64_t(1)); break; }
      default: throw Error("ones: unsupported type for fill",
                           0, 0, "ones", "", "m:ones:badType");
    }
}}

void ones_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    fillOnes(m, t);
    outs[0] = std::move(m);
}

// MATLAB's colon function: colon(j, k) = j:k, colon(j, i, k) = j:i:k.
// Useful when the operator form is awkward (function-handle slot, etc.)
// and to be a real callable for parity tests. Type preservation matches
// the operator path (see core/src/tree_walker.cpp:colonOutputType and
// core/src/vm.cpp:OpCode::COLON).
namespace { ValueType colonOutType(const Value *ops, size_t n)
{
    ValueType nonDouble = ValueType::DOUBLE;
    bool found = false;
    for (size_t i = 0; i < n; ++i) {
        ValueType t = ops[i].type();
        if (t == ValueType::DOUBLE) continue;
        if (!found) { nonDouble = t; found = true; }
        else if (t != nonDouble)
            throw Error("colon: operands must be all the same type, "
                        "or mixed with real scalar doubles",
                        0, 0, "colon", "", "m:colon:typeMix");
    }
    return nonDouble;
}}

void colon_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        ValueType t = colonOutType(args.data(), 2);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(), t, mr);
    } else if (args.size() == 3) {
        ValueType t = colonOutType(args.data(), 3);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(),
                                          args[2].toScalar(), t, mr);
    } else {
        throw Error("colon: requires 2 or 3 arguments",
                    0, 0, "colon", "", "m:colon:nargin");
    }
}

// MATLAB's sparse() with size args allocates an MxN sparse zero matrix.
// Numkit has no sparse storage class -- this stub returns dense zeros.
// Matches issparse=false (we ship that stub; see types.cpp:issparse).
// KNOWN GAP: numkit returns dense; MATLAB returns sparse storage class.
// All numerical operations match (zeros on both sides), only class()
// differs. Documented in PROGRESS for sparse().
void sparse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // sparse(M, N) -- allocate MxN dense zeros.
        const size_t M = static_cast<size_t>(args[0].toScalar());
        const size_t N = static_cast<size_t>(args[1].toScalar());
        outs[0] = Value::matrix(M, N, ValueType::DOUBLE, mr);
        return;
    }
    if (args.size() == 1) {
        // sparse(A) -- "convert" dense to sparse. We just return A as-is
        // (since we have no sparse storage). For most numerical use this
        // is correct; isparse() still returns false (matches numkit
        // semantics).
        outs[0] = args[0];
        return;
    }
    throw Error("sparse: numkit has no sparse storage; supports only "
                "sparse(M, N) → dense zeros and sparse(A) → A passthrough",
                0, 0, "sparse", "", "m:sparse:NoSparse");
}

// `nan` / `NaN` / `inf` / `Inf` are MATLAB built-in functions (not
// constants): bare `nan` returns scalar NaN; `nan(M, N, ..., 'type')`
// returns float array filled with NaN (only 'double' or 'single' are
// allowed -- integer types can't represent NaN/Inf, MATLAB throws).
// Same shape parsing as zeros/ones.
namespace { void nanInfFill(Value &v, double fillD, float fillS, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    if (t == ValueType::DOUBLE) {
        auto *p = v.doubleDataMut(); std::fill(p, p + n, fillD);
    } else if (t == ValueType::SINGLE) {
        auto *p = v.singleDataMut(); std::fill(p, p + n, fillS);
    }
}}

void nan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("nan: type must be 'double' or 'single' (NaN is float-only)",
                    0, 0, "nan", "", "m:nan:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), t);
    outs[0] = std::move(m);
}

void inf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::infinity(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("inf: type must be 'double' or 'single' (Inf is float-only)",
                    0, 0, "inf", "", "m:inf:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::infinity(),
                  std::numeric_limits<float>::infinity(), t);
    outs[0] = std::move(m);
}

// `true` and `false` are MATLAB built-in functions (not constants):
// bare `true` returns a scalar logical 1; `true(M, N, ...)` returns a
// logical array filled with 1 (or 0 for false). Mirrors zeros/ones
// shape parsing. See BUGS.md #30.
void true_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(true, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    auto v = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
    uint8_t *p = v.logicalDataMut();
    for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    outs[0] = std::move(v);
}

void false_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(false, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    // createMatrixND zero-fills LOGICAL by default.
    outs[0] = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
}

void eye_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    auto d = parseDimsArgs(dimArgs);
    if (t == ValueType::DOUBLE) {
        // Fast path: direct double eye().
        outs[0] = eye(d.rows, d.cols, mr);
        return;
    }
    // Typed eye: zero-fill matrix of `t`, then set diagonal to one.
    auto m = Value::matrix(d.rows, d.cols, t, mr);
    const size_t k = std::min(d.rows, d.cols);
    switch (t) {
      case ValueType::SINGLE: { auto *p = m.singleDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1.0f; break; }
      case ValueType::LOGICAL:{ auto *p = m.logicalDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT8:   { auto *p = m.int8DataMut();    for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT16:  { auto *p = m.int16DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT32:  { auto *p = m.int32DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT64:  { auto *p = m.int64DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT8:  { auto *p = m.uint8DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT16: { auto *p = m.uint16DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT32: { auto *p = m.uint32DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT64: { auto *p = m.uint64DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      default: throw Error("eye: unsupported type", 0, 0, "eye", "", "m:eye:badType");
    }
    outs[0] = std::move(m);
}

void magic_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("magic: requires exactly 1 argument",
                     0, 0, "magic", "", "m:magic:nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error("magic: N must be a non-negative integer",
                     0, 0, "magic", "", "m:magic:badN");
    outs[0] = magic(static_cast<size_t>(nd), ctx.engine->resource());
}

namespace {

// Collect a row/column vector of doubles in linear element order via
// elemAsDouble. Used by toeplitz/hankel/vander/compan. Caller-owned
// buffer to avoid an allocation hop through pmr::vector.
void valueToDoubleVec(const Value &v, std::vector<double> &dst)
{
    const std::size_t n = v.numel();
    dst.resize(n);
    for (std::size_t i = 0; i < n; ++i) dst[i] = v.elemAsDouble(i);
}

} // anonymous namespace

void toeplitz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("toeplitz: requires 1 or 2 arguments",
                    0, 0, "toeplitz", "", "m:toeplitz:nargin");
    std::vector<double> cv, rv;
    valueToDoubleVec(args[0], cv);
    if (args.size() == 2) {
        valueToDoubleVec(args[1], rv);
    } else {
        // Single-arg: r = c (real input). r[0] always overridden by c[0]
        // in the implementation, so any difference is irrelevant.
        rv = cv;
    }
    if (cv.empty() || rv.empty())
        throw Error("toeplitz: inputs must be non-empty",
                    0, 0, "toeplitz", "", "m:toeplitz:empty");
    outs[0] = toeplitz(cv.data(), cv.size(), rv.data(), rv.size(), ctx.engine->resource());
}

void hankel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("hankel: requires 1 or 2 arguments",
                    0, 0, "hankel", "", "m:hankel:nargin");
    std::vector<double> cv, rv;
    valueToDoubleVec(args[0], cv);
    if (args.size() == 2) {
        valueToDoubleVec(args[1], rv);
    } else {
        // Single-arg: r is all zeros, length = numel(c) (anti-triangular Hankel).
        rv.assign(cv.size(), 0.0);
    }
    if (cv.empty() || rv.empty())
        throw Error("hankel: inputs must be non-empty",
                    0, 0, "hankel", "", "m:hankel:empty");
    outs[0] = hankel(cv.data(), cv.size(), rv.data(), rv.size(), ctx.engine->resource());
}

void vander_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("vander: requires exactly 1 argument",
                    0, 0, "vander", "", "m:vander:nargin");
    std::vector<double> v;
    valueToDoubleVec(args[0], v);
    outs[0] = vander(v.data(), v.size(), ctx.engine->resource());
}

void compan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("compan: requires exactly 1 argument",
                    0, 0, "compan", "", "m:compan:nargin");
    std::vector<double> p;
    valueToDoubleVec(args[0], p);
    outs[0] = compan(p.data(), p.size(), ctx.engine->resource());
}

namespace {

// Common gateway for the "size-from-scalar" test-matrix functions
// (pascal, hilb, invhilb, wilkinson, hadamard).
size_t requireSizeArg(Span<const Value> args, const char *fn)
{
    if (args.size() != 1)
        throw Error(std::string(fn) + ": requires exactly 1 argument",
                    0, 0, fn, "", std::string("m:") + fn + ":nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error(std::string(fn) + ": N must be a non-negative integer",
                    0, 0, fn, "", std::string("m:") + fn + ":badN");
    return static_cast<size_t>(nd);
}

} // anonymous namespace

void pascal_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = pascal(requireSizeArg(args, "pascal"), ctx.engine->resource());
}

void hilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hilb(requireSizeArg(args, "hilb"), ctx.engine->resource());
}

void invhilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = invhilb(requireSizeArg(args, "invhilb"), ctx.engine->resource());
}

void wilkinson_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = wilkinson(requireSizeArg(args, "wilkinson"), ctx.engine->resource());
}

void hadamard_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hadamard(requireSizeArg(args, "hadamard"), ctx.engine->resource());
}

void rosser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (!args.empty())
        throw Error("rosser: takes no arguments",
                    0, 0, "rosser", "", "m:rosser:nargin");
    outs[0] = rosser(ctx.engine->resource());
}

void inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("inv: requires exactly 1 argument",
                    0, 0, "inv", "", "m:inv:nargin");
    outs[0] = inv(args[0], ctx.engine->resource());
}

void linsolve_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("linsolve: requires (A, B[, opts])",
                    0, 0, "linsolve", "", "m:linsolve:nargin");
    // 3rd arg (opts struct) accepted for MATLAB-compat but ignored.
    outs[0] = linsolve(args[0], args[1], ctx.engine->resource());
}

void pageinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("pageinv: requires exactly 1 argument",
                    0, 0, "pageinv", "", "m:pageinv:nargin");
    outs[0] = pageinv(args[0], ctx.engine->resource());
}

void trace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("trace: requires exactly 1 argument",
                    0, 0, "trace", "", "m:trace:nargin");
    outs[0] = trace(args[0], ctx.engine->resource());
}

void det_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("det: requires exactly 1 argument",
                    0, 0, "det", "", "m:det:nargin");
    outs[0] = det(args[0], ctx.engine->resource());
}

void chol_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("chol: requires exactly 1 argument",
                    0, 0, "chol", "", "m:chol:nargin");
    outs[0] = chol(args[0], ctx.engine->resource());
}

void lu_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("lu: requires exactly 1 argument",
                    0, 0, "lu", "", "m:lu:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [L, U, P] = lu_decompose(args[0], mr);
        outs[0] = std::move(L);
        outs[1] = std::move(U);
        if (nargout >= 3) outs[2] = std::move(P);
    } else {
        outs[0] = lu_combined(args[0], mr);
    }
}

void qr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("qr: requires exactly 1 argument",
                    0, 0, "qr", "", "m:qr:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [Q, R] = qr_decompose(args[0], mr);
        outs[0] = std::move(Q);
        outs[1] = std::move(R);
    } else {
        outs[0] = qr_R_only(args[0], mr);
    }
}

void svd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("svd: requires exactly 1 argument",
                    0, 0, "svd", "", "m:svd:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [U, S, V] = svd_decompose(args[0], mr);
        outs[0] = std::move(U);
        outs[1] = std::move(S);
        if (nargout >= 3) outs[2] = std::move(V);
    } else {
        outs[0] = svd_values(args[0], mr);
    }
}

void rank_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("rank: requires (A) or (A, tol)",
                    0, 0, "rank", "", "m:rank:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = rank_of(args[0], tol, ctx.engine->resource());
}

void pinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("pinv: requires (A) or (A, tol)",
                    0, 0, "pinv", "", "m:pinv:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = pinv(args[0], tol, ctx.engine->resource());
}

void cond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("cond: requires exactly 1 argument (2-norm only in this revision)",
                    0, 0, "cond", "", "m:cond:nargin");
    outs[0] = cond_2norm(args[0], ctx.engine->resource());
}

void orth_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("orth: requires (A) or (A, tol)",
                    0, 0, "orth", "", "m:orth:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = orth(args[0], tol, ctx.engine->resource());
}

void null_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("null: requires (A) or (A, tol)",
                    0, 0, "null", "", "m:null:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = null_basis(args[0], tol, ctx.engine->resource());
}

void normest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("normest: requires exactly 1 argument",
                    0, 0, "normest", "", "m:normest:nargin");
    outs[0] = normest(args[0], ctx.engine->resource());
}

void eig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("eig: requires exactly 1 argument",
                    0, 0, "eig", "", "m:eig:nargin");
    auto *mr = ctx.engine->resource();

    // Dispatch: symmetric -> Jacobi (eigenvalues + eigenvectors).
    // Asymmetric -> general path (eigenvalues only via char poly + roots;
    // eigenvectors deferred to Phase 2c-3 with QR iteration).
    if (isSymmetric(args[0], 1e-10)) {
        if (nargout >= 2) {
            auto [V, D] = eig_symmetric(args[0], mr);
            outs[0] = std::move(V);
            outs[1] = std::move(D);
        } else {
            outs[0] = eig_values(args[0], mr);
        }
    } else {
        if (nargout >= 2) {
            // [V, D] for asymmetric: works when all eigenvalues are
            // real (via null-space of A - lam*I); throws if complex
            // eigenvalues are present (Francis QR deferred).
            auto [V, D] = eig_general_VD(args[0], mr);
            outs[0] = std::move(V);
            outs[1] = std::move(D);
        } else {
            outs[0] = eig_general_values(args[0], mr);
        }
    }
}

void expm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("expm: requires exactly 1 argument",
                    0, 0, "expm", "", "m:expm:nargin");
    outs[0] = expm(args[0], ctx.engine->resource());
}

void logm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("logm: requires exactly 1 argument",
                    0, 0, "logm", "", "m:logm:nargin");
    outs[0] = logm_sym(args[0], ctx.engine->resource());
}

void sqrtm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("sqrtm: requires exactly 1 argument",
                    0, 0, "sqrtm", "", "m:sqrtm:nargin");
    outs[0] = sqrtm_sym(args[0], ctx.engine->resource());
}

void schur_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("schur: requires exactly 1 argument",
                    0, 0, "schur", "", "m:schur:nargin");
    auto *mr = ctx.engine->resource();
    auto [U, T] = schur_sym(args[0], mr);
    if (nargout >= 2) {
        outs[0] = std::move(U);
        outs[1] = std::move(T);
    } else {
        outs[0] = std::move(T);
    }
}

void sylvester_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 3)
        throw Error("sylvester: requires (A, B, C)",
                    0, 0, "sylvester", "", "m:sylvester:nargin");
    outs[0] = sylvester_sym(args[0], args[1], args[2], ctx.engine->resource());
}

void norm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("norm: requires (X) or (X, p)",
                    0, 0, "norm", "", "m:norm:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = norm_value(args[0], 2.0, mr);
        return;
    }
    const Value &p = args[1];
    if (p.isChar() || p.isString()) {
        const auto s = p.toString();
        if (s == "fro" || s == "Fro") {
            outs[0] = norm_fro(args[0], mr);
            return;
        }
        if (s == "inf" || s == "Inf") {
            outs[0] = norm_inf(args[0], mr);
            return;
        }
        throw Error("norm: string p must be 'fro' or 'inf'",
                    0, 0, "norm", "", "m:norm:badStringP");
    }
    const double pv = p.toScalar();
    if (std::isinf(pv)) {
        outs[0] = norm_inf(args[0], mr);
        return;
    }
    outs[0] = norm_value(args[0], pv, mr);
}

void subspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("subspace: requires (A, B)",
                    0, 0, "subspace", "", "m:subspace:nargin");
    outs[0] = subspace(args[0], args[1], ctx.engine->resource());
}

void hess_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("hess: requires exactly 1 argument",
                    0, 0, "hess", "", "m:hess:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [P, H] = hess(args[0], mr);
        outs[0] = std::move(P);
        outs[1] = std::move(H);
    } else {
        outs[0] = hess_H_only(args[0], mr);
    }
}

void topkrows_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("topkrows: requires (A, k)",
                    0, 0, "topkrows", "", "m:topkrows:nargin");
    const double kd = args[1].toScalar();
    if (kd < 0.0 || kd != std::floor(kd))
        throw Error("topkrows: k must be a non-negative integer",
                    0, 0, "topkrows", "", "m:topkrows:badK");
    outs[0] = topkrows(args[0], static_cast<std::size_t>(kd), ctx.engine->resource());
}

void size_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("Not enough input arguments",
                     0, 0, "size", "", "m:size:nargin");
    auto *mr = ctx.engine->resource();

    if (args.size() >= 2) {
        outs[0] = size(args[0], static_cast<int>(args[1].toScalar()), mr);
        return;
    }

    if (nargout > 1) {
        const auto &dims = args[0].dims();
        // Multi-output form: [r, c] = size(A) or [r, c, p, ...] = size(A).
        // For ND tensors, dims past nargout-1 are gathered into the last
        // requested output (MATLAB behaviour: extra-dim sizes multiplied
        // into the trailing slot). For dims past actual ndim, return 1.
        for (size_t i = 0; i < nargout && i < outs.size(); ++i) {
            double v;
            if (i + 1 < nargout) {
                v = static_cast<double>(dims.dim(static_cast<int>(i)));
            } else {
                // Last requested output: multiply remaining dims (if any).
                size_t prod = 1;
                for (int j = static_cast<int>(i); j < dims.ndim(); ++j)
                    prod *= dims.dim(j);
                if (dims.ndim() <= static_cast<int>(i)) prod = 1;
                v = static_cast<double>(prod);
            }
            outs[i] = Value::scalar(v, mr);
        }
        return;
    }

    outs[0] = size(args[0], mr);
}

void length_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("length: requires 1 argument",
                     0, 0, "length", "", "m:length:nargin");
    outs[0] = length(args[0], ctx.engine->resource());
}

void numel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("numel: requires 1 argument",
                     0, 0, "numel", "", "m:numel:nargin");
    outs[0] = numel(args[0], ctx.engine->resource());
}

void ndims_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ndims: requires 1 argument",
                     0, 0, "ndims", "", "m:ndims:nargin");
    outs[0] = ndims(args[0], ctx.engine->resource());
}

void reshape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("reshape: requires at least 2 arguments",
                     0, 0, "reshape", "", "m:reshape:nargin");

    const auto &x = args[0];
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> dims(&scratch);

    // Dims-vector form: reshape(A, [m n p ...]). No [] inference here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        dims = parseDimsArgsND(&scratch, args.subspan(1));
    } else {
        // Scalar-args form: reshape(A, m, n, ...). One [] allowed for
        // dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        dims.assign(dimCount, 1);
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount; ++i) {
            if (args[i + 1].isEmpty()) {
                if (inferPos >= 0)
                    throw Error("reshape: only one dimension may be inferred via []",
                                 0, 0, "reshape", "", "m:reshape:tooManyInferred");
                inferPos = static_cast<int>(i);
            } else {
                dims[i] = static_cast<size_t>(args[i + 1].toScalar());
                knownProd *= dims[i];
            }
        }
        if (inferPos >= 0) {
            if (knownProd == 0 || x.numel() % knownProd != 0)
                throw Error("reshape: size of array must be divisible by product of known dims",
                             0, 0, "reshape", "", "m:reshape:indivisible");
            dims[inferPos] = x.numel() / knownProd;
        }
    }

    // Strip trailing 1s past the 2nd dim (MATLAB convention).
    stripTrailingOnes(dims);
    outs[0] = reshapeND(x, dims.data(), dims.size(), mr);
}

void transpose_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("transpose: requires 1 argument",
                     0, 0, "transpose", "", "m:transpose:nargin");
    outs[0] = transpose(args[0], ctx.engine->resource());
}

void pagetranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                       CallContext &ctx)
{
    if (args.empty())
        throw Error("pagetranspose: requires 1 argument",
                     0, 0, "pagetranspose", "", "m:pagetranspose:nargin");
    outs[0] = pagetranspose(args[0], ctx.engine->resource());
}

void pagectranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                        CallContext &ctx)
{
    if (args.empty())
        throw Error("pagectranspose: requires 1 argument",
                     0, 0, "pagectranspose", "", "m:pagectranspose:nargin");
    outs[0] = pagectranspose(args[0], ctx.engine->resource());
}

void peaks_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    size_t n = 49;  // MATLAB default
    if (!args.empty()) {
        const double dn = args[0].toScalar();
        if (dn < 0 || dn > 1.0e9 || std::isnan(dn))
            throw Error("peaks: n must be a non-negative integer",
                         0, 0, "peaks", "", "m:peaks:badN");
        n = static_cast<size_t>(dn);
    }
    outs[0] = peaks(n, ctx.engine->resource());
}

void sphere_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    size_t n = 20;  // MATLAB default
    if (!args.empty()) n = static_cast<size_t>(args[0].toScalar());
    auto s = sphere(n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void cylinder_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    size_t n = 20;
    Value R;
    if (args.empty()) {
        // Default profile [1 1].
        R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        R.doubleDataMut()[0] = 1.0;
        R.doubleDataMut()[1] = 1.0;
    } else {
        if (args[0].isScalar()) {
            // cylinder(n) — single integer arg is `n`, R defaults to [1 1].
            n = static_cast<size_t>(args[0].toScalar());
            R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            R.doubleDataMut()[0] = 1.0;
            R.doubleDataMut()[1] = 1.0;
        } else {
            R = args[0];
            if (args.size() >= 2)
                n = static_cast<size_t>(args[1].toScalar());
        }
    }
    auto s = cylinder(R, n, mr);
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void ellipsoid_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 6)
        throw Error("ellipsoid: requires (xc, yc, zc, xr, yr, zr [, n])",
                     0, 0, "ellipsoid", "", "m:ellipsoid:nargin");
    const double xc = args[0].toScalar();
    const double yc = args[1].toScalar();
    const double zc = args[2].toScalar();
    const double xr = args[3].toScalar();
    const double yr = args[4].toScalar();
    const double zr = args[5].toScalar();
    size_t n = 20;
    if (args.size() >= 7) n = static_cast<size_t>(args[6].toScalar());
    auto s = ellipsoid(xc, yc, zc, xr, yr, zr, n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void pagemtimes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto parseFlag = [](const Value &v) -> TranspOp {
        if (!v.isChar() && !v.isString())
            throw Error("pagemtimes: transpose flag must be a string",
                         0, 0, "pagemtimes", "", "m:pagemtimes:flagType");
        const std::string s = v.toString();
        if (s == "none")       return TranspOp::None;
        if (s == "transpose")  return TranspOp::Transpose;
        if (s == "ctranspose") return TranspOp::CTranspose;
        throw Error("pagemtimes: invalid transpose flag '" + s
                     + "' (expected 'none', 'transpose', or 'ctranspose')",
                     0, 0, "pagemtimes", "", "m:pagemtimes:invalidFlag");
    };
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = pagemtimes(args[0], args[1], mr);
        return;
    }
    if (args.size() == 4) {
        outs[0] = pagemtimes(args[0], parseFlag(args[1]), args[2], parseFlag(args[3]), mr);
        return;
    }
    throw Error("pagemtimes: expected 2 or 4 arguments",
                 0, 0, "pagemtimes", "", "m:pagemtimes:nargin");
}

void diag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diag: requires 1 argument",
                     0, 0, "diag", "", "m:diag:nargin");
    outs[0] = diag(args[0], ctx.engine->resource());
}

void sort_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sort: requires 1 argument",
                     0, 0, "sort", "", "m:sort:nargin");
    auto [sorted, idx] = sort(args[0], ctx.engine->resource());
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void sortrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sortrows: requires at least 1 argument",
                     0, 0, "sortrows", "", "m:sortrows:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto cols = ScratchVec<int>(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const auto &c = args[1];
        if (c.type() == ValueType::CHAR || c.type() == ValueType::STRING)
            throw Error("sortrows: column spec must be numeric",
                         0, 0, "sortrows", "", "m:sortrows:badColType");
        cols.reserve(c.numel());
        for (size_t i = 0; i < c.numel(); ++i) {
            const double v = c.elemAsDouble(i);
            if (v != std::floor(v))
                throw Error("sortrows: column index must be an integer",
                             0, 0, "sortrows", "", "m:sortrows:badCol");
            cols.push_back(static_cast<int>(v));
        }
    }
    auto [sorted, idx] = sortrows(args[0], cols.data(), cols.size(), mr);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void find_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("find: requires 1 argument",
                     0, 0, "find", "", "m:find:nargin");
    outs[0] = find(args[0], ctx.engine->resource());
}

void nnz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nnz: requires 1 argument",
                     0, 0, "nnz", "", "m:nnz:nargin");
    outs[0] = nnz(args[0], ctx.engine->resource());
}

void nonzeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nonzeros: requires 1 argument",
                     0, 0, "nonzeros", "", "m:nonzeros:nargin");
    outs[0] = nonzeros(args[0], ctx.engine->resource());
}

void horzcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = horzcat(args, ctx.engine->resource());
}

void vertcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = vertcat(args, ctx.engine->resource());
}

void meshgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("meshgrid: requires at least 1 argument",
                     0, 0, "meshgrid", "", "m:meshgrid:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // meshgrid(x) ≡ meshgrid(x, x). See BUGS.md #21.
        auto [X, Y] = meshgrid(args[0], args[0], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 2) {
        auto [X, Y] = meshgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = meshgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("meshgrid: 4+ inputs are not supported",
                 0, 0, "meshgrid", "", "m:meshgrid:tooManyInputs");
}

void ndgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ndgrid: requires at least 2 arguments",
                     0, 0, "ndgrid", "", "m:ndgrid:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        auto [X, Y] = ndgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = ndgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("ndgrid: 4+ inputs are not yet supported",
                 0, 0, "ndgrid", "", "m:ndgrid:tooManyInputs");
}

void kron_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kron: requires 2 arguments",
                     0, 0, "kron", "", "m:kron:nargin");
    outs[0] = kron(args[0], args[1], ctx.engine->resource());
}

void cumsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumsum: requires at least 1 argument",
                     0, 0, "cumsum", "", "m:cumsum:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    outs[0] = (dim > 0) ? cumsum(args[0], dim, ctx.engine->resource())
                        : cumsum(args[0], ctx.engine->resource());
}

#define NK_CUM_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = name(args[0], dim, ctx.engine->resource());                 \
    }

NK_CUM_REG(cumprod)

// MATLAB cummax / cummin accept positional 'reverse' / 'omitnan' /
// 'includenan' string flags after the optional dim. Trick: 'reverse'
// = flip + cum + flip; 'includenan' propagation requires a second pass
// that fills NaN forward from the first NaN onwards (since the cum*
// kernel itself already skips NaN per omitnan default).
namespace {

void parseCumDirNan(Span<const Value> args, size_t start,
                    int &dim, bool &reverse, bool &include_nan)
{
    dim = 0;
    reverse = false;
    include_nan = false;        // matches numkit default = MATLAB default
    size_t i = start;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    while (i < args.size()) {
        if (!(args[i].isChar() || args[i].isString())) {
            throw Error("cummax/cummin: trailing positional must be a string flag",
                        0, 0, "cummax/cummin", "", "m:cum:badArg");
        }
        std::string s = args[i].toString();
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "reverse")    reverse = true;
        else if (s == "forward")    reverse = false;
        else if (s == "omitnan")    include_nan = false;
        else if (s == "includenan") include_nan = true;
        else
            throw Error("cummax/cummin: unknown flag '" + s + "'",
                        0, 0, "cummax/cummin", "", "m:cum:flag");
        ++i;
    }
}

// Propagate NaN forward in `out` based on the NaN positions in the
// (already same-shape) `src` input. Used to implement 'includenan'
// for cummax/cummin: once a NaN is hit in src along the operating
// dim, every subsequent output entry is set to NaN.
void propagateNanFromSrc(Value &out, const Value &src, int dim1Based)
{
    const auto &dd = out.dims();
    const int nd = dd.ndim();
    const int d = dim1Based;
    if (d < 1 || d > nd) return;
    size_t inner = 1;
    for (int i = 0; i < d - 1; ++i) inner *= dd.dim(i);
    size_t outer = 1;
    for (int i = d; i < nd; ++i) outer *= dd.dim(i);
    const size_t L = dd.dim(d - 1);
    double *o = out.doubleDataMut();
    const double *s = src.doubleData();
    for (size_t oc = 0; oc < outer; ++oc)
        for (size_t b = 0; b < inner; ++b) {
            const size_t base = oc * inner * L + b;
            bool seenNaN = false;
            for (size_t k = 0; k < L; ++k) {
                if (!seenNaN && std::isnan(s[base + k * inner]))
                    seenNaN = true;
                if (seenNaN)
                    o[base + k * inner] = std::numeric_limits<double>::quiet_NaN();
            }
        }
}

template <typename Fn>
Value runCumWithFlags(const Value &x, Span<const Value> args, Fn impl, std::pmr::memory_resource *mr)
{
    int dim; bool reverse; bool include_nan;
    parseCumDirNan(args, 1, dim, reverse, include_nan);
    Value src = x;
    if (reverse) src = flip(src, dim, mr);
    Value out = (dim > 0) ? impl(src, dim, mr) : impl(src, 0, mr);
    if (include_nan) {
        // Determine effective dim (firstNonSingleton when dim=0).
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        propagateNanFromSrc(out, src, effDim);
    }
    if (reverse) {
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        out = flip(out, effDim, mr);
    }
    return out;
}

} // anonymous

void cummax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummax: requires at least 1 argument",
                     0, 0, "cummax", "", "m:cummax:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummax(v, d, mr);
                              }, ctx.engine->resource());
}

void cummin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummin: requires at least 1 argument",
                     0, 0, "cummin", "", "m:cummin:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummin(v, d, mr);
                              }, ctx.engine->resource());
}

void diff_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diff: requires at least 1 argument",
                     0, 0, "diff", "", "m:diff:nargin");
    int n = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double nv = args[1].toScalar();
        if (nv != std::floor(nv) || nv < 0)
            throw Error("diff: order n must be a non-negative integer",
                         0, 0, "diff", "", "m:diff:badOrder");
        n = static_cast<int>(nv);
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = diff(args[0], n, dim, ctx.engine->resource());
}

#undef NK_CUM_REG

#define NK_LOGICAL_RED_REG(name, fn)                                           \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                   \
    }

NK_LOGICAL_RED_REG(any, anyOf)
NK_LOGICAL_RED_REG(all, allOf)

#undef NK_LOGICAL_RED_REG

void xor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xor: requires 2 arguments",
                     0, 0, "xor", "", "m:xor:nargin");
    outs[0] = xorOf(args[0], args[1], ctx.engine->resource());
}

void cross_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cross: requires 2 arguments",
                     0, 0, "cross", "", "m:cross:nargin");
    outs[0] = cross(args[0], args[1], ctx.engine->resource());
}

void dot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dot: requires 2 arguments",
                     0, 0, "dot", "", "m:dot:nargin");
    outs[0] = dot(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
