// toolboxes/linalg/src/gsvd.cpp
//
// gsvd — Generalized Singular Value Decomposition.

#include <numkit/linalg/gsvd.hpp>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/linalg/pseudo_subspace.hpp>
#include <numkit/linalg/solvers.hpp>
#include "linalg_detail.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::linalg {

namespace {

using Complex = std::complex<double>;

} // anonymous namespace

Value gsvd_values(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("gsvd: A and B must be 2D matrices", 0, 0, "gsvd", "", "numkit:gsvd:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t p = static_cast<std::size_t>(B.dims().dim(0));
    if (static_cast<std::size_t>(B.dims().dim(1)) != n)
        throw Error("gsvd: A and B must have the same number of columns", 0, 0, "gsvd", "", "numkit:gsvd:badDims");

    if (m == 0 || n == 0 || p == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Step 1: Stack K = [A; B] ((m+p) x n)
    auto K = Value::matrix(m + p, n, (A.isComplex() || B.isComplex()) ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (K.isComplex()) {
        Complex *kd = K.complexDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                kd[i + j * (m + p)] = A.isComplex() ? A.complexData()[i + j * m] : Complex(A.doubleData()[i + j * m], 0.0);
            }
            for (std::size_t i = 0; i < p; ++i) {
                kd[m + i + j * (m + p)] = B.isComplex() ? B.complexData()[i + j * p] : Complex(B.doubleData()[i + j * p], 0.0);
            }
        }
    } else {
        double *kd = K.doubleDataMut();
        const double *ad = A.doubleData();
        const double *bd = B.doubleData();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                kd[i + j * (m + p)] = ad[i + j * m];
            }
            for (std::size_t i = 0; i < p; ++i) {
                kd[m + i + j * (m + p)] = bd[i + j * p];
            }
        }
    }

    // Step 2: QR of K: K = Qk * Rk
    auto [Qk, Rk_full] = qr_decompose(K, mr);

    // Q1 is top m x n of Qk
    auto Q1 = Value::matrix(m, n, Qk.type(), mr);
    if (Qk.isComplex()) {
        const Complex *qd = Qk.complexData();
        Complex *q1d = Q1.complexDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                q1d[i + j * m] = qd[i + j * (m + p)];
            }
        }
    } else {
        const double *qd = Qk.doubleData();
        double *q1d = Q1.doubleDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                q1d[i + j * m] = qd[i + j * (m + p)];
            }
        }
    }

    // Singular values of Q1
    Value C_vals = svd_values(Q1, mr);
    const std::size_t num_vals = C_vals.numel();

    Value sigmas = Value::matrix(num_vals, 1, ValueType::DOUBLE, mr);
    double *sd = sigmas.doubleDataMut();

    for (std::size_t i = 0; i < num_vals; ++i) {
        double c = C_vals.doubleData()[i];
        if (c > 1.0) c = 1.0;
        if (c < 0.0) c = 0.0;
        double s = std::sqrt(std::max(0.0, 1.0 - c * c));
        if (s < 1e-14) {
            sd[i] = std::numeric_limits<double>::infinity();
        } else {
            sd[i] = c / s;
        }
    }

    std::sort(sd, sd + num_vals);
    return sigmas;
}

