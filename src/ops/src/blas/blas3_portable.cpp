#include <numkit/ops/blas3.hpp>
#include <numkit/ops/blas.hpp>
#include <numkit/ops/parallel_for.hpp>
#include <numkit/ops/thread_pool.hpp>
#include <vector>
#include <complex>
#include <algorithm>

namespace numkit::ops {

template <typename T>
static constexpr bool is_complex_type_v = false;
template <>
constexpr bool is_complex_type_v<std::complex<double>> = true;

// Basic portable GEMM implementation with optional threading
template <typename T>
void gemm_portable(MatrixTranspose transa, MatrixTranspose transb,
                   std::size_t m, std::size_t n, std::size_t k,
                   T alpha, const T *A, std::size_t lda,
                   const T *B, std::size_t ldb,
                   T beta, T *C, std::size_t ldc)
{
    if (m == 0 || n == 0) return;

    if (k == 0 || alpha == T(0)) {
        if (beta == T(0)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) C[i + j * ldc] = T(0);
            }
        } else if (beta != T(1)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) C[i + j * ldc] *= beta;
            }
        }
        return;
    }

    const bool notransa = (transa == MatrixTranspose::NoTrans);
    const bool notransb = (transb == MatrixTranspose::NoTrans);
    const bool conj_a   = (transa == MatrixTranspose::ConjTrans);
    const bool conj_b   = (transb == MatrixTranspose::ConjTrans);

    constexpr std::size_t kParallelThreshold = 256;
    numkit::detail::parallel_for(n, kParallelThreshold, [=](std::size_t j_start, std::size_t j_end) {
        for (std::size_t j = j_start; j < j_end; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                T sum = T(0);
                if (notransa && notransb) {
                    for (std::size_t p = 0; p < k; ++p) sum += A[i + p * lda] * B[p + j * ldb];
                } else if (!notransa && notransb) {
                    for (std::size_t p = 0; p < k; ++p) {
                        T a_val = A[p + i * lda];
                        if constexpr (is_complex_type_v<T>) { if (conj_a) a_val = std::conj(a_val); }
                        sum += a_val * B[p + j * ldb];
                    }
                } else if (notransa && !notransb) {
                    for (std::size_t p = 0; p < k; ++p) {
                        T b_val = B[j + p * ldb];
                        if constexpr (is_complex_type_v<T>) { if (conj_b) b_val = std::conj(b_val); }
                        sum += A[i + p * lda] * b_val;
                    }
                } else {
                    for (std::size_t p = 0; p < k; ++p) {
                        T a_val = A[p + i * lda];
                        T b_val = B[j + p * ldb];
                        if constexpr (is_complex_type_v<T>) {
                            if (conj_a) a_val = std::conj(a_val);
                            if (conj_b) b_val = std::conj(b_val);
                        }
                        sum += a_val * b_val;
                    }
                }
                if (beta == T(0)) {
                    C[i + j * ldc] = alpha * sum;
                } else {
                    C[i + j * ldc] = beta * C[i + j * ldc] + alpha * sum;
                }
            }
        }
    });

    // Update global atomic for test determinism check
    ::numkit::ops::g_last_gemm_threads_used.store(numkit::detail::ThreadPool::global().workers());
}

