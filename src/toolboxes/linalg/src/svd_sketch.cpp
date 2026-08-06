// toolboxes/linalg/src/svd_sketch.cpp
//
// svdsketch & svdappend — Randomized low-rank SVD and Incremental SVD.

#include <numkit/linalg/svd_sketch.hpp>
#include <numkit/linalg/decompositions.hpp>
#include "linalg_detail.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

namespace numkit::linalg {

namespace {

using Complex = std::complex<double>;

// Multiply two real matrices C = A * B
Value matMulReal(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t k = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t n = static_cast<std::size_t>(B.dims().dim(1));

    auto C = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *cd = C.doubleDataMut();
    std::fill(cd, cd + m * n, 0.0);

    const double *ad = A.doubleData();
    const double *bd = B.doubleData();

    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t l = 0; l < k; ++l) {
            const double b_lj = bd[l + j * k];
            if (b_lj == 0.0) continue;
            for (std::size_t i = 0; i < m; ++i) {
                cd[i + j * m] += ad[i + l * m] * b_lj;
            }
        }
    }
    return C;
}

// Transpose of a real matrix
Value matTranspose(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    auto At = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *atd = At.doubleDataMut();
    const double *ad = A.doubleData();
    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            atd[j + i * n] = ad[i + j * m];
        }
    }
    return At;
}

} // anonymous namespace

