// libs/signal/include/numkit/signal/time_frequency/spectrogram.hpp
//
// Short-time Fourier transform. periodogram / pwelch (which collapse the
// time axis) live in spectral_analysis/periodogram_pwelch.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Short-time Fourier transform. Returns (S, F, T).
/// S columns are per-segment FFTs (complex), F frequency axis, T time
/// centers of each segment.
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
std::tuple<Value, Value, Value>
spectrogram(std::pmr::memory_resource *mr,
            const Value &x,
            const Value &window,
            size_t noverlap,
            size_t nfft);

} // namespace numkit::signal
