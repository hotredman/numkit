// ops/include/numkit/ops/blas.hpp
//
// Umbrella header for BLAS-like operations.

#pragma once

#include <numkit/ops/blas1.hpp>
#include <numkit/ops/blas2.hpp>
#include <numkit/ops/blas3.hpp>
#include <atomic>
#include <cstdint>

namespace numkit::ops {

/// @brief Native Highway SIMD LU panel factorization kernel.
bool lu_panel(double *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row);

/// @brief Native Highway SIMD Complex LU panel factorization kernel.
bool lu_panel(std::complex<double> *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n, std::size_t offset_row);

/// @brief Returns the number of threads used in the most recent gemm call.
inline std::atomic<std::size_t> g_last_gemm_threads_used{0};
inline std::size_t get_last_gemm_threads_used() {
    return g_last_gemm_threads_used.load();
}

} // namespace numkit::ops
