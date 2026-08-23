/// @file melspec_delta.hpp
/// @ingroup group_audio
// toolboxes/audio/include/numkit/audio/spectral/melspec_delta.hpp
//
// Audio Cycle C: melSpectrogram + audioDelta.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <tuple>

namespace numkit::audio {

/// Mel-band power spectrogram.
///
/// Defaults:
///   * `NumBands` = 32
///   * `FrequencyRange` = `[0, fs/2]`
///   * `MelStyle` = `'oshaughnessy'`
///   * `Window` = `hamming(round(0.03·fs), 'periodic')`
///   * `OverlapLength` = `round(0.02·fs)`
///   * `FFTLength` = `numel(Window)`
///   * `Normalization` = `'bandwidth'`
///   * `SpectrumType` = `'power'`
///   * `WindowNormalization` = `true`
///
/// @param x          Real 1-D audio signal.
/// @param fs         Sample rate in Hz.
/// @param numBands   Number of mel bands. Default 32.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(S, F, T)`:
///                     * `S` — `numBands × NumFrames` mel-band power matrix.
///                     * `F` — `1 × numBands` band-center frequencies (Hz).
///                     * `T` — `NumFrames × 1` frame-center times (seconds).
///
/// @see mfcc, hz2mel, audioDelta
std::tuple<Value, Value, Value>
melSpectrogram(const Value &                x,
               double                       fs,
               int                          numBands = 32,
               std::pmr::memory_resource *  mr       = nullptr);

/// Locally-smoothed first derivative along the time axis.
///
/// Implements the standard delta-coefficient filter:
/// \f$ \Delta x[n] = \frac{\sum_{k=1}^{M} k \cdot (x[n+k] - x[n-k])}{2 \sum_{k=1}^{M} k^2} \f$
/// where `M = floor(windowLength / 2)`.
///
/// @param x             Feature matrix (rows = time, cols = channels).
/// @param windowLength  Filter window length (odd, ≥ 3). Default 9.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Same-shape DOUBLE array of delta coefficients.
///
/// @see melSpectrogram, mfcc
Value audioDelta(const Value &                x,
                 int                          windowLength = 9,
                 std::pmr::memory_resource *  mr           = nullptr);

} // namespace numkit::audio
