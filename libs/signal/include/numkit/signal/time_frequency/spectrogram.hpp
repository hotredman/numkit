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

/// stft(x, ...) — full two-sided / centered / one-sided STFT matching
/// MATLAB's `stft`. Defaults match MATLAB R2025b: window =
/// hann(128,'periodic'), overlap = 96, fftLength = 128.
///
/// @param x         1-D signal (row or column).
/// @param window    Window vector. Empty → 128-point Hann(periodic).
/// @param overlap   Samples shared between consecutive frames. SIZE_MAX
///                  → use 3*winLen/4 default.
/// @param fftLength FFT size per frame (≥ winLen). 0 → use winLen.
/// @param range     'twosided' (default), 'centered', or 'onesided'.
/// @return          Complex matrix sized [rows × frames] where rows =
///                  fftLength for twosided / centered, fftLength/2+1 for
///                  onesided.
///
/// KNOWN GAPS: fs argument and the 3-output `[S, F, T]` form (time /
/// frequency axes) are deferred. Multi-channel matrix input is also
/// deferred (currently only vectors).
Value stft(std::pmr::memory_resource *mr,
           const Value &x,
           const Value &window,
           std::size_t overlap,
           std::size_t fftLength,
           const std::string &range);

/// istft(S, ...) — inverse STFT via overlap-add with the same synthesis
/// window used for analysis. Round-trips x = istft(stft(x, ...))
/// exactly (within ulp) on COLA-compliant configurations such as
/// hann(N, 'periodic') with 75% overlap.
///
/// Signature mirrors stft. Reconstructed signal is returned as a column
/// vector. `range` must match the analysis-side spelling so we know
/// whether to undo a one-sided or centered packing before the IFFT.
Value istft(std::pmr::memory_resource *mr,
            const Value &S,
            const Value &window,
            std::size_t overlap,
            std::size_t fftLength,
            const std::string &range);

} // namespace numkit::signal