template <typename T>
void gemm_dispatch(std::size_t m, std::size_t n, std::size_t k,
                   T alpha, const T *A, std::size_t lda,
                   const T *B, std::size_t ldb,
                   T beta, T *C, std::size_t ldc)
{
    gemm_portable(MatrixTranspose::NoTrans, MatrixTranspose::NoTrans, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

template <typename T>
void trsm_base(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
               std::size_t m, std::size_t n,
               T alpha, const T *A, std::size_t lda,
               T *B, std::size_t ldb)
{
    if (alpha != T(1)) {
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] *= alpha;
        }
    }
    const bool no_trans = (trans == MatrixTranspose::NoTrans);
    const bool is_conj = (trans == MatrixTranspose::ConjTrans);

    if (side == MatrixSide::Left) {
        if (no_trans) {
            if (uplo == MatrixUplo::Lower) {
                for (std::size_t j = 0; j < n; ++j) {
                    for (std::size_t i = 0; i < m; ++i) {
                        if (B[i + j * ldb] != T(0)) {
                            if (diag == MatrixDiag::NonUnit) B[i + j * ldb] /= A[i + i * lda];
                            for (std::size_t k = i + 1; k < m; ++k) B[k + j * ldb] -= B[i + j * ldb] * A[k + i * lda];
                        }
                    }
                }
            } else {
                for (std::size_t j = 0; j < n; ++j) {
                    for (std::size_t i = m; i-- > 0;) {
                        if (B[i + j * ldb] != T(0)) {
                            if (diag == MatrixDiag::NonUnit) B[i + j * ldb] /= A[i + i * lda];
                            for (std::size_t k = 0; k < i; ++k) B[k + j * ldb] -= B[i + j * ldb] * A[k + i * lda];
                        }
                    }
                }
            }
        } else {
            if (uplo == MatrixUplo::Lower) {
                for (std::size_t j = 0; j < n; ++j) {
                    for (std::size_t i = m; i-- > 0;) {
                        T temp = B[i + j * ldb];
                        for (std::size_t k = i + 1; k < m; ++k) {
                            T a_val = A[k + i * lda];
                            if constexpr (is_complex_type_v<T>) { if (is_conj) a_val = std::conj(a_val); }
                            temp -= a_val * B[k + j * ldb];
                        }
                        T diag_val = A[i + i * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) diag_val = std::conj(diag_val); }
                        if (diag == MatrixDiag::NonUnit) temp /= diag_val;
                        B[i + j * ldb] = temp;
                    }
                }
            } else {
                for (std::size_t j = 0; j < n; ++j) {
                    for (std::size_t i = 0; i < m; ++i) {
                        T temp = B[i + j * ldb];
                        for (std::size_t k = 0; k < i; ++k) {
                            T a_val = A[k + i * lda];
                            if constexpr (is_complex_type_v<T>) { if (is_conj) a_val = std::conj(a_val); }
                            temp -= a_val * B[k + j * ldb];
                        }
                        T diag_val = A[i + i * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) diag_val = std::conj(diag_val); }
                        if (diag == MatrixDiag::NonUnit) temp /= diag_val;
                        B[i + j * ldb] = temp;
                    }
                }
            }
        }
    } else {
        if (no_trans) {
            if (uplo == MatrixUplo::Lower) {
                for (std::size_t j = n; j-- > 0;) {
                    if (diag == MatrixDiag::NonUnit) {
                        for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] /= A[j + j * lda];
                    }
                    for (std::size_t k = 0; k < j; ++k) {
                        if (A[j + k * lda] != T(0)) {
                            for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] -= A[j + k * lda] * B[i + j * ldb];
                        }
                    }
                }
            } else {
                for (std::size_t j = 0; j < n; ++j) {
                    if (diag == MatrixDiag::NonUnit) {
                        for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] /= A[j + j * lda];
                    }
                    for (std::size_t k = j + 1; k < n; ++k) {
                        if (A[j + k * lda] != T(0)) {
                            for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] -= A[j + k * lda] * B[i + j * ldb];
                        }
                    }
                }
            }
        } else {
            if (uplo == MatrixUplo::Lower) {
                for (std::size_t k = 0; k < n; ++k) {
                    for (std::size_t j = 0; j < k; ++j) {
                        T a_val = A[k + j * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) a_val = std::conj(a_val); }
                        if (a_val != T(0)) {
                            for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] -= a_val * B[i + j * ldb];
                        }
                    }
                    if (diag == MatrixDiag::NonUnit) {
                        T diag_val = A[k + k * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) diag_val = std::conj(diag_val); }
                        for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= diag_val;
                    }
                }
            } else {
                for (std::size_t k = n; k-- > 0;) {
                    for (std::size_t j = k + 1; j < n; ++j) {
                        T a_val = A[k + j * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) a_val = std::conj(a_val); }
                        if (a_val != T(0)) {
                            for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] -= a_val * B[i + j * ldb];
                        }
                    }
                    if (diag == MatrixDiag::NonUnit) {
                        T diag_val = A[k + k * lda];
                        if constexpr (is_complex_type_v<T>) { if (is_conj) diag_val = std::conj(diag_val); }
                        for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= diag_val;
                    }
                }
            }
        }
    }
}

