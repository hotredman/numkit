// ops/src/la_solve.cpp

#include <numkit/ops/la_solve.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

#include <numkit/ops/blas.hpp>
#include <numkit/ops/parallel_for.hpp>

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
bool lu_recursive_inplace(T *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row = 0)
{
    if (m == 0 || n == 0) return true;

    // Base case: panel LU for n <= 128 (L1/L2 cache optimal)
    if (n <= 128) {
        if constexpr (std::is_same_v<T, double>) {
            return ::numkit::ops::lu_panel(A, lda, piv, m, n, offset_row);
        } else {
            for (std::size_t j = 0; j < n; ++j) {
                std::size_t pivot = j;
                double pmax = abs_val(A[j + j * lda]);
                for (std::size_t i = j + 1; i < m; ++i) {
                    const double v = abs_val(A[i + j * lda]);
                    if (v > pmax) {
                        pmax = v;
                        pivot = i;
                    }
                }
                if (pmax == 0.0) return false;
                piv[j] = static_cast<std::int32_t>(pivot + offset_row);
                if (pivot != j) {
                    T *r1 = A + j;
                    T *r2 = A + pivot;
                    for (std::size_t col = 0; col < n; ++col) {
                        std::swap(r1[col * lda], r2[col * lda]);
                    }
                }
                const T inv_pivot = T(1) / A[j + j * lda];
                for (std::size_t i = j + 1; i < m; ++i) {
                    A[i + j * lda] *= inv_pivot;
                }
                for (std::size_t col = j + 1; col < n; ++col) {
                    const T f = A[j + col * lda];
                    if (f == T(0)) continue;
                    for (std::size_t i = j + 1; i < m; ++i) {
                        A[i + col * lda] -= A[i + j * lda] * f;
                    }
                }
            }
            return true;
        }
    }

    // Divide & Conquer
    const std::size_t n1 = n / 2;
    const std::size_t n2 = n - n1;

    // 1. Recursive LU on left panel A1 (m x n1)
    if (!lu_recursive_inplace(A, lda, piv, m, n1, offset_row)) return false;

    // 2. Apply permutations piv[0..n1-1] to right panel A2 (m x n2)
    const std::size_t p2_thresh = (n2 >= 512) ? 128 : (n2 + 1);
    numkit::detail::parallel_for(n2, p2_thresh, [=](std::size_t c_start, std::size_t c_end) {
        for (std::size_t i = 0; i < n1; ++i) {
            std::size_t p = static_cast<std::size_t>(piv[i] - offset_row);
            if (p != i) {
                T *r1 = A + i + (n1 + c_start) * lda;
                T *r2 = A + p + (n1 + c_start) * lda;
                std::size_t bcols = c_end - c_start;
                std::size_t col = 0;
                for (; col + 8 <= bcols; col += 8) {
                    T t0 = r1[(col + 0) * lda]; r1[(col + 0) * lda] = r2[(col + 0) * lda]; r2[(col + 0) * lda] = t0;
                    T t1 = r1[(col + 1) * lda]; r1[(col + 1) * lda] = r2[(col + 1) * lda]; r2[(col + 1) * lda] = t1;
                    T t2 = r1[(col + 2) * lda]; r1[(col + 2) * lda] = r2[(col + 2) * lda]; r2[(col + 2) * lda] = t2;
                    T t3 = r1[(col + 3) * lda]; r1[(col + 3) * lda] = r2[(col + 3) * lda]; r2[(col + 3) * lda] = t3;
                    T t4 = r1[(col + 4) * lda]; r1[(col + 4) * lda] = r2[(col + 4) * lda]; r2[(col + 4) * lda] = t4;
                    T t5 = r1[(col + 5) * lda]; r1[(col + 5) * lda] = r2[(col + 5) * lda]; r2[(col + 5) * lda] = t5;
                    T t6 = r1[(col + 6) * lda]; r1[(col + 6) * lda] = r2[(col + 6) * lda]; r2[(col + 6) * lda] = t6;
                    T t7 = r1[(col + 7) * lda]; r1[(col + 7) * lda] = r2[(col + 7) * lda]; r2[(col + 7) * lda] = t7;
                }
                for (; col < bcols; ++col) {
                    T t = r1[col * lda]; r1[col * lda] = r2[col * lda]; r2[col * lda] = t;
                }
            }
        }
    });

    // 3. Solve L11 * U12 = A12 using multithreaded SIMD trsm (n1 x n2)
    T *L11 = A;
    T *A12 = A + n1 * lda;
    if constexpr (is_complex_v<T>) {
        ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                           n1, n2, Complex(1.0, 0.0),
                           reinterpret_cast<const Complex*>(L11), lda,
                           reinterpret_cast<Complex*>(A12), lda);
    } else {
        ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                           n1, n2, 1.0, L11, lda, A12, lda);
    }

    // 4. Trailing matrix GEMM update: A22 -= L21 * U12 ((m - n1) x n2)
    T *L21 = A + n1;
    T *U12 = A + n1 * lda;
    T *A22 = A + n1 + n1 * lda;
    const std::size_t rem_rows = m - n1;

    if constexpr (is_complex_v<T>) {
        ::numkit::ops::gemm(rem_rows, n2, n1, Complex(-1.0, 0.0),
                           reinterpret_cast<const Complex*>(L21), lda,
                           reinterpret_cast<const Complex*>(U12), lda,
                           Complex(1.0, 0.0),
                           reinterpret_cast<Complex*>(A22), lda);
    } else {
        ::numkit::ops::gemm(rem_rows, n2, n1, -1.0,
                           reinterpret_cast<const double*>(L21), lda,
                           reinterpret_cast<const double*>(U12), lda,
                           1.0,
                           reinterpret_cast<double*>(A22), lda);
    }

    // 5. Recursive LU on right submatrix A22 ((m - n1) x n2)
    if (!lu_recursive_inplace(A22, lda, piv + n1, rem_rows, n2, offset_row + n1)) return false;

    // 6. Apply permutations piv[n1..n-1] to left panel L21 ((m - n1) x n1)
    const std::size_t p6_thresh = (n1 >= 512) ? 128 : (n1 + 1);
    numkit::detail::parallel_for(n1, p6_thresh, [=](std::size_t c_start, std::size_t c_end) {
        for (std::size_t i = 0; i < n2; ++i) {
            std::size_t p = static_cast<std::size_t>(piv[n1 + i] - (offset_row + n1));
            if (p != i) {
                T *r1 = A + n1 + i + c_start * lda;
                T *r2 = A + n1 + p + c_start * lda;
                std::size_t bcols = c_end - c_start;
                std::size_t col = 0;
                for (; col + 8 <= bcols; col += 8) {
                    T t0 = r1[(col + 0) * lda]; r1[(col + 0) * lda] = r2[(col + 0) * lda]; r2[(col + 0) * lda] = t0;
                    T t1 = r1[(col + 1) * lda]; r1[(col + 1) * lda] = r2[(col + 1) * lda]; r2[(col + 1) * lda] = t1;
                    T t2 = r1[(col + 2) * lda]; r1[(col + 2) * lda] = r2[(col + 2) * lda]; r2[(col + 2) * lda] = t2;
                    T t3 = r1[(col + 3) * lda]; r1[(col + 3) * lda] = r2[(col + 3) * lda]; r2[(col + 3) * lda] = t3;
                    T t4 = r1[(col + 4) * lda]; r1[(col + 4) * lda] = r2[(col + 4) * lda]; r2[(col + 4) * lda] = t4;
                    T t5 = r1[(col + 5) * lda]; r1[(col + 5) * lda] = r2[(col + 5) * lda]; r2[(col + 5) * lda] = t5;
                    T t6 = r1[(col + 6) * lda]; r1[(col + 6) * lda] = r2[(col + 6) * lda]; r2[(col + 6) * lda] = t6;
                    T t7 = r1[(col + 7) * lda]; r1[(col + 7) * lda] = r2[(col + 7) * lda]; r2[(col + 7) * lda] = t7;
                }
                for (; col < bcols; ++col) {
                    T t = r1[col * lda]; r1[col * lda] = r2[col * lda]; r2[col * lda] = t;
                }
            }
        }
    });

    return true;
}

