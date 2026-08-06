// toolboxes/linalg/src/linalg_detail.hpp
//
// PRIVATE (src-level) header — NOT part of the public linalg API.
// Templated kernel access layer helpers for double & std::complex<double>.

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <type_traits>
#include <vector>

#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/blas.hpp>

namespace numkit::linalg::detail {

using Complex = std::complex<double>;

// Helper to determine if type is std::complex<T>
template <typename T>
struct is_complex : std::false_type {};

template <typename T>
struct is_complex<std::complex<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_complex_v = is_complex<T>::value;

// Conjugate helper: conj for complex, identity for real
template <typename T>
inline T conj_if_complex(const T &val) {
    if constexpr (is_complex_v<T>) {
        return std::conj(val);
    } else {
        return val;
    }
}

// Absolute value squared: |x|^2
template <typename T>
inline double abs_sq(const T &val) {
    if constexpr (is_complex_v<T>) {
        return std::norm(val);
    } else {
        return val * val;
    }
}

// Magnitude: |x|
template <typename T>
inline double abs_val(const T &val) {
    if constexpr (is_complex_v<T>) {
        return std::abs(val);
    } else {
        return std::abs(val);
    }
}

// Real part helper
template <typename T>
inline double real_part(const T &val) {
    if constexpr (is_complex_v<T>) {
        return val.real();
    } else {
        return val;
    }
}

// Data pointer accessors for Value
template <typename T>
inline const T *get_data(const Value &v) {
    if constexpr (is_complex_v<T>) {
        return v.complexData();
    } else {
        return v.doubleData();
    }
}

template <typename T>
inline T *get_data_mut(Value &v) {
    if constexpr (is_complex_v<T>) {
        return v.complexDataMut();
    } else {
        return v.doubleDataMut();
    }
}

// Value matrix factory for double / complex
template <typename T>
inline Value make_matrix(std::size_t rows, std::size_t cols,
                         std::pmr::memory_resource *mr = nullptr) {
    if constexpr (is_complex_v<T>) {
        return Value::complexMatrix(rows, cols, mr);
    } else {
        return Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    }
}

// Narrowing helper: converts a complex Value to a real Value if all imaginary parts are zero (+/- 0)
inline Value narrow_if_real(const Value &v, std::pmr::memory_resource *mr = nullptr) {
    if (!v.isComplex()) {
        return v;
    }
    const Complex *cd = v.complexData();
    const std::size_t n = v.numel();
    for (std::size_t i = 0; i < n; ++i) {
        if (cd[i].imag() != 0.0) {
            return v;
        }
    }
    // All imaginary parts are zero, narrow to double matrix
    Value out = Value::matrix(v.dims().rows(), v.dims().cols(), ValueType::DOUBLE, mr);
    double *dd = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        dd[i] = cd[i].real();
    }
    return out;
}

// In-place LU with partial pivoting on a column-major n×n matrix (blocked dgetrf pattern).
template <typename T>
inline bool luPivotInplace(T *LU, std::int32_t *piv, std::size_t n) {
    constexpr std::size_t nb = 64; // panel block size

    for (std::size_t k = 0; k < n; k += nb) {
        std::size_t kb = std::min(nb, n - k);

        // 1. Unblocked panel factorization on columns k .. k+kb-1
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
                const T factor = LU[i + j * n] * inv_pivot;
                LU[i + j * n] = factor;
                for (std::size_t col = j + 1; col < k + kb; ++col)
                    LU[i + col * n] -= factor * LU[j + col * n];
            }
        }

        // 2. Trailing matrix update via SIMD GEMM if remaining cols exist
        if (k + kb < n) {
            // Forward solve on panel row blocks U[k..k+kb-1, k+kb..n-1]
            for (std::size_t col = k + kb; col < n; ++col) {
                for (std::size_t i1 = 0; i1 < kb; ++i1) {
                    for (std::size_t i2 = 0; i2 < i1; ++i2) {
                        LU[(k + i1) + col * n] -= LU[(k + i1) + (k + i2) * n] * LU[(k + i2) + col * n];
                    }
                }
            }

            // Trailing submatrix update: LU[k+kb..n-1, k+kb..n-1] -= L21 * U12
            const std::size_t rem_rows = n - (k + kb);
            const std::size_t rem_cols = n - (k + kb);
            const T *L21 = LU + (k + kb) + k * n;
            const T *U12 = LU + k + (k + kb) * n;
            T *A22 = LU + (k + kb) + (k + kb) * n;

            if constexpr (is_complex_v<T>) {
                ::numkit::ops::gemm(rem_rows, rem_cols, kb, Complex(-1.0, 0.0),
                                   reinterpret_cast<const Complex*>(L21), n,
                                   reinterpret_cast<const Complex*>(U12), n,
                                   Complex(1.0, 0.0),
                                   reinterpret_cast<Complex*>(A22), n);
            } else {
                ::numkit::ops::gemm(rem_rows, rem_cols, kb, -1.0, L21, n, U12, n, 1.0, A22, n);
            }
        }
    }
    return true;
}

// Templated LU solve for square system A * X = B
template <typename T>
inline bool luSolveSquare(const T *A_in, std::size_t n, const T *B_in,
                          std::size_t nrhs, T *X_out, ScratchArena *scratch) {
    ScratchVec<T> LU(n * n, scratch);
    ScratchVec<std::int32_t> piv(n, scratch);
    std::copy(A_in, A_in + n * n, LU.begin());

    if (!luPivotInplace(LU.data(), piv.data(), n)) {
        return false;
    }

    ScratchVec<T> Y(n * nrhs, scratch);
    std::vector<std::size_t> perm(n);
    for (std::size_t i = 0; i < n; ++i) perm[i] = i;
    for (std::size_t k = 0; k < n; ++k)
        std::swap(perm[k], perm[piv[k]]);

    for (std::size_t col = 0; col < nrhs; ++col) {
        for (std::size_t row = 0; row < n; ++row) {
            Y[row + col * n] = B_in[perm[row] + col * n];
        }
    }

    // Forward solve L * Y = P * B
    for (std::size_t col = 0; col < nrhs; ++col) {
        for (std::size_t i = 0; i < n; ++i) {
            T s = Y[i + col * n];
            for (std::size_t k = 0; k < i; ++k) {
                s -= LU[i + k * n] * Y[k + col * n];
            }
            Y[i + col * n] = s;
        }
    }

    // Back solve U * X = Y
    for (std::size_t col = 0; col < nrhs; ++col) {
        for (std::intptr_t i = static_cast<std::intptr_t>(n) - 1; i >= 0; --i) {
            T s = Y[i + col * n];
            for (std::size_t k = static_cast<std::size_t>(i) + 1; k < n; ++k) {
                s -= LU[static_cast<std::size_t>(i) + k * n] * X_out[k + col * n];
            }
            X_out[static_cast<std::size_t>(i) + col * n] = s / LU[static_cast<std::size_t>(i) + static_cast<std::size_t>(i) * n];
        }
    }
    return true;
}

} // namespace numkit::linalg::detail
