// toolboxes/linalg/src/decompositions.cpp
//
// chol / lu / qr / svd — implementations and engine adapters.
// Migrated from toolboxes/builtin/src/language/arrays/matrix.cpp.

#include <numkit/linalg/decompositions.hpp>
#include "decompositions_detail.hpp"   // shared raw-buffer kernels (private)

// Compute-only TU: Value substrate + Error, no engine. The chol / lu / qr /
// svd / qrupdate / qrinsert / qrdelete / cholupdate builtins (CallContext
// wrappers) live in decompositions_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace numkit::linalg {

// ────────────────────────────────────────────────────────────────────────
// Cholesky
// ────────────────────────────────────────────────────────────────────────

// Shared raw-buffer kernels (defined as inline templates in decompositions_detail.hpp)

Value chol(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("chol: input must be a 2D matrix",
                    0, 0, "chol", "", "numkit:chol:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("chol: matrix must be square",
                    0, 0, "chol", "", "numkit:chol:notSquare");
    if (m == 0)
        return Value::matrix(0, 0, A.type(), mr);

    if (A.isComplex()) {
        auto R = Value::complexMatrix(n, n, mr);
        if (cholUpperFactor(A.complexData(), R.complexDataMut(), n) != 0)
            throw Error("chol: matrix is not positive-definite",
                        0, 0, "chol", "", "numkit:chol:notPosDef");
        return detail::narrow_if_real(R);
    } else {
        auto R = Value::matrix(n, n, ValueType::DOUBLE, mr);
        if (cholUpperFactor(A.doubleData(), R.doubleDataMut(), n) != 0)
            throw Error("chol: matrix is not positive-definite",
                        0, 0, "chol", "", "numkit:chol:notPosDef");
        return R;
    }
}

// ────────────────────────────────────────────────────────────────────────
// LU
// ────────────────────────────────────────────────────────────────────────

namespace {

template <typename T>
std::tuple<Value, Value, Value>
lu_decompose_impl(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    ScratchArena scratch(mr);
    ScratchVec<T> LU(m * n, &scratch);
    ScratchVec<std::int32_t> piv(n, &scratch);
    const T *src = detail::get_data<T>(A);
    std::copy(src, src + m * n, LU.begin());
    if (!detail::luPivotInplace(LU.data(), piv.data(), n))
        throw Error("lu: matrix is singular",
                    0, 0, "lu", "", "numkit:lu:singular");

    Value Lout = detail::make_matrix<T>(n, n, mr);
    Value Uout = detail::make_matrix<T>(n, n, mr);
    Value Pout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    T *L = detail::get_data_mut<T>(Lout);
    T *U = detail::get_data_mut<T>(Uout);
    double *P = Pout.doubleDataMut();
    std::fill(L, L + n * n, T(0));
    std::fill(U, U + n * n, T(0));
    std::fill(P, P + n * n, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        L[i + i * n] = T(1);
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

    return std::make_tuple(detail::narrow_if_real(Lout), detail::narrow_if_real(Uout), std::move(Pout));
}

template <typename T>
Value lu_combined_impl(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    ScratchArena scratch(mr);
    ScratchVec<std::int32_t> piv(n, &scratch);
    Value out = detail::make_matrix<T>(n, n, mr);
    T *LU = detail::get_data_mut<T>(out);
    const T *src = detail::get_data<T>(A);
    std::copy(src, src + m * n, LU);
    if (!detail::luPivotInplace(LU, piv.data(), n))
        throw Error("lu: matrix is singular",
                    0, 0, "lu", "", "numkit:lu:singular");
    return detail::narrow_if_real(out);
}

} // anonymous namespace

std::tuple<Value, Value, Value>
lu_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("lu: input must be a 2D matrix",
                    0, 0, "lu", "", "numkit:lu:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("lu: square matrix required for [L,U,P] form",
                    0, 0, "lu", "", "numkit:lu:notSquare");

    if (A.isComplex()) {
        return lu_decompose_impl<detail::Complex>(A, mr);
    }
    return lu_decompose_impl<double>(A, mr);
}

Value lu_combined(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("lu: input must be a 2D matrix",
                    0, 0, "lu", "", "numkit:lu:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("lu: square matrix required",
                    0, 0, "lu", "", "numkit:lu:notSquare");

    if (A.isComplex()) {
        return lu_combined_impl<detail::Complex>(A, mr);
    }
    return lu_combined_impl<double>(A, mr);
}

