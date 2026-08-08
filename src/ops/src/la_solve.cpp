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

    // Base case: panel LU for n <= base_n (adaptive 32 for m >= 256 to maximize 24-thread GEMM offload)
    const std::size_t base_n = (m >= 256) ? 32 : 128;
    if (n <= base_n) {
        return ::numkit::ops::lu_panel(A, lda, piv, m, n, offset_row);
    }

    // Divide & Conquer
    const std::size_t n1 = n / 2;
    const std::size_t n2 = n - n1;

    // 1. Recursive LU on left panel A1 (m x n1)
    if (!lu_recursive_inplace(A, lda, piv, m, n1, offset_row)) return false;

    // 2. Apply permutations piv[0..n1-1] to right panel A2 (m x n2)
    const std::size_t p2_thresh = (n2 >= 64) ? 32 : (n2 + 1);
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
        ::numkit::ops::gemm(MatrixTranspose::NoTrans, MatrixTranspose::NoTrans, rem_rows, n2, n1, Complex(-1.0, 0.0),
                           reinterpret_cast<const Complex*>(L21), lda,
                           reinterpret_cast<const Complex*>(U12), lda,
                           Complex(1.0, 0.0),
                           reinterpret_cast<Complex*>(A22), lda);
    } else {
        ::numkit::ops::gemm(MatrixTranspose::NoTrans, MatrixTranspose::NoTrans, rem_rows, n2, n1, -1.0,
                           reinterpret_cast<const double*>(L21), lda,
                           reinterpret_cast<const double*>(U12), lda,
                           1.0,
                           reinterpret_cast<double*>(A22), lda);
    }

    // 5. Recursive LU on right submatrix A22 ((m - n1) x n2)
    if (!lu_recursive_inplace(A22, lda, piv + n1, rem_rows, n2, offset_row + n1)) return false;

    // 6. Apply permutations piv[n1..n-1] to left panel L21 ((m - n1) x n1)
    const std::size_t p6_thresh = (n1 >= 64) ? 32 : (n1 + 1);
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
bool lu_blocked_inplace(T *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row = 0)
{
    if (m == 0 || n == 0) return true;

    if (m <= 128 || n <= 128) {
        return ::numkit::ops::lu_panel(A, lda, piv, m, n, offset_row);
    }

    constexpr std::size_t nb = 64;
    const std::size_t min_mn = std::min(m, n);

    for (std::size_t j = 0; j < min_mn; j += nb) {
        const std::size_t jb = std::min(nb, min_mn - j);
        const std::size_t rem_m = m - j;
        T *A_jj = A + j + j * lda;

        if (!lu_recursive_inplace(A_jj, lda, piv + j, rem_m, jb, offset_row + j)) return false;

        if (j + jb < n) {
            const std::size_t rem_n = n - (j + jb);
            T *A_right = A + j + (j + jb) * lda;

            const std::size_t p_thresh = (rem_n >= 64) ? 32 : (rem_n + 1);
            numkit::detail::parallel_for(rem_n, p_thresh, [=](std::size_t c_start, std::size_t c_end) {
                for (std::size_t i = 0; i < jb; ++i) {
                    std::size_t p = static_cast<std::size_t>(piv[j + i] - (offset_row + j));
                    if (p != i) {
                        T *r1 = A_right + i + c_start * lda;
                        T *r2 = A_right + p + c_start * lda;
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

            T *L11 = A_jj;
            T *U12 = A_right;
            if constexpr (is_complex_v<T>) {
                ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                                   jb, rem_n, Complex(1.0, 0.0),
                                   reinterpret_cast<const Complex*>(L11), lda,
                                   reinterpret_cast<Complex*>(U12), lda);
            } else {
                ::numkit::ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit,
                                   jb, rem_n, 1.0, L11, lda, U12, lda);
            }

            if (j + jb < m) {
                const std::size_t trailing_m = rem_m - jb;
                T *L21 = A + (j + jb) + j * lda;
                T *A22 = A + (j + jb) + (j + jb) * lda;

                if constexpr (is_complex_v<T>) {
                    ::numkit::ops::gemm(MatrixTranspose::NoTrans, MatrixTranspose::NoTrans, trailing_m, rem_n, jb, Complex(-1.0, 0.0),
                                       reinterpret_cast<const Complex*>(L21), lda,
                                       reinterpret_cast<const Complex*>(U12), lda,
                                       Complex(1.0, 0.0),
                                       reinterpret_cast<Complex*>(A22), lda);
                } else {
                    ::numkit::ops::gemm(MatrixTranspose::NoTrans, MatrixTranspose::NoTrans, trailing_m, rem_n, jb, -1.0,
                                       reinterpret_cast<const double*>(L21), lda,
                                       reinterpret_cast<const double*>(U12), lda,
                                       1.0,
                                       reinterpret_cast<double*>(A22), lda);
                }
            }
        }
    }

    return true;
}

template <typename T>
bool lu_pivot_inplace(T *LU, std::int32_t *piv, std::size_t n)
{
    return lu_blocked_inplace(LU, n, piv, n, n, 0);
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

    std::size_t j = 0;
    if constexpr (std::is_same_v<T, double>) {
        // Unroll RHS by 4 to minimize L1 bandwidth and maximize ILP
        for (; j + 3 < nrhs; j += 4) {
            double *x_col0 = reinterpret_cast<double*>(X + (j + 0) * ldx);
            double *x_col1 = reinterpret_cast<double*>(X + (j + 1) * ldx);
            double *x_col2 = reinterpret_cast<double*>(X + (j + 2) * ldx);
            double *x_col3 = reinterpret_cast<double*>(X + (j + 3) * ldx);
            
            // Forward substitution
            for (std::size_t k = 0; k < n; ++k) {
                const double x0 = x_col0[k];
                const double x1 = x_col1[k];
                const double x2 = x_col2[k];
                const double x3 = x_col3[k];
                if (x0 == 0.0 && x1 == 0.0 && x2 == 0.0 && x3 == 0.0) continue;
                
                const double *l_col = reinterpret_cast<const double*>(LU_stack) + k * n;
                double *y0 = x_col0; double *y1 = x_col1; double *y2 = x_col2; double *y3 = x_col3;
                const double n0 = -x0; const double n1 = -x1; const double n2 = -x2; const double n3 = -x3;
                
                std::size_t i = k + 1;
                for (; i + 3 < n; i += 4) {
                    const double u0 = l_col[i + 0];
                    const double u1 = l_col[i + 1];
                    const double u2 = l_col[i + 2];
                    const double u3 = l_col[i + 3];

                    y0[i+0] += u0 * n0; y0[i+1] += u1 * n0; y0[i+2] += u2 * n0; y0[i+3] += u3 * n0;
                    y1[i+0] += u0 * n1; y1[i+1] += u1 * n1; y1[i+2] += u2 * n1; y1[i+3] += u3 * n1;
                    y2[i+0] += u0 * n2; y2[i+1] += u1 * n2; y2[i+2] += u2 * n2; y2[i+3] += u3 * n2;
                    y3[i+0] += u0 * n3; y3[i+1] += u1 * n3; y3[i+2] += u2 * n3; y3[i+3] += u3 * n3;
                }
                for (; i < n; ++i) {
                    const double u = l_col[i];
                    y0[i] += u * n0; y1[i] += u * n1; y2[i] += u * n2; y3[i] += u * n3;
                }
            }
            
            // Backward substitution
            for (std::intptr_t k_idx = static_cast<std::intptr_t>(n) - 1; k_idx >= 0; --k_idx) {
                const std::size_t k = static_cast<std::size_t>(k_idx);
                const double diag = reinterpret_cast<const double*>(LU_stack)[k + k * n];
                x_col0[k] /= diag; x_col1[k] /= diag; x_col2[k] /= diag; x_col3[k] /= diag;
                const double x0 = x_col0[k];
                const double x1 = x_col1[k];
                const double x2 = x_col2[k];
                const double x3 = x_col3[k];
                if (x0 == 0.0 && x1 == 0.0 && x2 == 0.0 && x3 == 0.0) continue;
                
                const double *u_col = reinterpret_cast<const double*>(LU_stack) + k * n;
                double *y0 = x_col0; double *y1 = x_col1; double *y2 = x_col2; double *y3 = x_col3;
                const double n0 = -x0; const double n1 = -x1; const double n2 = -x2; const double n3 = -x3;
                
                std::size_t i = 0;
                for (; i + 3 < k; i += 4) {
                    const double u0 = u_col[i + 0];
                    const double u1 = u_col[i + 1];
                    const double u2 = u_col[i + 2];
                    const double u3 = u_col[i + 3];

                    y0[i+0] += u0 * n0; y0[i+1] += u1 * n0; y0[i+2] += u2 * n0; y0[i+3] += u3 * n0;
                    y1[i+0] += u0 * n1; y1[i+1] += u1 * n1; y1[i+2] += u2 * n1; y1[i+3] += u3 * n1;
                    y2[i+0] += u0 * n2; y2[i+1] += u1 * n2; y2[i+2] += u2 * n2; y2[i+3] += u3 * n2;
                    y3[i+0] += u0 * n3; y3[i+1] += u1 * n3; y3[i+2] += u2 * n3; y3[i+3] += u3 * n3;
                }
                for (; i < k; ++i) {
                    const double u = u_col[i];
                    y0[i] += u * n0; y1[i] += u * n1; y2[i] += u * n2; y3[i] += u * n3;
                }
            }
        }
    }
    
    // Cleanup remaining columns
    for (; j < nrhs; ++j) {
        T *x_col = X + j * ldx;
        for (std::size_t k = 0; k < n; ++k) {
            const T xkj = x_col[k];
            if (xkj == T(0)) continue;
            if constexpr (std::is_same_v<T, double>) {
                const double *l_col = LU_stack + k * n;
                double *y_ptr = x_col;
                const std::size_t rem = n - (k + 1);
                const double neg_x = -xkj;
                numkit::ops::axpy(rem, neg_x, l_col + (k + 1), y_ptr + (k + 1));
            } else {
                for (std::size_t i = k + 1; i < n; ++i) {
                    x_col[i] -= LU_stack[i + k * n] * xkj;
                }
            }
        }
    }

    for (j = nrhs - (nrhs % 4 == 0 ? 0 : nrhs % 4); j < nrhs; ++j) {
        T *x_col = X + j * ldx;
        for (std::intptr_t k = static_cast<std::intptr_t>(n) - 1; k >= 0; --k) {
            x_col[k] /= LU_stack[k + k * n];
            const T xkj = x_col[k];
            if (xkj == T(0)) continue;
            if constexpr (std::is_same_v<T, double>) {
                const double *u_col = LU_stack + k * n;
                double *y_ptr = x_col;
                const std::size_t rem = static_cast<std::size_t>(k);
                const double neg_x = -xkj;
                numkit::ops::axpy(rem, neg_x, u_col, y_ptr);
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
void trsm_L_recursive(std::size_t m, std::size_t n, const T* A, std::size_t lda, T* B, std::size_t ldb) {
    if (m <= 16) {
        if constexpr (is_complex_v<T>) {
            ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit, m, n, Complex(1,0), reinterpret_cast<const Complex*>(A), lda, reinterpret_cast<Complex*>(B), ldb);
        } else {
            ops::trsm(MatrixSide::Left, MatrixUplo::Lower, MatrixTranspose::NoTrans, MatrixDiag::Unit, m, n, 1.0, A, lda, B, ldb);
        }
        return;
    }
    std::size_t m1 = m / 2;
    std::size_t m2 = m - m1;
    trsm_L_recursive(m1, n, A, lda, B, ldb);
    if constexpr (is_complex_v<T>) {
        ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, m2, n, m1, Complex(-1,0), reinterpret_cast<const Complex*>(A + m1), lda, reinterpret_cast<const Complex*>(B), ldb, Complex(1,0), reinterpret_cast<Complex*>(B + m1), ldb);
    } else {
        ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, m2, n, m1, -1.0, A + m1, lda, B, ldb, 1.0, B + m1, ldb);
    }
    trsm_L_recursive(m2, n, A + m1 + m1 * lda, lda, B + m1, ldb);
}

template <typename T>
void trsm_U_recursive(std::size_t m, std::size_t n, const T* A, std::size_t lda, T* B, std::size_t ldb) {
    if (m <= 16) {
        if constexpr (is_complex_v<T>) {
            ops::trsm(MatrixSide::Left, MatrixUplo::Upper, MatrixTranspose::NoTrans, MatrixDiag::NonUnit, m, n, Complex(1,0), reinterpret_cast<const Complex*>(A), lda, reinterpret_cast<Complex*>(B), ldb);
        } else {
            ops::trsm(MatrixSide::Left, MatrixUplo::Upper, MatrixTranspose::NoTrans, MatrixDiag::NonUnit, m, n, 1.0, A, lda, B, ldb);
        }
        return;
    }
    std::size_t m1 = m / 2;
    std::size_t m2 = m - m1;
    trsm_U_recursive(m2, n, A + m1 + m1 * lda, lda, B + m1, ldb);
    if constexpr (is_complex_v<T>) {
        ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, m1, n, m2, Complex(-1,0), reinterpret_cast<const Complex*>(A + m1 * lda), lda, reinterpret_cast<const Complex*>(B + m1), ldb, Complex(1,0), reinterpret_cast<Complex*>(B), ldb);
    } else {
        ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, m1, n, m2, -1.0, A + m1 * lda, lda, B + m1, ldb, 1.0, B, ldb);
    }
    trsm_U_recursive(m1, n, A, lda, B, ldb);
}

template <typename T>
void trsm_L_blocked_seq(std::size_t n, std::size_t nrhs, const T *A, std::size_t lda, T *B, std::size_t ldb) {
    constexpr std::size_t nb = 64;
    for (std::size_t j = 0; j < n; j += nb) {
        const std::size_t jb = std::min(nb, n - j);
        // Solve L_jj * X_j = B_j
        for (std::size_t k = 0; k < jb; ++k) {
            const T* l_col = A + (j + k) * lda;
            for (std::size_t c = 0; c < nrhs; ++c) {
                const T xkc = B[j + k + c * ldb];
                if (xkc != T(0)) {
                    if constexpr (std::is_same_v<T, double>) {
                        if (jb > k + 1) {
                            numkit::ops::axpy(jb - (k + 1), -xkc, reinterpret_cast<const double*>(l_col + j + k + 1), reinterpret_cast<double*>(B + j + k + 1 + c * ldb));
                        }
                    } else {
                        for (std::size_t i = k + 1; i < jb; ++i) {
                            B[j + i + c * ldb] -= l_col[j + i] * xkc;
                        }
                    }
                }
            }
        }
        // Update trailing matrix: B_trailing -= L_trailing * X_j
        if (j + jb < n) {
            const std::size_t rem_m = n - (j + jb);
            const T* L21 = A + j * lda + (j + jb);
            T* B2 = B + (j + jb);
            
            if constexpr (std::is_same_v<T, double>) {
                numkit::ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, rem_m, nrhs, jb, -1.0, 
                                  reinterpret_cast<const double*>(L21), lda, 
                                  reinterpret_cast<const double*>(B + j), ldb, 
                                  1.0, 
                                  reinterpret_cast<double*>(B2), ldb);
            } else {
                numkit::ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, rem_m, nrhs, jb, Complex(-1.0, 0.0), 
                                  reinterpret_cast<const Complex*>(L21), lda, 
                                  reinterpret_cast<const Complex*>(B + j), ldb, 
                                  Complex(1.0, 0.0), 
                                  reinterpret_cast<Complex*>(B2), ldb);
            }
        }
    }
}

