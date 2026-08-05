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

// In-place LU with partial pivoting on a column-major n×n matrix.
template <typename T>
inline bool luPivotInplace(T *LU, std::int32_t *piv, std::size_t n) {
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot = k;
        double pmax = abs_val(LU[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = abs_val(LU[i + k * n]);
            if (v > pmax) {
                pmax = v;
                pivot = i;
            }
        }
        if (pmax == 0.0) return false;
        piv[k] = static_cast<std::int32_t>(pivot);
        if (pivot != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(LU[k + j * n], LU[pivot + j * n]);
        }
        const T inv_pivot = T(1) / LU[k + k * n];
        for (std::size_t i = k + 1; i < n; ++i) {
            const T factor = LU[i + k * n] * inv_pivot;
            LU[i + k * n] = factor;
            for (std::size_t j = k + 1; j < n; ++j)
                LU[i + j * n] -= factor * LU[k + j * n];
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