// ────────────────────────────────────────────────────────────────────────
// QR (Householder, full Q)
// ────────────────────────────────────────────────────────────────────────

namespace {

// Householder QR with explicit Q construction. Decomposes m×n A
// (m >= n) into Q (m×m orthogonal/unitary) and R (m×n upper-triangular).
template <typename T>
void qrFullHouseholder(const T *A_in, std::size_t m, std::size_t n,
                       T *Qout, T *Rout,
                       std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<T> R_work(m * n, &scratch);
    ScratchVec<T> V(m * n, T(0), &scratch);
    ScratchVec<T> tau(n, T(0), &scratch);
    std::copy(A_in, A_in + m * n, R_work.begin());

    for (std::size_t k = 0; k < n; ++k) {
        double norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) {
            norm_sq += detail::abs_sq(R_work[i + k * m]);
        }
        if (norm_sq == 0.0) {
            tau[k] = T(0);
            continue;
        }
        const T xk = R_work[k + k * m];
        const double norm = std::sqrt(norm_sq);

        T alpha;
        if constexpr (detail::is_complex_v<T>) {
            const double abs_xk = std::abs(xk);
            const T phase = (abs_xk > 0.0) ? (xk / abs_xk) : T(1.0, 0.0);
            alpha = -phase * norm;
        } else {
            alpha = (xk >= 0.0) ? -norm : norm;
        }

        V[k + k * m] = xk - alpha;
        for (std::size_t i = k + 1; i < m; ++i)
            V[i + k * m] = R_work[i + k * m];

        double v_norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i)
            v_norm_sq += detail::abs_sq(V[i + k * m]);

        if (v_norm_sq == 0.0) {
            R_work[k + k * m] = alpha;
            tau[k] = T(0);
            continue;
        }
        tau[k] = T(2.0 / v_norm_sq);
        for (std::size_t j = k + 1; j < n; ++j) {
            T dot = T(0);
            for (std::size_t i = k; i < m; ++i) {
                if constexpr (detail::is_complex_v<T>) {
                    dot += std::conj(V[i + k * m]) * R_work[i + j * m];
                } else {
                    dot += V[i + k * m] * R_work[i + j * m];
                }
            }
            const T s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i)
                R_work[i + j * m] -= s * V[i + k * m];
        }
        R_work[k + k * m] = alpha;
        for (std::size_t i = k + 1; i < m; ++i)
            R_work[i + k * m] = T(0);
    }

    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i)
            Rout[i + j * m] = (i <= j) ? R_work[i + j * m] : T(0);

    std::fill(Qout, Qout + m * m, T(0));
    for (std::size_t i = 0; i < m; ++i)
        Qout[i + i * m] = T(1);

    for (std::size_t kk = n; kk-- > 0;) {
        const std::size_t k = kk;
        if (detail::abs_sq(tau[k]) == 0.0) continue;
        for (std::size_t j = 0; j < m; ++j) {
            T dot = T(0);
            for (std::size_t i = k; i < m; ++i) {
                if constexpr (detail::is_complex_v<T>) {
                    dot += std::conj(V[i + k * m]) * Qout[i + j * m];
                } else {
                    dot += V[i + k * m] * Qout[i + j * m];
                }
            }
            const T s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i)
                Qout[i + j * m] -= s * V[i + k * m];
        }
    }
}

