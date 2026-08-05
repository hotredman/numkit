// ops/src/la_solve.cpp

#include <numkit/ops/la_solve.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

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

// ── LU with partial pivoting (square A, n×n) ─────────────────────────
template <typename T>
bool lu_partial_pivot(T *A, std::int32_t *piv, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot_row = k;
        double pivot_val = abs_val(A[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = abs_val(A[i + k * n]);
            if (v > pivot_val) { pivot_val = v; pivot_row = i; }
        }
        if (pivot_val == 0.0) return false;
        piv[k] = static_cast<std::int32_t>(pivot_row);
        if (pivot_row != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(A[k + j * n], A[pivot_row + j * n]);
        }
        const T inv_pivot = T(1) / A[k + k * n];
        for (std::size_t i = k + 1; i < n; ++i) {
            const T factor = A[i + k * n] * inv_pivot;
            A[i + k * n] = factor;
            for (std::size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= factor * A[k + j * n];
        }
    }
    return true;
}

inline void apply_piv(const std::int32_t *piv, double *bx, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t p = static_cast<std::size_t>(piv[k]);
        if (p != k) std::swap(bx[k], bx[p]);
    }
}

inline void apply_piv(const std::int32_t *piv, Complex *bx, std::size_t n)
{
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t p = static_cast<std::size_t>(piv[k]);
        if (p != k) std::swap(bx[k], bx[p]);
    }
}

template <typename T>
void lu_solve_one(const T *LU, const std::int32_t *piv,
                  T *bx, std::size_t n)
{
    apply_piv(piv, bx, n);
    for (std::size_t i = 1; i < n; ++i) {
        T s = bx[i];
        for (std::size_t k = 0; k < i; ++k) s -= LU[i + k * n] * bx[k];
        bx[i] = s;
    }
    for (std::size_t i = n; i-- > 0;) {
        T s = bx[i];
        for (std::size_t k = i + 1; k < n; ++k) s -= LU[i + k * n] * bx[k];
        bx[i] = s / LU[i + i * n];
    }
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
        if (!lu_partial_pivot(A_lu.data(), piv.data(), n)) return false;
        for (std::size_t j = 0; j < nrhs; ++j) {
            T *xj = X + j * n;
            for (std::size_t i = 0; i < n; ++i) xj[i] = B[i + j * n];
            lu_solve_one(A_lu.data(), piv.data(), xj, n);
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
