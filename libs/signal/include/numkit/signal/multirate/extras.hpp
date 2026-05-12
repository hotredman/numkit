// libs/signal/include/numkit/signal/multirate/extras.hpp
//
// Multirate extras (F1): upfirdn, interp, intfilt, fftfilt.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// upfirdn(x, h, p[, q]) — upsample x by p, FIR-filter with h, downsample
/// by q (default 1). Returns the filtered + rate-converted signal.
Value upfirdn(const Value &x, const Value &h, size_t p, size_t q = 1, std::pmr::memory_resource *mr = nullptr);

/// interp(x, r[, n[, alpha]]) — interpolation by integer factor r using
/// a low-pass FIR of length 2*r*n+1 (n is the half-window in input
/// samples, default 4). alpha is a normalised passband edge, default 0.5
/// (cutoff at half of original Nyquist). Output length = numel(x) * r.
Value interp(const Value &x, size_t r, size_t n = 4, double alpha = 0.5, std::pmr::memory_resource *mr = nullptr);

/// intfilt(r, n, alpha) — design an FIR interpolation kernel for integer
/// upsampling by r. Length = 2*n*r+1. alpha is the passband edge in
/// fractions of the post-interpolation Nyquist (default 0.5).
Value intfilt(size_t r, size_t n = 4, double alpha = 0.5, std::pmr::memory_resource *mr = nullptr);

/// fftfilt(b, x[, nfft]) — overlap-add FFT-based FIR filtering. b is the
/// filter impulse response; x is the input signal. nfft chooses the
/// block FFT size (defaulted to a heuristic that's fast for typical
/// b, x). Output length = numel(x).
Value fftfilt(const Value &b, const Value &x, size_t nfft = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