// Column-pivoted Householder QR: A·P = Q·R
template <typename T>
void qrPivotedHouseholder(const T *A_in, std::size_t m, std::size_t n,
                          T *Qout, T *Rout, std::size_t *permOut,
                          std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<T> R_work(m * n, &scratch);
    ScratchVec<T> V(m * n, T(0), &scratch);
    ScratchVec<T> tau(n, T(0), &scratch);
    std::copy(A_in, A_in + m * n, R_work.begin());
    for (std::size_t j = 0; j < n; ++j) permOut[j] = j;

    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pj = k;
        double pmax = -1.0;
        for (std::size_t j = k; j < n; ++j) {
            double s = 0.0;
            for (std::size_t i = k; i < m; ++i) {
                s += detail::abs_sq(R_work[i + j * m]);
            }
            if (s > pmax) { pmax = s; pj = j; }
        }
        if (pj != k) {
            for (std::size_t i = 0; i < m; ++i)
                std::swap(R_work[i + k * m], R_work[i + pj * m]);
            std::swap(permOut[k], permOut[pj]);
        }
        double norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) {
            norm_sq += detail::abs_sq(R_work[i + k * m]);
        }
        if (norm_sq == 0.0) { tau[k] = T(0); continue; }
        const T xk = R_work[k + k * m];
        const double norm = std::sqrt(norm_sq);

        T alpha;
        if constexpr (detail::is_complex_v<T>) {
            const double abs_xk = std::abs(xk);
            const T phase = (abs_xk > 0.0) ? (xk / abs_xk) : T(1.0, 0.0);
            alpha = -phase * norm;
        } else {
            alpha = (xk >= 0.0) ? -norm : norm;
        }

        V[k + k * m] = xk - alpha;
        for (std::size_t i = k + 1; i < m; ++i) V[i + k * m] = R_work[i + k * m];
        double v_norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) v_norm_sq += detail::abs_sq(V[i + k * m]);
        if (v_norm_sq == 0.0) { R_work[k + k * m] = alpha; tau[k] = T(0); continue; }
        tau[k] = T(2.0 / v_norm_sq);
        for (std::size_t j = k + 1; j < n; ++j) {
            T dot = T(0);
            for (std::size_t i = k; i < m; ++i) {
                if constexpr (detail::is_complex_v<T>) {
                    dot += std::conj(V[i + k * m]) * R_work[i + j * m];
                } else {
                    dot += V[i + k * m] * R_work[i + j * m];
                }
            }
            const T s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i) R_work[i + j * m] -= s * V[i + k * m];
        }
        R_work[k + k * m] = alpha;
        for (std::size_t i = k + 1; i < m; ++i) R_work[i + k * m] = T(0);
    }

    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i)
            Rout[i + j * m] = (i <= j) ? R_work[i + j * m] : T(0);

    std::fill(Qout, Qout + m * m, T(0));
    for (std::size_t i = 0; i < m; ++i) Qout[i + i * m] = T(1);
    for (std::size_t kk = n; kk-- > 0;) {
        const std::size_t k = kk;
        if (detail::abs_sq(tau[k]) == 0.0) continue;
        for (std::size_t j = 0; j < m; ++j) {
            T dot = T(0);
            for (std::size_t i = k; i < m; ++i) {
                if constexpr (detail::is_complex_v<T>) {
                    dot += std::conj(V[i + k * m]) * Qout[i + j * m];
                } else {
                    dot += V[i + k * m] * Qout[i + j * m];
                }
            }
            const T s = tau[k] * dot;
            for (std::size_t i = k; i < m; ++i) Qout[i + j * m] -= s * V[i + k * m];
        }
    }
}

} // anonymous namespace

std::tuple<Value, Value>
qr_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("qr: input must be a 2D matrix",
                    0, 0, "qr", "", "numkit:qr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m < n)
        throw Error("qr: number of rows must be >= number of columns "
                    "(wide matrices via row-pivoted QR are deferred)",
                    0, 0, "qr", "", "numkit:qr:wide");
    if (A.isComplex()) {
        auto Q = Value::complexMatrix(m, m, mr);
        auto R = Value::complexMatrix(m, n, mr);
        qrFullHouseholder(A.complexData(), m, n, Q.complexDataMut(), R.complexDataMut(), mr);
        return std::make_tuple(detail::narrow_if_real(Q, mr), detail::narrow_if_real(R, mr));
    }
    auto Q = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto R = Value::matrix(m, n, ValueType::DOUBLE, mr);
    qrFullHouseholder(A.doubleData(), m, n, Q.doubleDataMut(), R.doubleDataMut(), mr);
    return std::make_tuple(std::move(Q), std::move(R));
}