template <typename T>
void trsm_generic(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
                  std::size_t m, std::size_t n,
                  T alpha, const T *A, std::size_t lda,
                  T *B, std::size_t ldb)
{
    if (m == 0 || n == 0) return;
    
    // Fall back to base recursive structure just like in highway implementation
    if (side == MatrixSide::Left) {
        if (m <= 8) {
            trsm_base(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
            return;
        }
        if (alpha != T(1)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] *= alpha;
            }
        }
        std::size_t m1 = m / 2;
        std::size_t m2 = m - m1;
        
        if (uplo == MatrixUplo::Lower && trans == MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m1, n, T(1), A, lda, B, ldb);
            gemm_dispatch(m2, n, m1, T(-1), A + m1, lda, B, ldb, T(1), B + m1, ldb);
            trsm_generic(side, uplo, trans, diag, m2, n, T(1), A + m1 + m1 * lda, lda, B + m1, ldb);
        } else if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m2, n, T(1), A + m1 + m1 * lda, lda, B + m1, ldb);
            gemm_dispatch(m1, n, m2, T(-1), A + m1 * lda, lda, B + m1, ldb, T(1), B, ldb);
            trsm_generic(side, uplo, trans, diag, m1, n, T(1), A, lda, B, ldb);
        } else if (uplo == MatrixUplo::Upper && trans != MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m1, n, T(1), A, lda, B, ldb);
            std::vector<T> U01_T(m2 * m1);
            for(std::size_t c=0; c<m2; ++c) {
                for(std::size_t r=0; r<m1; ++r) {
                    T val = A[r + (m1 + c)*lda];
                    if constexpr (is_complex_type_v<T>) {
                        if (trans == MatrixTranspose::ConjTrans) val = std::conj(val);
                    }
                    U01_T[c + r*m2] = val; 
                }
            }
            gemm_dispatch(m2, n, m1, T(-1), U01_T.data(), m2, B, ldb, T(1), B + m1, ldb);
            trsm_generic(side, uplo, trans, diag, m2, n, T(1), A + m1 + m1 * lda, lda, B + m1, ldb);
        } else if (uplo == MatrixUplo::Lower && trans != MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m2, n, T(1), A + m1 + m1 * lda, lda, B + m1, ldb);
            std::vector<T> L10_T(m1 * m2);
            for(std::size_t c=0; c<m1; ++c) {
                for(std::size_t r=0; r<m2; ++r) {
                    T val = A[(m1 + r) + c*lda];
                    if constexpr (is_complex_type_v<T>) {
                        if (trans == MatrixTranspose::ConjTrans) val = std::conj(val);
                    }
                    L10_T[c + r*m1] = val;
                }
            }
            gemm_dispatch(m1, n, m2, T(-1), L10_T.data(), m1, B + m1, ldb, T(1), B, ldb);
            trsm_generic(side, uplo, trans, diag, m1, n, T(1), A, lda, B, ldb);
        }
    } else { // Right side
        if (n <= 8) {
            trsm_base(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
            return;
        }
        if (alpha != T(1)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] *= alpha;
            }
        }
        std::size_t n1 = n / 2;
        std::size_t n2 = n - n1;

        if (uplo == MatrixUplo::Lower && trans == MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m, n2, T(1), A + n1 + n1 * lda, lda, B + n1 * ldb, ldb);
            gemm_dispatch(m, n1, n2, T(-1), B + n1 * ldb, ldb, A + n1, lda, T(1), B, ldb);
            trsm_generic(side, uplo, trans, diag, m, n1, T(1), A, lda, B, ldb);
        } else if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m, n1, T(1), A, lda, B, ldb);
            gemm_dispatch(m, n2, n1, T(-1), B, ldb, A + n1 * lda, lda, T(1), B + n1 * ldb, ldb);
            trsm_generic(side, uplo, trans, diag, m, n2, T(1), A + n1 + n1 * lda, lda, B + n1 * ldb, ldb);
        } else if (uplo == MatrixUplo::Upper && trans != MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m, n2, T(1), A + n1 + n1 * lda, lda, B + n1 * ldb, ldb);
            std::vector<T> U01_T(n2 * n1);
            for(std::size_t c=0; c<n2; ++c) {
                for(std::size_t r=0; r<n1; ++r) {
                    T val = A[r + (n1 + c)*lda];
                    if constexpr (is_complex_type_v<T>) {
                        if (trans == MatrixTranspose::ConjTrans) val = std::conj(val);
                    }
                    U01_T[c + r*n2] = val; 
                }
            }
            gemm_dispatch(m, n1, n2, T(-1), B + n1 * ldb, ldb, U01_T.data(), n2, T(1), B, ldb);
            trsm_generic(side, uplo, trans, diag, m, n1, T(1), A, lda, B, ldb);
        } else if (uplo == MatrixUplo::Lower && trans != MatrixTranspose::NoTrans) {
            trsm_generic(side, uplo, trans, diag, m, n1, T(1), A, lda, B, ldb);
            std::vector<T> L10_T(n1 * n2);
            for(std::size_t c=0; c<n1; ++c) {
                for(std::size_t r=0; r<n2; ++r) {
                    T val = A[(n1 + r) + c*lda];
                    if constexpr (is_complex_type_v<T>) {
                        if (trans == MatrixTranspose::ConjTrans) val = std::conj(val);
                    }
                    L10_T[c + r*n1] = val;
                }
            }
            gemm_dispatch(m, n2, n1, T(-1), B, ldb, L10_T.data(), n1, T(1), B + n1 * ldb, ldb);
            trsm_generic(side, uplo, trans, diag, m, n2, T(1), A + n1 + n1 * lda, lda, B + n1 * ldb, ldb);
        }
    }
}

