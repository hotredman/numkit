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

/// rectpuls(t[, w]) — unit-amplitude rectangular pulse on (-w/2, w/2).
/// Default w = 1. y = 1 inside the support, 0 outside; the boundary
/// |t| == w/2 returns 0 (matches MATLAB's open interval).
MValue rectpuls(Allocator &alloc, const MValue &t, double w = 1.0);

/// tripuls(t[, w]) — unit-amplitude triangular pulse on |t| ≤ w/2.
/// y = max(1 - 2·|t|/w, 0). Default w = 1.
MValue tripuls(Allocator &alloc, const MValue &t, double w = 1.0);

/// gauspuls(t, fc[, bw]) — Gaussian-modulated sinusoid.
/// y = exp(-α·t²) · cos(2π·fc·t) where α is set by the fractional
/// bandwidth bw at the -6dB envelope level (MATLAB convention,
/// bwr = -6 dB hard-coded). Default bw = 0.5.
MValue gauspuls(Allocator &alloc, const MValue &t, double fc, double bw = 0.5);

/// pulstran(t, d, fnName[, args...]) — pulse train: ∑_i fn(t - d_i, args).
/// fnName is one of "rectpuls" / "tripuls" / "gauspuls" — custom
/// (anonymous) function handles throw m:pulstran:fnUnsupported until
/// the engine callback API lands. The trailing args are forwarded to
/// the named generator (e.g. pulstran(t, d, "gauspuls", fc)).
MValue pulstran(Allocator &alloc, const MValue &t, const MValue &d,
                const std::string &fnName, double fcOrW = 1.0,
                double bw = 0.5);

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
