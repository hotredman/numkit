// libs/signal/include/numkit/signal/multirate/multirate.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Integer-rate downsampling without anti-alias filtering: y(i) = x(i*n).
/// Output length = ceil(nx / n). Orientation (row/column) preserved.
Value downsample(const Value &x, size_t n, std::pmr::memory_resource *mr = nullptr);

/// Integer-rate upsampling with zero-stuffing: y(i*n) = x(i), else 0.
/// Output length = nx * n. Orientation preserved.
Value upsample(const Value &x, size_t n, std::pmr::memory_resource *mr = nullptr);

/// Integer-factor decimation: anti-alias FIR lowpass followed by downsample.
/// FIR cutoff is 1 / factor of Nyquist; order 8 * factor (clamped to nx-1).
Value decimate(const Value &x, size_t factor, std::pmr::memory_resource *mr = nullptr);

/// Rational rate conversion: upsample by p, anti-alias lowpass, downsample by q.
/// Output length = ceil(nx * p / q). MATLAB's resample(x, p, q) with default
/// 10 * max(p, q) FIR order.
Value resample(const Value &x, size_t p, size_t q, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
