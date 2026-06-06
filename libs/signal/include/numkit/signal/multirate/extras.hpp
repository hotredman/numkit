// libs/signal/include/numkit/signal/multirate/extras.hpp
//
// Multirate extras (F1): upfirdn, interp, intfilt, fftfilt.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Upsample-FIR-Downsample (polyphase) rational rate conversion.
///
/// Pipeline: upsample `x` by `p` (zero-stuffing) → FIR-filter with `h` →
/// downsample by `q`. The combined operation is implemented via a
/// polyphase decomposition so the actual cost is `numel(h)·numel(x)/q`
/// rather than the full `p·numel(x)·numel(h)/q`.
///
/// @param x   Input signal.
/// @param h   FIR filter coefficients.
/// @param p   Upsample factor, ≥ 1.
/// @param q   Downsample factor, ≥ 1. Default 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Output signal of length `ceil(((nx-1)·p + numel(h)) / q)`.
///
/// @see resample, interp
Value upfirdn(const Value &                x,
              const Value &                h,
              size_t                       p,
              size_t                       q  = 1,
              std::pmr::memory_resource *  mr = nullptr);

/// Integer-factor interpolation with a low-pass FIR kernel.
///
/// Designs an FIR low-pass of length `2·n·r + 1` and applies it after
/// zero-stuffing by `r`. Output length is `numel(x) · r`.
///
/// @param x      Input signal.
/// @param r      Interpolation factor, ≥ 2.
/// @param n      Half-window length in input samples. Default 4.
/// @param alpha  Normalised passband edge in fractions of input Nyquist.
///               Default 0.5.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Interpolated signal of length `numel(x) · r`.
///
/// @see intfilt, resample
Value interp(const Value &                x,
             size_t                       r,
             size_t                       n     = 4,
             double                       alpha = 0.5,
             std::pmr::memory_resource *  mr    = nullptr);

/// Design an FIR interpolation kernel.
///
/// Returns the impulse response usable directly with `upfirdn`. Length
/// `2·n·r + 1`. The filter is a windowed sinc with passband edge at
/// `alpha · (Nyquist / r)`.
///
/// @param r      Interpolation factor, ≥ 2.
/// @param n      Half-window length in input samples. Default 4.
/// @param alpha  Passband edge in fractions of the post-interpolation
///               Nyquist. Default 0.5.
/// @param mr     Memory resource (nullptr → process default).
/// @return       1 × (2·n·r + 1) DOUBLE FIR kernel.
///
/// @see interp, upfirdn
Value intfilt(size_t                       r,
              size_t                       n     = 4,
              double                       alpha = 0.5,
              std::pmr::memory_resource *  mr    = nullptr);

/// Overlap-add FFT-based FIR filtering.
///
/// Filters `x` with FIR `b` using the overlap-add method on blocks of
/// length `nfft`. For long signals and moderately long filters this is
/// substantially faster than `filter(b, [1], x)`.
///
/// @param b      FIR filter coefficients.
/// @param x      Input signal.
/// @param nfft   FFT block size. `0` (default) → chooses a heuristic
///               based on `numel(b)`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered signal of length `numel(x)`.
///
/// @see filter, conv
Value fftfilt(const Value &                b,
              const Value &                x,
              size_t                       nfft = 0,
              std::pmr::memory_resource *  mr   = nullptr);

} // namespace numkit::signal
