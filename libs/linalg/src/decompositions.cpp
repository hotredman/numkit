// libs/linalg/src/decompositions.cpp
//
// chol / lu / qr / svd — implementations and engine adapters.
// Migrated from libs/builtin/src/language/arrays/matrix.cpp.

#include <numkit/linalg/decompositions.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace numkit::linalg {

// ────────────────────────────────────────────────────────────────────────
// Cholesky
// ────────────────────────────────────────────────────────────────────────

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
        double s = a[j + j * n];
        for (std::size_t k = 0; k < j; ++k)
            s -= r[k + j * n] * r[k + j * n];
        if (s <= 0.0)
            throw Error("chol: matrix is not positive-definite",
                        0, 0, "chol", "", "m:chol:notPosDef");
        r[j + j * n] = std::sqrt(s);
        const double inv_diag = 1.0 / r[j + j * n];
        for (std::size_t i = j + 1; i < n; ++i) {
            double t = a[j + i * n];
            for (std::size_t k = 0; k < j; ++k)
                t -= r[k + j * n] * r[k + i * n];
            r[j + i * n] = t * inv_diag;
        }
    }
    return R;
}

// ────────────────────────────────────────────────────────────────────────
// LU
// ────────────────────────────────────────────────────────────────────────

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
        L[i + i * n] = 1.0;
        for (std::size_t j = 0; j < i; ++j)
            L[i + j * n] = LU[i + j * n];
        for (std::size_t j = i; j < n; ++j)
            U[i + j * n] = LU[i + j * n];
    }
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

// ────────────────────────────────────────────────────────────────────────
// QR (Householder, full Q)
// ────────────────────────────────────────────────────────────────────────

