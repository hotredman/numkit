// libs/audio/include/numkit/audio/spectral/cepstral.hpp
//
// Audio Cycle D: cepstral coefficient extractors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::audio {

// Generic cepstral extractor: log10/cubic-root rectification → DCT-II
// → keep first NumCoeffs (default 13). Input S is L × M (filterbank
// bands × frames). Output is M × NumCoeffs (frames first, MATLAB
// convention).
Value cepstralCoefficients(std::pmr::memory_resource *mr, const Value &S,
                           int numCoeffs = 13);

// Mel-frequency cepstral coefficients. Pipeline:
//   melSpectrogram(x, fs) → power → cepstralCoefficients
// Returns coeffs (NumFrames × (NumCoeffs+1)) with LogEnergy='append'
// MATLAB default. delta and deltaDelta returned as additional outputs
// (audioDelta of coeffs, then again).
std::tuple<Value, Value, Value>
mfcc(std::pmr::memory_resource *mr, const Value &x, double fs,
     int numCoeffs = 13);

// Gammatone cepstral coefficients. Same as mfcc but uses an ERB-spaced
// filterbank instead of mel.
std::tuple<Value, Value, Value>
gtcc(std::pmr::memory_resource *mr, const Value &x, double fs,
     int numCoeffs = 13);

} // namespace numkit::audio
