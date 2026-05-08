// libs/signal/include/numkit/signal/transforms/transform_helpers.hpp
//
// Cross-cutting helpers for transform-domain code: nextpow2 (length
// rounding for FFT), fftshift / ifftshift (DC-to-center reordering).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Smallest integer p such that 2^p >= n. Returns 0 for n <= 0.
/// MATLAB's nextpow2 (scalar form).
Value nextpow2(std::pmr::memory_resource *mr, double n);

/// Vectorized form: applies nextpow2 elementwise to any-shape input.
/// Returns a DOUBLE array with the same shape as `x`.
Value nextpow2(std::pmr::memory_resource *mr, const Value &x);

/// Cyclic shift along every non-singleton dim by ceil(extent/2). For odd
/// extents the first ceil(N/2) elements move to the back. Matches MATLAB
/// R2025b for vectors, matrices, and 3-D arrays.
Value fftshift(std::pmr::memory_resource *mr, const Value &x);

/// Inverse of fftshift — cyclic shift by floor(extent/2) along every
/// non-singleton dim. fftshift and ifftshift are inverses for any extent.
Value ifftshift(std::pmr::memory_resource *mr, const Value &x);

/// Single-dim form: shift only along the requested dim (1=rows, 2=cols,
/// 3=pages). MATLAB syntax: fftshift(X, dim).
Value fftshift(std::pmr::memory_resource *mr, const Value &x, int dim);

/// Single-dim form for ifftshift.
Value ifftshift(std::pmr::memory_resource *mr, const Value &x, int dim);

} // namespace numkit::signal
