// libs/builtin/src/language/operators/la_solve.cpp

#include "la_solve.hpp"

#include <numkit/core/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace numkit::builtin::detail {

namespace {

// ── LU with partial pivoting (square A, n×n) ─────────────────────────
//
// Decomposes A in place into combined L (unit-diagonal, below) and U
// (above). piv[k] = row that was swapped into row k at step k.
// Returns false if a zero pivot is encountered (singular).
bool lu_partial_pivot(double *A, std::int32_t *piv, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        // Pivot search: largest |A[i,k]| for i in [k, n).
        std::size_t pivot_row = k;
        double pivot_val = std::fabs(A[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(A[i + k * n]);
            if (v > pivot_val) { pivot_val = v; pivot_row = i; }
        }
        if (pivot_val == 0.0) return false;
        piv[k] = static_cast<std::int32_t>(pivot_row);
        if (pivot_row != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(A[k + j * n], A[pivot_row + j * n]);
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

// Apply piv to a length-n column: bx[k] <-> bx[piv[k]] in step order.
inline void apply_piv(const std::int32_t *piv, double *bx, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t p = static_cast<std::size_t>(piv[k]);
        if (p != k) std::swap(bx[k], bx[p]);
    }
}

// Solve LU·x = P·b in place. LU has unit-diagonal L below, U above.
void lu_solve_one(const double *LU, const std::int32_t *piv,
                  double *bx, std::size_t n)
{
    apply_piv(piv, bx, n);
    // Forward solve: L·z = P·b (L unit-diagonal)
    for (std::size_t i = 1; i < n; ++i) {
        double s = bx[i];
        for (std::size_t k = 0; k < i; ++k) s -= LU[i + k * n] * bx[k];
        bx[i] = s;
    }
    // Backward solve: U·x = z
    for (std::size_t i = n; i-- > 0;) {
        double s = bx[i];
        for (std::size_t k = i + 1; k < n; ++k) s -= LU[i + k * n] * bx[k];
        bx[i] = s / LU[i + i * n];
    }
}

// ── QR via Householder (tall A, m×n with m >= n) ─────────────────────
//
// In-place: A becomes the upper-triangular R (top n×n) plus garbage below;
// B (m×nrhs) becomes Q^T·B (only first n rows are used). Then back-solve
// R·X = (Q^T·B)[0:n] gives the least-squares solution X (n×nrhs).
//
// Returns false if a zero column-norm is hit (rank-deficient A) or if
// the resulting R has a zero diagonal entry.
bool qr_solve_house(std::pmr::memory_resource *mr,
                    double *A, std::size_t m, std::size_t n,
                    double *B, std::size_t nrhs,
                    double *X)
{
    ScratchVec<double> v(m, mr);

    for (std::size_t k = 0; k < n; ++k) {
        // Compute ||A[k:m, k]||
        double norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) {
            const double e = A[i + k * m];
            norm_sq += e * e;
        }
        if (norm_sq == 0.0) return false;

        const double xk    = A[k + k * m];
        const double norm  = std::sqrt(norm_sq);
        // Choose sign of alpha to avoid cancellation in (xk - alpha).
        const double alpha = (xk >= 0.0) ? -norm : norm;

        // Build full Householder vector v[k:m]: v[k] = xk - alpha, v[i>k] = A[i+k*m].
        v[k] = xk - alpha;
        for (std::size_t i = k + 1; i < m; ++i) v[i] = A[i + k * m];

        double v_norm_sq = v[k] * v[k];
        for (std::size_t i = k + 1; i < m; ++i) v_norm_sq += v[i] * v[i];
        if (v_norm_sq == 0.0) {
            A[k + k * m] = alpha;
            continue;
        }
        const double tau = 2.0 / v_norm_sq;

        // Apply H = I - tau·v·v^T to A[k:m, k+1:n] (skip column k — we'll
        // overwrite its diagonal with alpha and don't need the rest).
        for (std::size_t j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i) dot += v[i] * A[i + j * m];
            const double s = tau * dot;
            for (std::size_t i = k; i < m; ++i) A[i + j * m] -= s * v[i];
        }
        // Apply H to B[k:m, :]
        for (std::size_t j = 0; j < nrhs; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i) dot += v[i] * B[i + j * m];
            const double s = tau * dot;
            for (std::size_t i = k; i < m; ++i) B[i + j * m] -= s * v[i];
        }

        A[k + k * m] = alpha;  // Diagonal entry of R.
    }

    // Back-solve R·X = (Q^T·B)[0:n, :] where R is upper-triangular n×n
    // stored in the top of A (column-major, leading-dim m).
    for (std::size_t j = 0; j < nrhs; ++j) {
        for (std::size_t i = n; i-- > 0;) {
            double s = B[i + j * m];
            for (std::size_t k = i + 1; k < n; ++k)
                s -= A[i + k * m] * X[k + j * n];
            const double r_ii = A[i + i * m];
            if (r_ii == 0.0) return false;
            X[i + j * n] = s / r_ii;
        }
    }
    return true;
}

} // anonymous namespace

bool la_solve(std::pmr::memory_resource *mr,
              const double *A, std::size_t m, std::size_t n,
              const double *B, std::size_t nrhs,
              double *X)
{
    if (m < n || m == 0 || n == 0 || nrhs == 0) return false;

    ScratchArena arena(mr);

    if (m == n) {
        // Square: LU with partial pivoting.
        ScratchVec<double> A_lu(n * n, &arena);
        std::copy(A, A + n * n, A_lu.begin());
        ScratchVec<std::int32_t> piv(n, &arena);
        if (!lu_partial_pivot(A_lu.data(), piv.data(), n)) return false;
        for (std::size_t j = 0; j < nrhs; ++j) {
            double *xj = X + j * n;
            for (std::size_t i = 0; i < n; ++i) xj[i] = B[i + j * n];
            lu_solve_one(A_lu.data(), piv.data(), xj, n);
        }
        return true;
    }

    // Tall (m > n): QR via Householder + back-solve.
    ScratchVec<double> A_qr(m * n, &arena);
    ScratchVec<double> B_qr(m * nrhs, &arena);
    std::copy(A, A + m * n, A_qr.begin());
    std::copy(B, B + m * nrhs, B_qr.begin());
    return qr_solve_house(&arena, A_qr.data(), m, n,
                          B_qr.data(), nrhs, X);
}

} // namespace numkit::builtin::detail
