// libs/signal/include/numkit/signal/transforms/transform_helpers.hpp
//
// Cross-cutting helpers for transform-domain code: nextpow2 (length
// rounding for FFT), fftshift / ifftshift (DC-to-center reordering).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Smallest integer p such that 2^p >= n. Returns 0 for n <= 0.
/// MATLAB's nextpow2.
Value nextpow2(std::pmr::memory_resource *mr, double n);

/// Cyclic shift by N/2 — zero-frequency bin moves to the center.
/// For real and complex input. Operates element-wise across numel(x).
Value fftshift(std::pmr::memory_resource *mr, const Value &x);

/// Inverse of fftshift — cyclic shift by (N+1)/2.
Value ifftshift(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::signal