template <typename T>
void trsm_U_blocked_seq(std::size_t n, std::size_t nrhs, const T *A, std::size_t lda, T *B, std::size_t ldb) {
    constexpr std::size_t nb = 64;
    for (std::intptr_t j_idx = static_cast<std::intptr_t>(n) - 1; j_idx >= 0; j_idx -= nb) {
        const std::size_t j = static_cast<std::size_t>(std::max(std::intptr_t{0}, j_idx - static_cast<std::intptr_t>(nb) + 1));
        const std::size_t jb = static_cast<std::size_t>(j_idx) - j + 1;
        
        // Solve U_jj * X_j = B_j
        for (std::intptr_t k_idx = static_cast<std::intptr_t>(jb) - 1; k_idx >= 0; --k_idx) {
            const std::size_t k = static_cast<std::size_t>(k_idx);
            const T* u_col = A + (j + k) * lda;
            const T akk = u_col[j + k];
            for (std::size_t c = 0; c < nrhs; ++c) {
                B[j + k + c * ldb] /= akk;
                const T xkc = B[j + k + c * ldb];
                if (xkc != T(0)) {
                    if constexpr (std::is_same_v<T, double>) {
                        if (k > 0) {
                            numkit::ops::axpy(k, -xkc, reinterpret_cast<const double*>(u_col + j), reinterpret_cast<double*>(B + j + c * ldb));
                        }
                    } else {
                        for (std::size_t i = 0; i < k; ++i) {
                            B[j + i + c * ldb] -= u_col[j + i] * xkc;
                        }
                    }
                }
            }
        }
        // Update leading matrix: B_leading -= U_leading * X_j
        if (j > 0) {
            const std::size_t rem_m = j;
            const T* U12 = A + j * lda;
            T* B1 = B;
            
            if constexpr (std::is_same_v<T, double>) {
                numkit::ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, rem_m, nrhs, jb, -1.0, 
                                  reinterpret_cast<const double*>(U12), lda, 
                                  reinterpret_cast<const double*>(B + j), ldb, 
                                  1.0, 
                                  reinterpret_cast<double*>(B1), ldb);
            } else {
                numkit::ops::gemm(ops::MatrixTranspose::NoTrans, ops::MatrixTranspose::NoTrans, rem_m, nrhs, jb, Complex(-1.0, 0.0), 
                                  reinterpret_cast<const Complex*>(U12), lda, 
                                  reinterpret_cast<const Complex*>(B + j), ldb, 
                                  Complex(1.0, 0.0), 
                                  reinterpret_cast<Complex*>(B1), ldb);
            }
        }
    }
}

