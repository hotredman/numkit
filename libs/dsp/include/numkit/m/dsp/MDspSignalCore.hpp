// libs/dsp/include/numkit/m/dsp/MDspSignalCore.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m::dsp {

/// Smallest integer p such that 2^p >= n. Returns 0 for n <= 0.
/// MATLAB's nextpow2.
MValue nextpow2(Allocator &alloc, double n);

/// Cyclic shift by N/2 — zero-frequency bin moves to the center.
/// For real and complex input. Operates element-wise across numel(x).
MValue fftshift(Allocator &alloc, const MValue &x);

/// Inverse of fftshift — cyclic shift by (N+1)/2.
MValue ifftshift(Allocator &alloc, const MValue &x);

/// chirp(t, f0, t1, f1[, method]) — frequency-modulated cosine.
/// Output has the same shape as `t` and is always DOUBLE.
///   method = "linear":      y = cos(2π·(f0·t + ((f1-f0)/(2·t1))·t²))
///   method = "quadratic":   y = cos(2π·(f0·t + ((f1-f0)/(3·t1²))·t³))
///   method = "logarithmic": y = cos(2π·f0·((β^t - 1)/log(β))),
///                            β = (f1/f0)^(1/t1). Requires f0 > 0,
///                            f1 > 0, f1 != f0.
MValue chirp(Allocator &alloc, const MValue &t,
             double f0, double t1, double f1,
             const std::string &method = "linear");

} // namespace numkit::m::dsp