std::tuple<Value, Value, Value> svdsketch(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("svdsketch: input A must be a 2D matrix", 0, 0, "svdsketch", "", "numkit:svdsketch:notMatrix");

    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t min_mn = std::min(m, n);

    if (min_mn == 0) {
        return {Value::matrix(m, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(n, 0, ValueType::DOUBLE, mr)};
    }

    // Step 1: Draw Gaussian random test matrix Omega (n x l)
    std::size_t l = std::min(min_mn, static_cast<std::size_t>(15));
    auto Omega = Value::matrix(n, l, ValueType::DOUBLE, mr);
    double *od = Omega.doubleDataMut();

    std::mt19937 gen(42); // fixed seed for deterministic reproducibility
    std::normal_distribution<double> dist(0.0, 1.0);
    for (std::size_t i = 0; i < n * l; ++i) od[i] = dist(gen);

    // Step 2: Y = A * Omega (m x l)
    Value Y = matMulReal(A, Omega, mr);

    // Step 3: QR of Y: Q * R = Y => Q (m x l)
    auto [Q_full, R_y] = qr_decompose(Y, mr);
    auto Q = Value::matrix(m, l, ValueType::DOUBLE, mr);
    const double *qfd = Q_full.doubleData();
    double *qd = Q.doubleDataMut();
    for (std::size_t j = 0; j < l; ++j) {
        for (std::size_t i = 0; i < m; ++i) {
            qd[i + j * m] = qfd[i + j * m];
        }
    }

    // Step 4: B = Q^T * A (l x n)
    Value Qt = matTranspose(Q, mr);
    Value B = matMulReal(Qt, A, mr);

    // Step 5: SVD of B: B = U_b * S * V^T
    auto [Ub, S, V] = svd_decompose(B, mr);

    // Step 6: U = Q * Ub
    Value U = matMulReal(Q, Ub, mr);

    // Step 7: Truncate rank by tolerance
    const double s0 = S.doubleData()[0];
    const double cutoff = tol * s0;
    std::size_t rank = 0;
    for (std::size_t i = 0; i < std::min(l, min_mn); ++i) {
        if (S.doubleData()[i + i * l] >= cutoff) {
            rank++;
        }
    }
    if (rank == 0) rank = 1;

    // Slice top rank columns/rows
    auto Ur = Value::matrix(m, rank, ValueType::DOUBLE, mr);
    auto Sr = Value::matrix(rank, rank, ValueType::DOUBLE, mr);
    auto Vr = Value::matrix(n, rank, ValueType::DOUBLE, mr);

    double *urd = Ur.doubleDataMut();
    double *srd = Sr.doubleDataMut();
    double *vrd = Vr.doubleDataMut();

    std::fill(srd, srd + rank * rank, 0.0);

    const double *ud = U.doubleData();
    const double *sd = S.doubleData();
    const double *vd = V.doubleData();

    for (std::size_t j = 0; j < rank; ++j) {
        for (std::size_t i = 0; i < m; ++i) urd[i + j * m] = ud[i + j * m];
        for (std::size_t i = 0; i < n; ++i) vrd[i + j * n] = vd[i + j * n];
        srd[j + j * rank] = sd[j + j * l];
    }

    return {detail::narrow_if_real(Ur, mr),
            detail::narrow_if_real(Sr, mr),
            detail::narrow_if_real(Vr, mr)};
}

std::tuple<Value, Value, Value> svdappend(const Value &U, const Value &S, const Value &V,
                                          const Value &A_new, std::pmr::memory_resource *mr)
{
    if (U.dims().ndim() != 2 || S.dims().ndim() != 2 || V.dims().ndim() != 2 || A_new.dims().ndim() != 2)
        throw Error("svdappend: inputs must be 2D matrices", 0, 0, "svdappend", "", "numkit:svdappend:notMatrix");

    const std::size_t m = static_cast<std::size_t>(U.dims().dim(0));
    const std::size_t k = static_cast<std::size_t>(U.dims().dim(1));
    const std::size_t n = static_cast<std::size_t>(V.dims().dim(0));
    const std::size_t p = static_cast<std::size_t>(A_new.dims().dim(1));

    if (static_cast<std::size_t>(A_new.dims().dim(0)) != m)
        throw Error("svdappend: A_new must have the same number of rows as U", 0, 0, "svdappend", "", "numkit:svdappend:badDims");

    // Brand algorithm:
    // m_k = U^T * A_new  (k x p)
    Value Ut = matTranspose(U, mr);
    Value m_k = matMulReal(Ut, A_new, mr);

    // P = A_new - U * m_k  (m x p)
    Value Umk = matMulReal(U, m_k, mr);
    Value P = Value::matrix(m, p, ValueType::DOUBLE, mr);
    double *pd = P.doubleDataMut();
    const double *anewd = A_new.doubleData();
    const double *umkd = Umk.doubleData();
    for (std::size_t i = 0; i < m * p; ++i) pd[i] = anewd[i] - umkd[i];

    // Economic QR of P: Qp * Rp = P (Qp is m x p, Rp is p x p)
    auto [Qp_full, Rp_full] = qr_decompose(P, mr);
    auto Qp = Value::matrix(m, p, ValueType::DOUBLE, mr);
    auto Rp = Value::matrix(p, p, ValueType::DOUBLE, mr);

    const double *qpfd = Qp_full.doubleData();
    const double *rpfd = Rp_full.doubleData();
    double *qpd = Qp.doubleDataMut();
    double *rpd = Rp.doubleDataMut();

    for (std::size_t j = 0; j < p; ++j) {
        for (std::size_t i = 0; i < m; ++i) qpd[i + j * m] = qpfd[i + j * m];
        for (std::size_t i = 0; i < p; ++i) rpd[i + j * p] = rpfd[i + j * (m + p)];
    }

    // Build block matrix K ((k+p) x (k+p)):
    // [ S    m_k ]
    // [ 0    R_p ]
    const std::size_t dim = k + p;
    Value K = Value::matrix(dim, dim, ValueType::DOUBLE, mr);
    double *kd = K.doubleDataMut();
    std::fill(kd, kd + dim * dim, 0.0);

    const double *sd = S.doubleData();
    const double *mkd = m_k.doubleData();

    // S block (k x k)
    for (std::size_t j = 0; j < k; ++j) {
        for (std::size_t i = 0; i < k; ++i) {
            kd[i + j * dim] = sd[i + j * k];
        }
    }
    // m_k block (k x p)
    for (std::size_t j = 0; j < p; ++j) {
        for (std::size_t i = 0; i < k; ++i) {
            kd[i + (k + j) * dim] = mkd[i + j * k];
        }
    }
    // Rp block (p x p)
    for (std::size_t j = 0; j < p; ++j) {
        for (std::size_t i = 0; i < p; ++i) {
            kd[(k + i) + (k + j) * dim] = rpd[i + j * p];
        }
    }

    // SVD of K: K = Uk * S_new * Vk^T
    auto [Uk, S_new, Vk] = svd_decompose(K, mr);

    // Form U_large = [U Qp] (m x (k+p))
    Value U_large = Value::matrix(m, dim, ValueType::DOUBLE, mr);
    double *uld = U_large.doubleDataMut();
    const double *ud = U.doubleData();
    for (std::size_t j = 0; j < k; ++j) {
        for (std::size_t i = 0; i < m; ++i) uld[i + j * m] = ud[i + j * m];
    }
    for (std::size_t j = 0; j < p; ++j) {
        for (std::size_t i = 0; i < m; ++i) uld[i + (k + j) * m] = qpd[i + j * m];
    }

    // Form V_large = [V 0; 0 I_p] ((n+p) x (k+p))
    const std::size_t n_new = n + p;
    Value V_large = Value::matrix(n_new, dim, ValueType::DOUBLE, mr);
    double *vld = V_large.doubleDataMut();
    std::fill(vld, vld + n_new * dim, 0.0);

    const double *vd = V.doubleData();
    for (std::size_t j = 0; j < k; ++j) {
        for (std::size_t i = 0; i < n; ++i) vld[i + j * n_new] = vd[i + j * n];
    }
    for (std::size_t j = 0; j < p; ++j) {
        vld[(n + j) + (k + j) * n_new] = 1.0;
    }

    // Final U_new = U_large * Uk
    Value U_new = matMulReal(U_large, Uk, mr);

    // Final V_new = V_large * Vk
    Value V_new = matMulReal(V_large, Vk, mr);

    return {detail::narrow_if_real(U_new, mr),
            detail::narrow_if_real(S_new, mr),
            detail::narrow_if_real(V_new, mr)};
}

} // namespace numkit::linalg