template <typename T>
bool lu_pivot_inplace(T *LU, std::int32_t *piv, std::size_t n)
{
    return lu_recursive_inplace(LU, n, piv, n, n, 0);
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
bool la_solve_small_fastpath(const T *A, std::size_t lda,
                             const T *B, std::size_t ldb,
                             T *X, std::size_t ldx, std::size_t n, std::size_t nrhs)
{
    alignas(64) T LU_stack[128 * 128];
    alignas(64) std::int32_t piv_stack[128];

    for (std::size_t col = 0; col < n; ++col) {
        std::memcpy(LU_stack + col * n, A + col * lda, n * sizeof(T));
    }
    for (std::size_t col = 0; col < nrhs; ++col) {
        std::memcpy(X + col * ldx, B + col * ldb, n * sizeof(T));
    }

    if constexpr (std::is_same_v<T, double>) {
        if (!::numkit::ops::lu_panel(LU_stack, n, piv_stack, n, n, 0)) return false;
    } else {
        if (!lu_recursive_inplace(LU_stack, n, piv_stack, n, n, 0)) return false;
    }

    for (std::size_t i = 0; i < n; ++i) {
        std::size_t p = static_cast<std::size_t>(piv_stack[i]);
        if (p != i) {
            T *r1 = X + i;
            T *r2 = X + p;
            for (std::size_t col = 0; col < nrhs; ++col) {
                std::swap(r1[col * ldx], r2[col * ldx]);
            }
        }
    }

    for (std::size_t j = 0; j < nrhs; ++j) {
        T *x_col = X + j * ldx;
        for (std::size_t k = 0; k < n; ++k) {
            const T xkj = x_col[k];
            if (xkj == T(0)) continue;
            if constexpr (std::is_same_v<T, double>) {
                const double *l_col = LU_stack + k * n;
                if (n > k + 1) {
                    axpy(n - (k + 1), -xkj, l_col + (k + 1), x_col + (k + 1));
                }
            } else {
                for (std::size_t i = k + 1; i < n; ++i) {
                    x_col[i] -= LU_stack[i + k * n] * xkj;
                }
            }
        }
    }

    for (std::size_t j = 0; j < nrhs; ++j) {
        T *x_col = X + j * ldx;
        for (std::intptr_t k = static_cast<std::intptr_t>(n) - 1; k >= 0; --k) {
            x_col[k] /= LU_stack[k + k * n];
            const T xkj = x_col[k];
            if (xkj == T(0)) continue;
            if constexpr (std::is_same_v<T, double>) {
                const double *u_col = LU_stack + k * n;
                if (k > 0) {
                    axpy(static_cast<std::size_t>(k), -xkj, u_col, x_col);
                }
            } else {
                for (std::intptr_t i = k - 1; i >= 0; --i) {
                    x_col[i] -= LU_stack[i + k * n] * xkj;
                }
            }
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
        if (n <= 128 && nrhs <= 128) {
            return la_solve_small_fastpath(A, m, B, m, X, m, n, nrhs);
        }
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
