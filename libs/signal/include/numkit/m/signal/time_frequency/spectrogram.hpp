// libs/signal/include/numkit/m/signal/time_frequency/spectrogram.hpp
//
// Short-time Fourier transform. periodogram / pwelch (which collapse the
// time axis) live in spectral_analysis/periodogram_pwelch.hpp.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

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
