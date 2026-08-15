// toolboxes/linalg/src/decompositions_detail.hpp
//
// PRIVATE (src-level) header — NOT part of the public linalg API.
//
// Shared raw-buffer / low-level factorisation kernels between compute
// (decompositions.cpp) and its register half (decompositions_reg.cpp).

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <memory_resource>
#include <tuple>
#include <vector>

#include <numkit/value/value.hpp>
#include <numkit/ops/blas.hpp>
#include "linalg_detail.hpp"

namespace numkit::linalg {

// Build upper-triangular R (column-major, n×n) such that R'·R = A, reading the
// upper triangle of A. Returns the 1-based column index where the
// factorization broke down (non-positive pivot), or 0 on success.
template <typename T>
std::size_t cholUpperFactor(const T *a, T *r, std::size_t n) {
    std::fill(r, r + n * n, T(0));
    for (std::size_t j = 0; j < n; ++j) {
        T s = a[j + j * n];
        for (std::size_t k = 0; k < j; ++k) {
            if constexpr (detail::is_complex_v<T>) {
                s -= detail::abs_sq(r[k + j * n]);
            } else {
                s -= r[k + j * n] * r[k + j * n];
            }
        }
        if constexpr (detail::is_complex_v<T>) {
            if (s.real() <= 0.0 || std::abs(s.imag()) > 1e-12 * (1.0 + std::abs(s.real()))) return j + 1;
            r[j + j * n] = T(std::sqrt(s.real()), 0.0);
        } else {
            if (s <= 0.0) return j + 1;
            r[j + j * n] = std::sqrt(s);
        }
        const T inv_diag = T(1) / r[j + j * n];
        for (std::size_t i = j + 1; i < n; ++i) {
            T t = a[j + i * n];
            if constexpr (detail::is_complex_v<T>) {
                const T a_ji = a[i + j * n];
                if (std::abs(t - std::conj(a_ji)) > 1e-12 * (1.0 + std::abs(t))) return j + 1;
            }
            for (std::size_t k = 0; k < j; ++k) {
                if constexpr (detail::is_complex_v<T>) {
                    t -= std::conj(r[k + j * n]) * r[k + i * n];
                } else {
                    t -= r[k + j * n] * r[k + i * n];
                }
            }
            r[j + i * n] = t * inv_diag;
        }
    }
    return 0;
}

// Transpose (conjugate transpose for complex) a square k×k column-major matrix into a fresh Value.
template <typename T>
Value transposeSquare(const T *src, std::size_t k, std::pmr::memory_resource *mr) {
    Value out = detail::make_matrix<T>(k, k, mr);
    T *d = detail::get_data_mut<T>(out);
    for (std::size_t col = 0; col < k; ++col) {
        for (std::size_t row = 0; row < k; ++row) {
            d[row + col * k] = detail::conj_if_complex(src[col + row * k]);
        }
    }
    return out;
}

// Column-pivoted QR (the [Q,R,P] form).
std::tuple<Value, Value>
qr_pivoted(const Value &A, std::vector<std::size_t> &perm,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
