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
