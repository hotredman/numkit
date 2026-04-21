// libs/dsp/include/numkit/m/dsp/MDspResample.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::dsp {

/// Integer-rate downsampling without anti-alias filtering: y(i) = x(i*n).
/// Output length = ceil(nx / n). Orientation (row/column) preserved.
MValue downsample(Allocator &alloc, const MValue &x, size_t n);

/// Integer-rate upsampling with zero-stuffing: y(i*n) = x(i), else 0.
/// Output length = nx * n. Orientation preserved.
MValue upsample(Allocator &alloc, const MValue &x, size_t n);

/// Integer-factor decimation: anti-alias FIR lowpass followed by downsample.
/// FIR cutoff is 1 / factor of Nyquist; order 8 * factor (clamped to nx-1).
MValue decimate(Allocator &alloc, const MValue &x, size_t factor);

/// Rational rate conversion: upsample by p, anti-alias lowpass, downsample by q.
/// Output length = ceil(nx * p / q). MATLAB's resample(x, p, q) with default
/// 10 * max(p, q) FIR order.
MValue resample(Allocator &alloc, const MValue &x, size_t p, size_t q);

} // namespace numkit::m::dsp