template <typename T>
void syrk_recursive(MatrixUplo uplo, MatrixTranspose trans,
                    std::size_t n, std::size_t k,
                    T alpha, const T *V_T, std::size_t ld_v_t, const T *V, std::size_t ld_v,
                    T *C, std::size_t ldc)
{
    if (n <= 8) {
        for (std::size_t j = 0; j < n; ++j) {
            std::size_t i_start = (uplo == MatrixUplo::Lower) ? j : 0;
            std::size_t i_end   = (uplo == MatrixUplo::Lower) ? n : j + 1;
            for (std::size_t i = i_start; i < i_end; ++i) {
                T sum = 0;
                for (std::size_t l = 0; l < k; ++l) {
                    sum += V_T[i + l * ld_v_t] * V[l + j * ld_v];
                }
                C[i + j * ldc] += alpha * sum;
            }
        }
        return;
    }

    std::size_t n1 = n / 2;
    std::size_t n2 = n - n1;

    if (uplo == MatrixUplo::Upper) {
        syrk_recursive(uplo, trans, n1, k, alpha, V_T, ld_v_t, V, ld_v, C, ldc);
        gemm_dispatch(n1, n2, k, alpha, V_T, ld_v_t, V + n1 * ld_v, ld_v, T(1), C + n1 * ldc, ldc);
        syrk_recursive(uplo, trans, n2, k, alpha, V_T + n1, ld_v_t, V + n1 * ld_v, ld_v, C + n1 + n1 * ldc, ldc);
    } else {
        syrk_recursive(uplo, trans, n1, k, alpha, V_T, ld_v_t, V, ld_v, C, ldc);
        gemm_dispatch(n2, n1, k, alpha, V_T + n1, ld_v_t, V, ld_v, T(1), C + n1, ldc);
        syrk_recursive(uplo, trans, n2, k, alpha, V_T + n1, ld_v_t, V + n1 * ld_v, ld_v, C + n1 + n1 * ldc, ldc);
    }
}

template <typename T>
void syrk_generic(MatrixUplo uplo, MatrixTranspose trans,
                  std::size_t n, std::size_t k,
                  T alpha, const T *A, std::size_t lda,
                  T beta, T *C, std::size_t ldc)
{
    if (n == 0) return;

    if (beta != T(1)) {
        for (std::size_t j = 0; j < n; ++j) {
            std::size_t i_start = (uplo == MatrixUplo::Lower) ? j : 0;
            std::size_t i_end   = (uplo == MatrixUplo::Lower) ? n : j + 1;
            if (beta == T(0)) {
                for (std::size_t i = i_start; i < i_end; ++i) C[i + j * ldc] = T(0);
            } else {
                for (std::size_t i = i_start; i < i_end; ++i) C[i + j * ldc] *= beta;
            }
        }
    }
    
    if (k == 0 || alpha == T(0)) return;

    const bool is_trans = (trans != MatrixTranspose::NoTrans);
    const bool is_conj  = (trans == MatrixTranspose::ConjTrans);

    std::vector<T> V_T(n * k);
    std::vector<T> V(k * n);
    
    if (!is_trans) {
        for (std::size_t col = 0; col < k; ++col) {
            for (std::size_t row = 0; row < n; ++row) {
                V_T[row + col * n] = A[row + col * lda];
                T val = A[row + col * lda];
                if constexpr (is_complex_type_v<T>) {
                    if (is_conj) val = std::conj(val);
                }
                V[col + row * k] = val;
            }
        }
    } else {
        for (std::size_t col = 0; col < n; ++col) {
            for (std::size_t row = 0; row < k; ++row) {
                V[row + col * k] = A[row + col * lda];
                T val = A[row + col * lda];
                if constexpr (is_complex_type_v<T>) {
                    if (is_conj) val = std::conj(val);
                }
                V_T[col + row * n] = val;
            }
        }
    }

    syrk_recursive(uplo, trans, n, k, alpha, V_T.data(), n, V.data(), k, C, ldc);
}

