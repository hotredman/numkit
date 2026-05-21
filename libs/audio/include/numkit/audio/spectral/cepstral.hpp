// libs/audio/include/numkit/audio/spectral/cepstral.hpp
//
// Audio Cycle D: cepstral coefficient extractors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::audio {

/// Generic cepstral coefficient extractor.
///
/// Pipeline: log10 / cubic-root rectification → DCT-II → keep first
/// `numCoeffs` coefficients. Input `S` is `L × M` (filterbank bands ×
/// frames). Output is `M × numCoeffs` (frames first).
///
/// @param S          `L × M` real filterbank energy matrix.
/// @param numCoeffs  Number of cepstral coefficients to keep. Default 13.
/// @param mr         Memory resource (nullptr → process default).
/// @return           `M × numCoeffs` DOUBLE matrix.
///
/// @see mfcc, gtcc
Value cepstralCoefficients(const Value &                S,
                           int                          numCoeffs = 13,
                           std::pmr::memory_resource *  mr        = nullptr);

/// Mel-frequency cepstral coefficients (MFCC).
///
/// Pipeline:
///   1. Hamming `(0.03 · fs, 'periodic')` STFT.
///   2. `|FFT|` magnitude.
///   3. Slaney mel filterbank (`'Bandwidth'` normalisation).
///   4. log10 + DCT-II via `cepstralCoefficients`.
///   5. Natural-log unwindowed-frame energy prepended (`'append'` default).
///
/// @param x          Real 1-D audio signal.
/// @param fs         Sample rate in Hz.
/// @param numCoeffs  Number of cepstral coefficients. Default 13.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(coeffs, delta, deltaDelta)` — three matrices,
///                   each `NumFrames × (numCoeffs + 1)`. `delta` and
///                   `deltaDelta` are the first and second time-derivative
///                   estimates of the coefficients.
///
/// @see cepstralCoefficients, audioDelta, gtcc
std::tuple<Value, Value, Value>
mfcc(const Value &                x,
     double                       fs,
     int                          numCoeffs = 13,
     std::pmr::memory_resource *  mr        = nullptr);

/// Gammatone cepstral coefficients (GTCC).
///
/// Same STFT + `cepstralCoefficients` pipeline
/// as `mfcc` but with an ERB-spaced Patterson-Holdsworth gammatone
/// filterbank (Slaney 1993): cascaded 4-stage biquad frequency response,
/// `FrequencyRange = [50, fs/2]`,
/// `NumFilters = ceil(hz2erb(fs/2) - hz2erb(50))`.
///
/// @param x          Real 1-D audio signal.
/// @param fs         Sample rate in Hz.
/// @param numCoeffs  Number of cepstral coefficients. Default 13.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(coeffs, delta, deltaDelta)`.
///
/// @see mfcc, hz2erb
std::tuple<Value, Value, Value>
gtcc(const Value &                x,
     double                       fs,
     int                          numCoeffs = 13,
     std::pmr::memory_resource *  mr        = nullptr);

} // namespace numkit::audio
