// toolboxes/linalg/src/linalg_detail.hpp
//
// PRIVATE (src-level) header — NOT part of the public linalg API.
// Templated kernel access layer helpers for double & std::complex<double>.

#pragma once

#include <cmath>
#include <complex>
#include <cstddef>
#include <memory_resource>
#include <type_traits>

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

} // namespace numkit::linalg::detail
