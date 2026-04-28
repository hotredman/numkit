// libs/dsp/include/numkit/m/signal/MDspSpectral.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

/// Single-segment periodogram power spectral density estimate.
/// Returns (Pxx, F).
///
/// @param window  Optional window vector of length numel(x). Pass an empty
///                MValue to use a rectangular window (all ones).
/// @param nfft    FFT size. Pass 0 to auto-pick nextPow2(numel(x)).
std::tuple<MValue, MValue>
periodogram(Allocator &alloc, const MValue &x, const MValue &window, size_t nfft);

/// Welch's method: averaged, modified periodogram. Returns (Pxx, F).
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
std::tuple<MValue, MValue>
pwelch(Allocator &alloc,
       const MValue &x,
       const MValue &window,
       size_t noverlap,
       size_t nfft);

/// Short-time Fourier transform. Returns (S, F, T).
/// S columns are per-segment FFTs (complex), F frequency axis, T time
/// centers of each segment.
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
std::tuple<MValue, MValue, MValue>
spectrogram(Allocator &alloc,
            const MValue &x,
            const MValue &window,
            size_t noverlap,
            size_t nfft);

} // namespace numkit::m::signal
