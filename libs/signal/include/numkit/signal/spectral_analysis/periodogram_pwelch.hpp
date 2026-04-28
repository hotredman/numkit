// libs/signal/include/numkit/signal/spectral_analysis/periodogram_pwelch.hpp
//
// Power spectrum estimation: periodogram + Welch's method.
// spectrogram (the time-frequency variant of pwelch that retains every
// segment) lives in time_frequency/spectrogram.hpp.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Single-segment periodogram power spectral density estimate.
/// Returns (Pxx, F).
///
/// @param window  Optional window vector of length numel(x). Pass an empty
///                Value to use a rectangular window (all ones).
/// @param nfft    FFT size. Pass 0 to auto-pick nextPow2(numel(x)).
std::tuple<Value, Value>
periodogram(Allocator &alloc, const Value &x, const Value &window, size_t nfft);

/// Welch's method: averaged, modified periodogram. Returns (Pxx, F).
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
std::tuple<Value, Value>
pwelch(Allocator &alloc,
       const Value &x,
       const Value &window,
       size_t noverlap,
       size_t nfft);

} // namespace numkit::signal