namespace {

// Householder QR with explicit Q construction. Decomposes m×n A
// (m >= n) into Q (m×m orthogonal) and R (m×n upper-triangular).
void qrFullHouseholder(const double *A_in, std::size_t m, std::size_t n,
                       double *Qout, double *Rout,
                       std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<double> R_work(m * n, &scratch);
    ScratchVec<double> V(m * n, 0.0, &scratch);
    ScratchVec<double> tau(n, 0.0, &scratch);
    std::copy(A_in, A_in + m * n, R_work.begin());

    for (std::size_t k = 0; k < n; ++k) {
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
        for (std::size_t j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i)
                dot += V[i + k * m] * R_work[i + j * m];
            const double s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i)
                R_work[i + j * m] -= s * V[i + k * m];
        }
        R_work[k + k * m] = alpha;
        for (std::size_t i = k + 1; i < m; ++i)
            R_work[i + k * m] = 0.0;
    }

    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i)
            Rout[i + j * m] = (i <= j) ? R_work[i + j * m] : 0.0;

    std::fill(Qout, Qout + m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i)
        Qout[i + i * m] = 1.0;
    for (std::size_t kk = n; kk-- > 0;) {
        const std::size_t k = kk;
        if (tau[k] == 0.0) continue;
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

// ────────────────────────────────────────────────────────────────────────
// SVD (one-sided Jacobi)
// ────────────────────────────────────────────────────────────────────────

namespace {

// One-sided Jacobi SVD: rotates columns of A until they become orthogonal.
// Caller normalises columns and assembles U, S, V.
void jacobiSvdInplace(double *A, std::size_t m, std::size_t n,
                      double *V, std::size_t maxSweeps, double tol)
{
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

                double c, s;
                if (alpha == beta) {
                    c = 0.7071067811865476;
                    s = (gamma >= 0.0 ? 1.0 : -1.0) * c;
                } else {
                    const double tau = (beta - alpha) / (2.0 * gamma);
                    const double t = (tau >= 0.0)
                        ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
                        : 1.0 / (tau - std::sqrt(1.0 + tau * tau));
                    c = 1.0 / std::sqrt(1.0 + t * t);
                    s = t * c;
                }

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

    const bool transposed = (m_in < n_in);
    const std::size_t m = transposed ? n_in : m_in;
    const std::size_t n = transposed ? m_in : n_in;

    ScratchArena scratch(mr);
    ScratchVec<double> A_work(m * n, &scratch);
    ScratchVec<double> V_work(n * n, &scratch);

    const double *A_data = A.doubleData();
    if (!transposed) {
        std::copy(A_data, A_data + m * n, A_work.begin());
    } else {
        for (std::size_t j = 0; j < n_in; ++j)
            for (std::size_t i = 0; i < m_in; ++i)
                A_work[j + i * n_in] = A_data[i + j * m_in];
    }

    jacobiSvdInplace(A_work.data(), m, n, V_work.data(),
                     /*maxSweeps=*/64,
                     /*tol=*/1e-13);

    ScratchVec<double> sigma(n, &scratch);
    ScratchVec<std::size_t> order(n, &scratch);
    for (std::size_t k = 0; k < n; ++k) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i)
            s += A_work[i + k * m] * A_work[i + k * m];
        sigma[k] = std::sqrt(s);
        order[k] = k;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return sigma[a] > sigma[b]; });

    auto Uout = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Sout = Value::matrix(m, n, ValueType::DOUBLE, mr);
    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *U = Uout.doubleDataMut();
    double *S = Sout.doubleDataMut();
    double *V = Vout.doubleDataMut();
    std::fill(U, U + m * m, 0.0);
    std::fill(S, S + m * n, 0.0);
    std::fill(V, V + n * n, 0.0);

    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t src = order[k];
        S[k + k * m] = sigma[src];
        if (sigma[src] > 0.0) {
            const double inv_s = 1.0 / sigma[src];
            for (std::size_t i = 0; i < m; ++i)
                U[i + k * m] = A_work[i + src * m] * inv_s;
        } else {
            for (std::size_t i = 0; i < m; ++i) U[i + k * m] = 0.0;
        }
        for (std::size_t i = 0; i < n; ++i)
            V[i + k * n] = V_work[i + src * n];
    }

    // Gram-Schmidt completion for m > n.
    for (std::size_t k = n; k < m; ++k) {
        for (std::size_t i = 0; i < m; ++i) {
            ScratchVec<double> v(m, 0.0, &scratch);
            v[i] = 1.0;
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
    // Transposed case fix-up: SVD(A^T) ↔ SVD(A) with U/V swapped and S transposed.
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

// ────────────────────────────────────────────────────────────────────────
// QR rank-1 update / column insert / column delete
// (Daniel-Gragg-Kaufman-Stewart 1976; Golub & Van Loan §6.5)
// ────────────────────────────────────────────────────────────────────────

namespace {

// Compute the Givens rotation (c, s) that zeros y in [x; y] → [r; 0].
// Returns r = hypot(x, y). c = x/r, s = y/r. Edge-case-safe.
inline void givens(double x, double y, double &c, double &s, double &r)
{
    if (y == 0.0) { c = (x >= 0.0) ? 1.0 : -1.0; s = 0.0; r = std::fabs(x); return; }
    if (x == 0.0) { c = 0.0; s = (y >= 0.0) ? 1.0 : -1.0; r = std::fabs(y); return; }
    r = std::hypot(x, y);
    c = x / r;
    s = y / r;
}

// Apply Givens rotation (c, s) to rows i and j of an m×n matrix M
// (column-major). Rotates row i ← c*row_i + s*row_j;
//                  row j ← -s*row_i + c*row_j.
void applyGivensRows(double *M, std::size_t leadDim, std::size_t ncols,
                     std::size_t i, std::size_t j, double c, double s)
{
    for (std::size_t k = 0; k < ncols; ++k) {
        const double a = M[i + k * leadDim];
        const double b = M[j + k * leadDim];
        M[i + k * leadDim] =  c * a + s * b;
        M[j + k * leadDim] = -s * a + c * b;
    }
}

// Apply Givens rotation (c, s) to columns i and j of an m×n matrix M
// (column-major). col_i ← c*col_i + s*col_j; col_j ← -s*col_i + c*col_j.
void applyGivensCols(double *M, std::size_t leadDim, std::size_t nrows,
                     std::size_t i, std::size_t j, double c, double s)
{
    for (std::size_t k = 0; k < nrows; ++k) {
        const double a = M[k + i * leadDim];
        const double b = M[k + j * leadDim];
        M[k + i * leadDim] =  c * a + s * b;
        M[k + j * leadDim] = -s * a + c * b;
    }
}

} // anonymous namespace

std::tuple<Value, Value>
qrupdate(const Value &Q, const Value &R, const Value &u, const Value &v,
         std::pmr::memory_resource *mr)
{
    if (Q.dims().ndim() != 2 || R.dims().ndim() != 2)
        throw Error("qrupdate: Q and R must be 2D matrices",
                    0, 0, "qrupdate", "", "m:qrupdate:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t mq = static_cast<std::size_t>(Q.dims().dim(1));
    const std::size_t mr2 = static_cast<std::size_t>(R.dims().dim(0));
    const std::size_t n  = static_cast<std::size_t>(R.dims().dim(1));
    if (mq != m || mr2 != m)
        throw Error("qrupdate: Q must be m×m and R must be m×n",
                    0, 0, "qrupdate", "", "m:qrupdate:badShape");
    if (u.numel() != m || v.numel() != n)
        throw Error("qrupdate: length(u) must equal size(R, 1) and length(v) must equal size(R, 2)",
                    0, 0, "qrupdate", "", "m:qrupdate:badVec");

    auto Q1 = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto R1 = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *Qd = Q1.doubleDataMut();
    double *Rd = R1.doubleDataMut();
    std::copy(Q.doubleData(), Q.doubleData() + m * m, Qd);
    std::copy(R.doubleData(), R.doubleData() + m * n, Rd);

    ScratchArena scratch(mr);
    ScratchVec<double> w(m, &scratch);
    // w = Q' * u
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t k = 0; k < m; ++k) s += Qd[k + i * m] * u.elemAsDouble(k);
        w[i] = s;
    }

    // Bring w down to a single non-zero in position 0 via Givens
    // rotations chained from bottom (m-1) to (0). Apply mirrored
    // rotations to Q (cols) and R (rows) to preserve A = Q · R.
    for (std::size_t i = m - 1; i > 0; --i) {
        double c, s, r;
        givens(w[i - 1], w[i], c, s, r);
        w[i - 1] = r;
        w[i] = 0.0;
        // Row rotation on R (rows i-1 and i, all n columns)
        applyGivensRows(Rd, m, n, i - 1, i, c, s);
        // Mirror on Q (cols i-1 and i) so A = Q*R invariant
        applyGivensCols(Qd, m, m, i - 1, i, c, s);
    }

    // R is now upper Hessenberg (one sub-diagonal below). Add
    // w[0] * v' to the first row: R[0, j] += w[0] * v(j).
    const double w0 = w[0];
    for (std::size_t j = 0; j < n; ++j)
        Rd[0 + j * m] += w0 * v.elemAsDouble(j);

    // Restore upper triangular form: chase the sub-diagonal bulge
    // top-down with Givens rotations on rows of R + cols of Q.
    const std::size_t kmax = std::min(m - 1, n);
    for (std::size_t i = 0; i < kmax; ++i) {
        double c, s, r;
        givens(Rd[i + i * m], Rd[i + 1 + i * m], c, s, r);
        Rd[i + i * m] = r;
        Rd[i + 1 + i * m] = 0.0;
        if (i + 1 < n)
            applyGivensRows(Rd + (i + 1) * m, m, n - i - 1, i, i + 1, c, s);
        applyGivensCols(Qd, m, m, i, i + 1, c, s);
    }
    return std::make_tuple(std::move(Q1), std::move(R1));
}

std::tuple<Value, Value>
qrinsert(const Value &Q, const Value &R, std::size_t k_1based, const Value &x,
         std::pmr::memory_resource *mr)
{
    if (Q.dims().ndim() != 2 || R.dims().ndim() != 2)
        throw Error("qrinsert: Q and R must be 2D matrices",
                    0, 0, "qrinsert", "", "m:qrinsert:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(1));
    if (static_cast<std::size_t>(Q.dims().dim(1)) != m
        || static_cast<std::size_t>(R.dims().dim(0)) != m)
        throw Error("qrinsert: Q must be m×m and R must be m×n",
                    0, 0, "qrinsert", "", "m:qrinsert:badShape");
    if (x.numel() != m)
        throw Error("qrinsert: x must have length size(Q, 1)",
                    0, 0, "qrinsert", "", "m:qrinsert:badX");
    if (k_1based < 1 || k_1based > n + 1)
        throw Error("qrinsert: k must be in 1..n+1",
                    0, 0, "qrinsert", "", "m:qrinsert:badK");
    const std::size_t k = k_1based - 1;

    // y = Q' * x (length m).
    ScratchArena scratch(mr);
    ScratchVec<double> y(m, &scratch);
    const double *Qd_in = Q.doubleData();
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t r2 = 0; r2 < m; ++r2)
            s += Qd_in[r2 + i * m] * x.elemAsDouble(r2);
        y[i] = s;
    }

    // Build new R (m × n+1) by inserting y at column k between R(:,0:k-1)
    // and R(:,k:n-1). Q stays m × m (will be updated in place by mirrored
    // col rotations).
    auto R1 = Value::matrix(m, n + 1, ValueType::DOUBLE, mr);
    double *Rd = R1.doubleDataMut();
    const double *Rd_in = R.doubleData();
    for (std::size_t j = 0; j < k; ++j)
        std::copy(Rd_in + j * m, Rd_in + (j + 1) * m, Rd + j * m);
    std::copy(y.begin(), y.end(), Rd + k * m);
    for (std::size_t j = k; j < n; ++j)
        std::copy(Rd_in + j * m, Rd_in + (j + 1) * m, Rd + (j + 1) * m);

    auto Q1 = Value::matrix(m, m, ValueType::DOUBLE, mr);
    double *Qd = Q1.doubleDataMut();
    std::copy(Qd_in, Qd_in + m * m, Qd);

    // Column k of R has entries y[k], y[k+1], ..., y[m-1] below the
    // diagonal. Zero them from bottom up via Givens rotations.
    for (std::size_t i = m - 1; i > k; --i) {
        double c, s, r;
        givens(Rd[(i - 1) + k * m], Rd[i + k * m], c, s, r);
        Rd[(i - 1) + k * m] = r;
        Rd[i + k * m] = 0.0;
        // Rotate rows i-1, i across cols k+1 .. n of R (the new
        // column count is n+1, so we touch [k+1, n]).
        if (k + 1 <= n)
            applyGivensRows(Rd + (k + 1) * m, m, n - k, i - 1, i, c, s);
        // Mirror on Q's cols i-1, i.
        applyGivensCols(Qd, m, m, i - 1, i, c, s);
    }
    return std::make_tuple(std::move(Q1), std::move(R1));
}