std::tuple<Value, Value, Value, Value, Value> gsvd(const Value &A, const Value &B,
                                                   std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("gsvd: A and B must be 2D matrices", 0, 0, "gsvd", "", "numkit:gsvd:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t p = static_cast<std::size_t>(B.dims().dim(0));
    if (static_cast<std::size_t>(B.dims().dim(1)) != n)
        throw Error("gsvd: A and B must have the same number of columns", 0, 0, "gsvd", "", "numkit:gsvd:badDims");

    if (m == 0 || n == 0 || p == 0) {
        return {Value::matrix(m, m, ValueType::DOUBLE, mr),
                Value::matrix(p, p, ValueType::DOUBLE, mr),
                Value::matrix(n, n, ValueType::DOUBLE, mr),
                Value::matrix(m, n, ValueType::DOUBLE, mr),
                Value::matrix(p, n, ValueType::DOUBLE, mr)};
    }

    // Step 1: Stack K = [A; B] ((m+p) x n)
    auto K = Value::matrix(m + p, n, (A.isComplex() || B.isComplex()) ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (K.isComplex()) {
        Complex *kd = K.complexDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                kd[i + j * (m + p)] = A.isComplex() ? A.complexData()[i + j * m] : Complex(A.doubleData()[i + j * m], 0.0);
            }
            for (std::size_t i = 0; i < p; ++i) {
                kd[m + i + j * (m + p)] = B.isComplex() ? B.complexData()[i + j * p] : Complex(B.doubleData()[i + j * p], 0.0);
            }
        }
    } else {
        double *kd = K.doubleDataMut();
        const double *ad = A.doubleData();
        const double *bd = B.doubleData();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                kd[i + j * (m + p)] = ad[i + j * m];
            }
            for (std::size_t i = 0; i < p; ++i) {
                kd[m + i + j * (m + p)] = bd[i + j * p];
            }
        }
    }

    // Step 2: QR of K: K = Qk * Rk (Qk is (m+p) x (m+p), Rk is n x n)
    auto [Qk, Rk_full] = qr_decompose(K, mr);

    // Extract top n x n from Rk_full as Rk
    auto Rk = Value::matrix(n, n, Rk_full.type(), mr);
    if (Rk_full.isComplex()) {
        const Complex *rfd = Rk_full.complexData();
        Complex *rd = Rk.complexDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < n; ++i) {
                rd[i + j * n] = rfd[i + j * (m + p)];
            }
        }
    } else {
        const double *rfd = Rk_full.doubleData();
        double *rd = Rk.doubleDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < n; ++i) {
                rd[i + j * n] = rfd[i + j * (m + p)];
            }
        }
    }

    // Split Qk = [Q1; Q2] (first n columns of Qk)
    auto Q1 = Value::matrix(m, n, Qk.type(), mr);
    auto Q2 = Value::matrix(p, n, Qk.type(), mr);
    if (Qk.isComplex()) {
        const Complex *qd = Qk.complexData();
        Complex *q1d = Q1.complexDataMut();
        Complex *q2d = Q2.complexDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                q1d[i + j * m] = qd[i + j * (m + p)];
            }
            for (std::size_t i = 0; i < p; ++i) {
                q2d[i + j * p] = qd[m + i + j * (m + p)];
            }
        }
    } else {
        const double *qd = Qk.doubleData();
        double *q1d = Q1.doubleDataMut();
        double *q2d = Q2.doubleDataMut();
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                q1d[i + j * m] = qd[i + j * (m + p)];
            }
            for (std::size_t i = 0; i < p; ++i) {
                q2d[i + j * p] = qd[m + i + j * (m + p)];
            }
        }
    }

    // Step 3: SVD of Q1: Q1 = U * C * W^H
    auto [U, C, W] = svd_decompose(Q1, mr);

    // Step 4: SVD of Q2 * W = V * S * Z^H
    // First compute Q2_W = Q2 * W
    auto Q2_W = Value::matrix(p, n, (Q2.isComplex() || W.isComplex()) ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (Q2_W.isComplex()) {
        Complex *q2wd = Q2_W.complexDataMut();
        for (std::size_t i = 0; i < p; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                Complex s(0.0, 0.0);
                for (std::size_t k = 0; k < n; ++k) {
                    Complex q2val = Q2.isComplex() ? Q2.complexData()[i + k * p] : Complex(Q2.doubleData()[i + k * p], 0.0);
                    Complex wval  = W.isComplex()  ? W.complexData()[k + j * n]  : Complex(W.doubleData()[k + j * n], 0.0);
                    s += q2val * wval;
                }
                q2wd[i + j * p] = s;
            }
        }
    } else {
        double *q2wd = Q2_W.doubleDataMut();
        const double *q2d = Q2.doubleData();
        const double *wd  = W.doubleData();
        for (std::size_t i = 0; i < p; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < n; ++k) {
                    s += q2d[i + k * p] * wd[k + j * n];
                }
                q2wd[i + j * p] = s;
            }
        }
    }

    auto [V, S, Z] = svd_decompose(Q2_W, mr);

    // Step 5: X = inv(Rk) * W * Z
    // First compute W_Z = W * Z
    auto W_Z = Value::matrix(n, n, (W.isComplex() || Z.isComplex()) ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (W_Z.isComplex()) {
        Complex *wzd = W_Z.complexDataMut();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                Complex s(0.0, 0.0);
                for (std::size_t k = 0; k < n; ++k) {
                    Complex wval = W.isComplex() ? W.complexData()[i + k * n] : Complex(W.doubleData()[i + k * n], 0.0);
                    Complex zval = Z.isComplex() ? Z.complexData()[k + j * n] : Complex(Z.doubleData()[k + j * n], 0.0);
                    s += wval * zval;
                }
                wzd[i + j * n] = s;
            }
        }
    } else {
        double *wzd = W_Z.doubleDataMut();
        const double *wd = W.doubleData();
        const double *zd = Z.doubleData();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < n; ++k) {
                    s += wd[i + k * n] * zd[k + j * n];
                }
                wzd[i + j * n] = s;
            }
        }
    }

    // Step 5: X = Rk^H * (W * Z)
    auto X = Value::matrix(n, n, (Rk.isComplex() || W_Z.isComplex()) ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (X.isComplex()) {
        Complex *xd = X.complexDataMut();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                Complex s(0.0, 0.0);
                for (std::size_t k = 0; k < n; ++k) {
                    Complex rkval = Rk.isComplex()  ? std::conj(Rk.complexData()[k + i * n])  : Complex(Rk.doubleData()[k + i * n], 0.0);
                    Complex wzval  = W_Z.isComplex() ? W_Z.complexData()[k + j * n]           : Complex(W_Z.doubleData()[k + j * n], 0.0);
                    s += rkval * wzval;
                }
                xd[i + j * n] = s;
            }
        }
    } else {
        double *xd = X.doubleDataMut();
        const double *rkd = Rk.doubleData();
        const double *wzd = W_Z.doubleData();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < n; ++k) {
                    s += rkd[k + i * n] * wzd[k + j * n]; // Rk^H is transposed
                }
                xd[i + j * n] = s;
            }
        }
    }

    return {detail::narrow_if_real(U, mr),
            detail::narrow_if_real(V, mr),
            detail::narrow_if_real(X, mr),
            detail::narrow_if_real(C, mr),
            detail::narrow_if_real(S, mr)};
}

} // namespace numkit::linalg
