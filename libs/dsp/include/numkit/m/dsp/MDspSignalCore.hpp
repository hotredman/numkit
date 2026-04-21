// libs/dsp/include/numkit/m/dsp/MDspSignalCore.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::dsp {

/// Smallest integer p such that 2^p >= n. Returns 0 for n <= 0.
/// MATLAB's nextpow2.
MValue nextpow2(Allocator &alloc, double n);

/// Cyclic shift by N/2 — zero-frequency bin moves to the center.
/// For real and complex input. Operates element-wise across numel(x).
MValue fftshift(Allocator &alloc, const MValue &x);

/// Inverse of fftshift — cyclic shift by (N+1)/2.
MValue ifftshift(Allocator &alloc, const MValue &x);

} // namespace numkit::m::dsp