std::tuple<Value, Value>
qrdelete(const Value &Q, const Value &R, std::size_t k_1based,
         std::pmr::memory_resource *mr)
{
    if (Q.dims().ndim() != 2 || R.dims().ndim() != 2)
        throw Error("qrdelete: Q and R must be 2D matrices",
                    0, 0, "qrdelete", "", "m:qrdelete:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(1));
    if (static_cast<std::size_t>(Q.dims().dim(1)) != m
        || static_cast<std::size_t>(R.dims().dim(0)) != m)
        throw Error("qrdelete: Q must be m×m and R must be m×n",
                    0, 0, "qrdelete", "", "m:qrdelete:badShape");
    if (n == 0)
        throw Error("qrdelete: R has no columns to delete",
                    0, 0, "qrdelete", "", "m:qrdelete:empty");
    if (k_1based < 1 || k_1based > n)
        throw Error("qrdelete: k must be in 1..n",
                    0, 0, "qrdelete", "", "m:qrdelete:badK");
    const std::size_t k = k_1based - 1;

    // New R is m × (n-1) — drop column k, shift columns k+1..n-1 left.
    auto R1 = Value::matrix(m, n - 1, ValueType::DOUBLE, mr);
    double *Rd = R1.doubleDataMut();
    const double *Rd_in = R.doubleData();
    for (std::size_t j = 0; j < k; ++j)
        std::copy(Rd_in + j * m, Rd_in + (j + 1) * m, Rd + j * m);
    for (std::size_t j = k + 1; j < n; ++j)
        std::copy(Rd_in + j * m, Rd_in + (j + 1) * m, Rd + (j - 1) * m);

    auto Q1 = Value::matrix(m, m, ValueType::DOUBLE, mr);
    double *Qd = Q1.doubleDataMut();
    std::copy(Q.doubleData(), Q.doubleData() + m * m, Qd);

    // After dropping column k, rows k..min(m-1, n-1) have a "bulge"
    // immediately below the diagonal that needs to be chased away
    // with Givens rotations.
    const std::size_t lim = std::min(m - 1, n - 1);
    for (std::size_t i = k; i < lim; ++i) {
        double c, s, r;
        givens(Rd[i + i * m], Rd[i + 1 + i * m], c, s, r);
        Rd[i + i * m] = r;
        Rd[i + 1 + i * m] = 0.0;
        if (i + 1 < n - 1)
            applyGivensRows(Rd + (i + 1) * m, m, (n - 1) - (i + 1),
                            i, i + 1, c, s);
        applyGivensCols(Qd, m, m, i, i + 1, c, s);
    }
    return std::make_tuple(std::move(Q1), std::move(R1));
}

// ────────────────────────────────────────────────────────────────────────
// cholupdate — rank-1 Cholesky update/downdate
// ────────────────────────────────────────────────────────────────────────

Value cholupdate(const Value &R, const Value &x, int sign,
                 std::pmr::memory_resource *mr)
{
    if (R.dims().ndim() != 2)
        throw Error("cholupdate: R must be a 2D matrix",
                    0, 0, "cholupdate", "", "m:cholupdate:notMatrix");
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(0));
    if (n != static_cast<std::size_t>(R.dims().dim(1)))
        throw Error("cholupdate: R must be square",
                    0, 0, "cholupdate", "", "m:cholupdate:notSquare");
    if (x.numel() != n)
        throw Error("cholupdate: x must have length equal to size(R, 1)",
                    0, 0, "cholupdate", "", "m:cholupdate:badX");
    if (sign != 1 && sign != -1)
        throw Error("cholupdate: sign must be '+' (1) or '-' (-1)",
                    0, 0, "cholupdate", "", "m:cholupdate:badSign");
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    if (sign == 1) {
        // Rank-1 update (Golub & Van Loan, Algorithm 6.5.1).
        // For k = 0..n-1:
        //   r = sqrt(R(k,k)^2 + x(k)^2)
        //   c = R(k,k) / r,  s = x(k) / r
        //   R(k,k) = r
        //   for j = k+1..n-1:
        //     R(k,j)' = c*R(k,j) + s*x(j)
        //     x(j)'   = c*x(j) - s*R(k,j)
        auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
        double *Rd = out.doubleDataMut();
        std::copy(R.doubleData(), R.doubleData() + n * n, Rd);

        ScratchArena scratch(mr);
        ScratchVec<double> xb(n, &scratch);
        for (std::size_t i = 0; i < n; ++i) xb[i] = x.elemAsDouble(i);

        for (std::size_t k = 0; k < n; ++k) {
            const double Rkk = Rd[k + k * n];
            const double xk  = xb[k];
            const double r   = std::hypot(Rkk, xk);
            if (r == 0.0) continue;
            const double c = Rkk / r;
            const double s = xk  / r;
            Rd[k + k * n] = r;
            for (std::size_t j = k + 1; j < n; ++j) {
                const double Rkj = Rd[k + j * n];
                Rd[k + j * n] = c * Rkj + s * xb[j];
                xb[j]         = c * xb[j] - s * Rkj;
            }
        }
        return out;
    }

    // Downdate.
    //
    // KNOWN GAP: MATLAB uses LINPACK's stable Saunders 1972 method
    // (solve R'·p = x; Givens rotations on [α; p]). We use the
    // straight-forward O(n³) path: form B = R'·R − x·x' and chol(B).
    // Algebraically identical, numerically equivalent at machine
    // precision for well-conditioned R, and clearly signals
    // non-PD via chol's existing throw.
    ScratchArena scratch(mr);
    ScratchVec<double> B(n * n, &scratch);
    const double *Rd = R.doubleData();
    // B = R'·R
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i) {
            double sum = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                sum += Rd[k + i * n] * Rd[k + j * n];
            B[i + j * n] = sum;
        }
    // B -= x·x'
    for (std::size_t j = 0; j < n; ++j) {
        const double xj = x.elemAsDouble(j);
        for (std::size_t i = 0; i < n; ++i)
            B[i + j * n] -= x.elemAsDouble(i) * xj;
    }
    // Wrap B in a Value to feed chol().
    auto Btmp = Value::matrix(n, n, ValueType::DOUBLE, mr);
    std::copy(B.begin(), B.end(), Btmp.doubleDataMut());
    try {
        return chol(Btmp, mr);
    } catch (const Error &) {
        throw Error("cholupdate: downdate would break positive-definiteness",
                    0, 0, "cholupdate", "", "m:cholupdate:downdateFailed");
    }
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

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

