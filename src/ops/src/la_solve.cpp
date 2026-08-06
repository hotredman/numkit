// ops/src/la_solve.cpp

#include <numkit/ops/la_solve.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

#include <numkit/ops/blas.hpp>

namespace numkit::ops {

namespace {

using Complex = std::complex<double>;

template <typename T>
constexpr bool is_complex_v = false;
template <typename T>
constexpr bool is_complex_v<std::complex<T>> = true;

template <typename T>
inline double abs_val(const T &x) {
    if constexpr (is_complex_v<T>) return std::abs(x);
    else return std::fabs(x);
}

template <typename T>
inline double abs_sq(const T &x) {
    if constexpr (is_complex_v<T>) return std::norm(x);
    else return x * x;
}

template <typename T>
inline T conj_val(const T &x) {
    if constexpr (is_complex_v<T>) return std::conj(x);
    else return x;
}

// ── Blocked LU with partial pivoting (square A, n×n) using SIMD GEMM ──
template <typename T>
bool lu_pivot_inplace(T *LU, std::int32_t *piv, std::size_t n)
{
    const std::size_t nb = (n >= 1024) ? 64 : 32; // dynamic panel block size

    for (std::size_t k = 0; k < n; k += nb) {
        std::size_t kb = std::min(nb, n - k);

        // 1. Panel factorization on columns k .. k+kb-1
        for (std::size_t j = k; j < k + kb; ++j) {
            std::size_t pivot = j;
            double pmax = abs_val(LU[j + j * n]);
            for (std::size_t i = j + 1; i < n; ++i) {
                const double v = abs_val(LU[i + j * n]);
                if (v > pmax) {
                    pmax = v;
                    pivot = i;
                }
            }
            if (pmax == 0.0) return false;
            piv[j] = static_cast<std::int32_t>(pivot);
            if (pivot != j) {
                for (std::size_t col = 0; col < n; ++col)
                    std::swap(LU[j + col * n], LU[pivot + col * n]);
            }
            const T inv_pivot = T(1) / LU[j + j * n];
            for (std::size_t i = j + 1; i < n; ++i) {
                LU[i + j * n] *= inv_pivot;
            }
            if constexpr (std::is_same_v<T, double>) {
                const double *l_col = LU + j * n;
                for (std::size_t col = j + 1; col < k + kb; ++col) {
                    const double f = LU[j + col * n];
                    if (f == 0.0) continue;
                    double *col_ptr = LU + col * n;
                    if (n > j + 1) {
                        ::numkit::ops::axpy(n - (j + 1), -f, l_col + (j + 1), col_ptr + (j + 1));
                    }
                }
            } else {
                for (std::size_t col = j + 1; col < k + kb; ++col) {
                    const T f = LU[j + col * n];
                    if (f == T(0)) continue;
                    for (std::size_t i = j + 1; i < n; ++i) {
                        LU[i + col * n] -= LU[i + j * n] * f;
                    }
                }
            }
        }

        // 2. Trailing matrix update via SIMD GEMM & trsm
        if (k + kb < n) {
            const std::size_t rem_cols = n - (k + kb);
            T *L11 = LU + k + k * n;
            T *U12 = LU + k + (k + kb) * n;

            // Solve L11 * U12 = A12 using multithreaded SIMD trsm
            if constexpr (is_complex_v<T>) {
                ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                                   kb, rem_cols, Complex(1.0, 0.0),
                                   reinterpret_cast<const Complex*>(L11), n,
                                   reinterpret_cast<Complex*>(U12), n);
            } else {
                ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                                   kb, rem_cols, 1.0, L11, n, U12, n);
            }

            const std::size_t rem_rows = n - (k + kb);
            const T *L21 = LU + (k + kb) + k * n;
            T *A22 = LU + (k + kb) + (k + kb) * n;

            if constexpr (is_complex_v<T>) {
                ::numkit::ops::gemm(rem_rows, rem_cols, kb, Complex(-1.0, 0.0),
                                   reinterpret_cast<const Complex*>(L21), n,
                                   reinterpret_cast<const Complex*>(U12), n,
                                   Complex(1.0, 0.0),
                                   reinterpret_cast<Complex*>(A22), n);
            } else {
                ::numkit::ops::gemm(rem_rows, rem_cols, kb, -1.0,
                                   reinterpret_cast<const double*>(L21), n,
                                   reinterpret_cast<const double*>(U12), n,
                                   1.0,
                                   reinterpret_cast<double*>(A22), n);
            }
        }
    }
    return true;
}