void gemm(MatrixTranspose transa, MatrixTranspose transb, std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc)
{
    gemm_portable(transa, transb, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

void gemm(MatrixTranspose transa, MatrixTranspose transb, std::size_t m, std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          const std::complex<double> *B, std::size_t ldb,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    gemm_portable(transa, transb, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          double *B, std::size_t ldb)
{
    trsm_generic(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
}

void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> *B, std::size_t ldb)
{
    trsm_generic(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
}

void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          double beta, double *C, std::size_t ldc)
{
    syrk_generic(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
}

void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    syrk_generic(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
}

bool lu_panel(std::complex<double> *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row)
{
    using Complex = std::complex<double>;
    for (std::size_t j = 0; j < n; ++j) {
        std::size_t pivot = j;
        double pmax = std::norm(A[j + j * lda]);
        
        for (std::size_t i = j + 1; i < m; ++i) {
            double v = std::norm(A[i + j * lda]);
            if (v > pmax) { pmax = v; pivot = i; }
        }

        if (pmax == 0.0) return false;
        piv[j] = static_cast<std::int32_t>(pivot + offset_row);
        if (pivot != j) {
            for (std::size_t col = 0; col < n; ++col) {
                std::swap(A[j + col * lda], A[pivot + col * lda]);
            }
        }
        
        const Complex inv_pivot = Complex(1.0, 0.0) / A[j + j * lda];
        for (std::size_t i = j + 1; i < m; ++i) {
            A[i + j * lda] *= inv_pivot;
        }

        for (std::size_t col = j + 1; col < n; ++col) {
            const Complex f = A[j + col * lda];
            if (f == Complex(0.0, 0.0)) continue;
            
            Complex *col_ptr = A + col * lda;
            const Complex *l_col = A + j * lda;
            
            std::size_t i = j + 1;
            for (; i + 4 <= m; i += 4) {
                col_ptr[i+0] -= l_col[i+0] * f;
                col_ptr[i+1] -= l_col[i+1] * f;
                col_ptr[i+2] -= l_col[i+2] * f;
                col_ptr[i+3] -= l_col[i+3] * f;
            }
            for (; i < m; ++i) {
                col_ptr[i] -= l_col[i] * f;
            }
        }
    }
    return true;
}

bool lu_panel(double *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row)
{
    for (std::size_t j = 0; j < n; ++j) {
        std::size_t pivot = j;
        double pmax = std::abs(A[j + j * lda]);
        
        for (std::size_t i = j + 1; i < m; ++i) {
            double v = std::abs(A[i + j * lda]);
            if (v > pmax) {
                pmax = v;
                pivot = i;
            }
        }
        
        if (pmax == 0.0) return false;
        
        piv[j] = static_cast<std::int32_t>(pivot + offset_row);
        
        if (pivot != j) {
            for (std::size_t col = 0; col < n; ++col) {
                std::swap(A[j + col * lda], A[pivot + col * lda]);
            }
        }
        
        double inv_pivot = 1.0 / A[j + j * lda];
        for (std::size_t i = j + 1; i < m; ++i) {
            A[i + j * lda] *= inv_pivot;
            double f = A[i + j * lda];
            for (std::size_t col = j + 1; col < n; ++col) {
                A[i + col * lda] -= f * A[j + col * lda];
            }
        }
    }
    return true;
}

} // namespace numkit::ops