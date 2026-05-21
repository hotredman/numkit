// libs/signal/include/numkit/signal/time_frequency/spectrogram.hpp
//
// Short-time Fourier transform. periodogram / pwelch (which collapse the
// time axis) live in spectral_analysis/periodogram_pwelch.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Short-time Fourier transform with explicit `(S, F, T)` outputs.
///
/// Slides a window of length `winLen` across `x` with `noverlap` samples
/// of overlap, computes the FFT of each windowed segment, and returns
/// the resulting time–frequency matrix.
///
/// @param x         1-D signal (row or column).
/// @param window    Window vector. `Value::Empty` → Hamming of length
///                  `min(256, numel(x))`.
/// @param noverlap  Samples of overlap between consecutive segments.
///                  `0` → `winLen / 2`.
/// @param nfft      FFT size. `0` → `nextPow2(winLen)`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Tuple `(S, F, T)`:
///                    * `S` — `nfft/2+1 × numFrames` COMPLEX matrix
///                            of per-frame one-sided FFTs.
///                    * `F` — frequency axis in normalised cycles per
///                            sample, `nfft/2+1 × 1` column vector.
///                    * `T` — time axis (centre of each frame in samples),
///                            `numFrames × 1` column vector.
///
/// @see stft, istft, periodogram
std::tuple<Value, Value, Value>
spectrogram(const Value &                x,
            const Value &                window,
            size_t                       noverlap,
            size_t                       nfft,
            std::pmr::memory_resource *  mr = nullptr);

/// Full two-sided / centred / one-sided STFT.
///
/// Defaults:
///   * window = `hann(128, 'periodic')`
///   * overlap = 96 (75%)
///   * fftLength = 128
///
/// @param x          1-D signal (row or column).
/// @param window     Window vector. `Value::Empty` → 128-point Hann(periodic).
/// @param overlap    Samples shared between consecutive frames. `SIZE_MAX`
///                   sentinel → use default `3·winLen/4`.
/// @param fftLength  FFT size per frame, ≥ `winLen`. `0` → use `winLen`.
/// @param range      `"twosided"` (default), `"centered"`, or `"onesided"`.
/// @param mr         Memory resource (nullptr → process default).
/// @return           COMPLEX matrix sized `[rows × frames]` where
///                   `rows = fftLength` for twosided / centered,
///                   `rows = fftLength/2 + 1` for onesided.
///
/// @note KNOWN GAPS: `fs` argument and the 3-output `[S, F, T]` form
///       are deferred. Multi-channel matrix input is also deferred.
///
/// @see istft, spectrogram
Value stft(const Value &                x,
           const Value &                window,
           std::size_t                  overlap,
           std::size_t                  fftLength,
           const std::string &          range,
           std::pmr::memory_resource *  mr = nullptr);

/// Inverse STFT via overlap-add.
///
/// Round-trips `x == istft(stft(x, …), …)` exactly (within ulp) on
/// COLA-compliant configurations such as `hann(N, 'periodic')` with
/// 75% overlap.
///
/// Parameters must match the analysis-side spelling so we know whether
/// to undo a one-sided or centered packing before the inverse FFT.
///
/// @param S          STFT matrix produced by `stft`.
/// @param window     Same window vector used in analysis.
/// @param overlap    Same overlap.
/// @param fftLength  Same FFT length.
/// @param range      Same range keyword.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Reconstructed signal as a column vector.
///
/// @see stft
Value istft(const Value &                S,
            const Value &                window,
            std::size_t                  overlap,
            std::size_t                  fftLength,
            const std::string &          range,
            std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