// ── QR via Householder (tall A, m×n with m >= n) ─────────────────────
template <typename T>
bool qr_solve_house(T *A, std::size_t m, std::size_t n, T *B, std::size_t nrhs, T *X, std::pmr::memory_resource *mr)
{
    ScratchVec<T> v(m, mr);

    for (std::size_t k = 0; k < n; ++k) {
        double norm_sq = 0.0;
        for (std::size_t i = k; i < m; ++i) {
            norm_sq += abs_sq(A[i + k * m]);
        }
        if (norm_sq == 0.0) return false;

        const T xk = A[k + k * m];
        const double norm = std::sqrt(norm_sq);

        T alpha;
        if constexpr (is_complex_v<T>) {
            double ax = std::abs(xk);
            T phase = (ax > 0.0) ? (xk / ax) : T(1.0, 0.0);
            alpha = -phase * norm;
        } else {
            alpha = (xk >= 0.0) ? -norm : norm;
        }

        v[k] = xk - alpha;
        for (std::size_t i = k + 1; i < m; ++i) v[i] = A[i + k * m];

        double v_norm_sq = abs_sq(v[k]);
        for (std::size_t i = k + 1; i < m; ++i) v_norm_sq += abs_sq(v[i]);
        if (v_norm_sq == 0.0) {
            A[k + k * m] = alpha;
            continue;
        }
        const double tau = 2.0 / v_norm_sq;

        for (std::size_t j = k + 1; j < n; ++j) {
            T dot(0);
            for (std::size_t i = k; i < m; ++i) dot += conj_val(v[i]) * A[i + j * m];
            const T s = T(tau) * dot;
            for (std::size_t i = k; i < m; ++i) A[i + j * m] -= s * v[i];
        }
        for (std::size_t j = 0; j < nrhs; ++j) {
            T dot(0);
            for (std::size_t i = k; i < m; ++i) dot += conj_val(v[i]) * B[i + j * m];
            const T s = T(tau) * dot;
            for (std::size_t i = k; i < m; ++i) B[i + j * m] -= s * v[i];
        }

        A[k + k * m] = alpha;
    }

    for (std::size_t j = 0; j < nrhs; ++j) {
        for (std::size_t i = n; i-- > 0;) {
            T s = B[i + j * m];
            for (std::size_t k = i + 1; k < n; ++k)
                s -= A[i + k * m] * X[k + j * n];
            const T r_ii = A[i + i * m];
            if (abs_val(r_ii) == 0.0) return false;
            X[i + j * n] = s / r_ii;
        }
    }
    return true;
}

template <typename T>
bool la_solve_impl(const T *A, std::size_t m, std::size_t n, const T *B, std::size_t nrhs, T *X, std::pmr::memory_resource *mr)
{
    if (m < n || m == 0 || n == 0 || nrhs == 0) return false;

    ScratchArena arena(mr);

    if (m == n) {
        ScratchVec<T> A_lu(n * n, &arena);
        std::copy(A, A + n * n, A_lu.begin());
        ScratchVec<std::int32_t> piv(n, &arena);
        if (!lu_pivot_inplace(A_lu.data(), piv.data(), n)) return false;

        std::vector<std::size_t> perm(n);
        for (std::size_t i = 0; i < n; ++i) perm[i] = i;
        for (std::size_t k = 0; k < n; ++k)
            std::swap(perm[k], perm[piv[k]]);

        for (std::size_t col = 0; col < nrhs; ++col) {
            for (std::size_t row = 0; row < n; ++row) {
                X[row + col * n] = B[perm[row] + col * n];
            }
        }

        if constexpr (is_complex_v<T>) {
            ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                      n, nrhs, Complex(1.0, 0.0), reinterpret_cast<const Complex*>(A_lu.data()), n,
                      reinterpret_cast<Complex*>(X), n);
            ops::trsm(MatrixSide::Left, MatrixUplo::Upper, MatrixTranspose::NoTrans, MatrixDiag::NonUnit,
                      n, nrhs, Complex(1.0, 0.0), reinterpret_cast<const Complex*>(A_lu.data()), n,
                      reinterpret_cast<Complex*>(X), n);
        } else {
            ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                      n, nrhs, 1.0, reinterpret_cast<const double*>(A_lu.data()), n,
                      reinterpret_cast<double*>(X), n);
            ops::trsm(MatrixSide::Left, MatrixUplo::Upper, MatrixTranspose::NoTrans, MatrixDiag::NonUnit,
                      n, nrhs, 1.0, reinterpret_cast<const double*>(A_lu.data()), n,
                      reinterpret_cast<double*>(X), n);
        }
        return true;
    }

    ScratchVec<T> A_qr(m * n, &arena);
    ScratchVec<T> B_qr(m * nrhs, &arena);
    std::copy(A, A + m * n, A_qr.begin());
    std::copy(B, B + m * nrhs, B_qr.begin());
    return qr_solve_house(A_qr.data(), m, n, B_qr.data(), nrhs, X, &arena);
}

} // anonymous namespace

bool la_solve(const double *A, std::size_t m, std::size_t n, const double *B, std::size_t nrhs, double *X, std::pmr::memory_resource *mr)
{
    return la_solve_impl(A, m, n, B, nrhs, X, mr);
}

bool la_solve(const std::complex<double> *A, std::size_t m, std::size_t n, const std::complex<double> *B, std::size_t nrhs, std::complex<double> *X, std::pmr::memory_resource *mr)
{
    return la_solve_impl(A, m, n, B, nrhs, X, mr);
}

} // namespace numkit::ops