Value qr_R_only(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("qr: input must be a 2D matrix",
                    0, 0, "qr", "", "numkit:qr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m < n)
        throw Error("qr: number of rows must be >= number of columns",
                    0, 0, "qr", "", "numkit:qr:wide");
    ScratchArena scratch(mr);
    if (A.isComplex()) {
        ScratchVec<detail::Complex> Q_unused(m * m, &scratch);
        auto R = Value::complexMatrix(m, n, mr);
        qrFullHouseholder(A.complexData(), m, n, Q_unused.data(), R.complexDataMut(), mr);
        return detail::narrow_if_real(R, mr);
    }
    ScratchVec<double> Q_unused(m * m, &scratch);
    auto R = Value::matrix(m, n, ValueType::DOUBLE, mr);
    qrFullHouseholder(A.doubleData(), m, n, Q_unused.data(), R.doubleDataMut(), mr);
    return R;
}

std::tuple<Value, Value>
qr_pivoted(const Value &A, std::vector<std::size_t> &perm,
           std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("qr: input must be a 2D matrix",
                    0, 0, "qr", "", "numkit:qr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m < n)
        throw Error("qr: number of rows must be >= number of columns "
                    "(wide matrices via row-pivoted QR are deferred)",
                    0, 0, "qr", "", "numkit:qr:wide");
    perm.assign(n, 0);
    if (A.isComplex()) {
        auto Q = Value::complexMatrix(m, m, mr);
        auto R = Value::complexMatrix(m, n, mr);
        qrPivotedHouseholder(A.complexData(), m, n,
                             Q.complexDataMut(), R.complexDataMut(), perm.data(), mr);
        return std::make_tuple(detail::narrow_if_real(Q, mr), detail::narrow_if_real(R, mr));
    }
    auto Q = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto R = Value::matrix(m, n, ValueType::DOUBLE, mr);
    qrPivotedHouseholder(A.doubleData(), m, n,
                         Q.doubleDataMut(), R.doubleDataMut(), perm.data(), mr);
    return std::make_tuple(std::move(Q), std::move(R));
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
                    0, 0, "svd", "", "numkit:svd:notMatrix");
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
                    0, 0, "qrupdate", "", "numkit:qrupdate:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t mq = static_cast<std::size_t>(Q.dims().dim(1));
    const std::size_t mr2 = static_cast<std::size_t>(R.dims().dim(0));
    const std::size_t n  = static_cast<std::size_t>(R.dims().dim(1));
    if (mq != m || mr2 != m)
        throw Error("qrupdate: Q must be m×m and R must be m×n",
                    0, 0, "qrupdate", "", "numkit:qrupdate:badShape");
    if (u.numel() != m || v.numel() != n)
        throw Error("qrupdate: length(u) must equal size(R, 1) and length(v) must equal size(R, 2)",
                    0, 0, "qrupdate", "", "numkit:qrupdate:badVec");

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
                    0, 0, "qrinsert", "", "numkit:qrinsert:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(1));
    if (static_cast<std::size_t>(Q.dims().dim(1)) != m
        || static_cast<std::size_t>(R.dims().dim(0)) != m)
        throw Error("qrinsert: Q must be m×m and R must be m×n",
                    0, 0, "qrinsert", "", "numkit:qrinsert:badShape");
    if (x.numel() != m)
        throw Error("qrinsert: x must have length size(Q, 1)",
                    0, 0, "qrinsert", "", "numkit:qrinsert:badX");
    if (k_1based < 1 || k_1based > n + 1)
        throw Error("qrinsert: k must be in 1..n+1",
                    0, 0, "qrinsert", "", "numkit:qrinsert:badK");
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
                    0, 0, "qrdelete", "", "numkit:qrdelete:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Q.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(1));
    if (static_cast<std::size_t>(Q.dims().dim(1)) != m
        || static_cast<std::size_t>(R.dims().dim(0)) != m)
        throw Error("qrdelete: Q must be m×m and R must be m×n",
                    0, 0, "qrdelete", "", "numkit:qrdelete:badShape");
    if (n == 0)
        throw Error("qrdelete: R has no columns to delete",
                    0, 0, "qrdelete", "", "numkit:qrdelete:empty");
    if (k_1based < 1 || k_1based > n)
        throw Error("qrdelete: k must be in 1..n",
                    0, 0, "qrdelete", "", "numkit:qrdelete:badK");
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
                    0, 0, "cholupdate", "", "numkit:cholupdate:notMatrix");
    const std::size_t n = static_cast<std::size_t>(R.dims().dim(0));
    if (n != static_cast<std::size_t>(R.dims().dim(1)))
        throw Error("cholupdate: R must be square",
                    0, 0, "cholupdate", "", "numkit:cholupdate:notSquare");
    if (x.numel() != n)
        throw Error("cholupdate: x must have length equal to size(R, 1)",
                    0, 0, "cholupdate", "", "numkit:cholupdate:badX");
    if (sign != 1 && sign != -1)
        throw Error("cholupdate: sign must be '+' (1) or '-' (-1)",
                    0, 0, "cholupdate", "", "numkit:cholupdate:badSign");
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
                    0, 0, "cholupdate", "", "numkit:cholupdate:downdateFailed");
    }
}

} // namespace numkit::linalg
