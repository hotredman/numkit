// libs/signal/include/numkit/signal/transforms/transform_helpers.hpp
//
// Cross-cutting helpers for transform-domain code: nextpow2 (length
// rounding for FFT), fftshift / ifftshift (DC-to-center reordering).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Smallest integer p such that \f$ 2^p \ge n \f$ (scalar form).
///
/// MATLAB's `nextpow2`. Useful for padding sequences to a length suitable
/// for radix-2 FFT.
///
/// @param n   Input scalar. Negative / zero → returns 0.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar DOUBLE.
///
/// @code  size_t pad = 1ull << (size_t)nextpow2(numel(x)).toScalar();  @endcode
Value nextpow2(double n, std::pmr::memory_resource *mr = nullptr);

/// Vectorised `nextpow2` — applies elementwise to an array.
///
/// @param x   Real input array (any shape).
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array with the same shape as `x`.
Value nextpow2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// DC-to-center reordering for FFT output (multidim form).
///
/// Cyclically shifts `x` by `ceil(extent/2)` along every non-singleton
/// dimension. After `fftshift(fft(x))` the DC component (`k = 0`) lies
/// at the center of each axis. For odd extents the first `ceil(N/2)`
/// elements move to the back.
///
/// @param x   Input array (any shape, any type).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape and same-type array with elements permuted.
///
/// @see ifftshift, fft
Value fftshift(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse of @ref fftshift — shift by `floor(extent/2)`.
///
/// `fftshift` and `ifftshift` are exact inverses for any extent (they
/// differ only when N is odd; for even N they coincide).
///
/// @param x   Input array (any shape, any type).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape and same-type array with elements permuted.
/// @see fftshift
Value ifftshift(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Single-dim `fftshift` — shift only along axis `dim`.
///
/// @param x    Input array.
/// @param dim  Axis (1 = rows, 2 = cols, 3 = pages).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape array shifted along `dim`.
Value fftshift(const Value &                x,
               int                          dim,
               std::pmr::memory_resource *  mr = nullptr);

/// @brief Single-dim `ifftshift` — inverse of single-dim @ref fftshift.
///
/// @param x    Input array.
/// @param dim  Axis (1 = rows, 2 = cols, 3 = pages).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape array shifted along `dim`.
Value ifftshift(const Value &x, int dim,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