template <typename T>
bool la_solve_impl(const T *A, std::size_t m, std::size_t n, const T *B, std::size_t nrhs, T *X, std::pmr::memory_resource *mr)
{
    if (m < n || m == 0 || n == 0 || nrhs == 0) return false;

    ScratchArena arena(mr);

    if (m == n) {
        if (n <= 96 && nrhs <= 96) {
            return la_solve_small_fastpath(A, m, B, m, X, m, n, nrhs);
        }
        ScratchVec<T> A_lu(n * n, &arena);
        std::copy(A, A + n * n, A_lu.begin());
        ScratchVec<std::int32_t> piv(n, &arena);
        if (!lu_blocked_inplace(A_lu.data(), n, piv.data(), n, n, 0)) return false;

        std::memcpy(X, B, n * nrhs * sizeof(T));

        const std::int32_t *piv_ptr = piv.data();
        const std::size_t p_thresh = (nrhs >= 64) ? 32 : (nrhs + 1);
        numkit::detail::parallel_for(nrhs, p_thresh, [=](std::size_t c_start, std::size_t c_end) {
            for (std::size_t k = 0; k < n; ++k) {
                std::size_t p = static_cast<std::size_t>(piv_ptr[k]);
                if (p != k) {
                    T *r1 = X + k + c_start * n;
                    T *r2 = X + p + c_start * n;
                    std::size_t bcols = c_end - c_start;
                    for (std::size_t col = 0; col < bcols; ++col) {
                        std::swap(r1[col * n], r2[col * n]);
                    }
                }
            }
        });

        numkit::detail::parallel_for(nrhs, (nrhs >= 64) ? 16 : 1, [=, &A_lu](std::size_t c_start, std::size_t c_end) {
            std::size_t cols = c_end - c_start;
            T* X_slice = X + c_start * n;
            trsm_L_blocked_seq(n, cols, A_lu.data(), n, X_slice, n);
            trsm_U_blocked_seq(n, cols, A_lu.data(), n, X_slice, n);
        });

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