void qrupdate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 4)
        throw Error("qrupdate: requires (Q, R, u, v)",
                    0, 0, "qrupdate", "", "m:qrupdate:nargin");
    auto [Q1, R1] = qrupdate(args[0], args[1], args[2], args[3], ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void qrinsert_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4 || args.size() > 5)
        throw Error("qrinsert: requires (Q, R, k, x[, 'col']) — row form is not yet supported",
                    0, 0, "qrinsert", "", "m:qrinsert:nargin");
    if (args.size() == 5) {
        if (!(args[4].isChar() || args[4].isString())
            || args[4].toString() != "col")
            throw Error("qrinsert: row form not supported in v1 — pass 'col' or omit",
                        0, 0, "qrinsert", "", "m:qrinsert:rowDeferred");
    }
    const double kd = args[2].toScalar();
    if (kd < 1.0 || kd != std::floor(kd))
        throw Error("qrinsert: k must be a positive integer",
                    0, 0, "qrinsert", "", "m:qrinsert:badK");
    auto [Q1, R1] = qrinsert(args[0], args[1],
                             static_cast<std::size_t>(kd), args[3],
                             ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void qrdelete_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3 || args.size() > 4)
        throw Error("qrdelete: requires (Q, R, k[, 'col']) — row form is not yet supported",
                    0, 0, "qrdelete", "", "m:qrdelete:nargin");
    if (args.size() == 4) {
        if (!(args[3].isChar() || args[3].isString())
            || args[3].toString() != "col")
            throw Error("qrdelete: row form not supported in v1 — pass 'col' or omit",
                        0, 0, "qrdelete", "", "m:qrdelete:rowDeferred");
    }
    const double kd = args[2].toScalar();
    if (kd < 1.0 || kd != std::floor(kd))
        throw Error("qrdelete: k must be a positive integer",
                    0, 0, "qrdelete", "", "m:qrdelete:badK");
    auto [Q1, R1] = qrdelete(args[0], args[1],
                             static_cast<std::size_t>(kd),
                             ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void cholupdate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("cholupdate: requires (R, x[, sign])",
                    0, 0, "cholupdate", "", "m:cholupdate:nargin");
    int sign = 1;
    if (args.size() == 3) {
        if (args[2].isChar() || args[2].isString()) {
            std::string s = args[2].toString();
            if      (s == "+") sign = 1;
            else if (s == "-") sign = -1;
            else throw Error("cholupdate: sign must be '+' or '-'",
                             0, 0, "cholupdate", "", "m:cholupdate:badSign");
        } else {
            sign = (args[2].toScalar() >= 0.0) ? 1 : -1;
        }
    }
    outs[0] = cholupdate(args[0], args[1], sign, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
