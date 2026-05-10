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

// Mel-frequency cepstral coefficients (Cycle G — bit-equal MATLAB R2025b).
// Pipeline: hamming(0.03*fs,'periodic') STFT, |FFT| magnitude, Slaney
// mel filterbank ('Bandwidth' norm), log10+DCT-II via cepstralCoefficients,
// natural-log unwindowed-frame energy prepended ('append' default).
// Returns (coeffs, delta, deltaDelta) all (NumFrames × (NumCoeffs+1)).
std::tuple<Value, Value, Value>
mfcc(std::pmr::memory_resource *mr, const Value &x, double fs,
     int numCoeffs = 13);

// Gammatone cepstral coefficients (Cycle H — bit-equal MATLAB R2025b).
// Same STFT + cepstralCoefficients pipeline as mfcc but with ERB-spaced
// Patterson-Holdsworth gammatone filterbank (Slaney 1993): cascaded
// 4-stage biquad freq response, FrequencyRange=[50, fs/2],
// NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)).
std::tuple<Value, Value, Value>
gtcc(std::pmr::memory_resource *mr, const Value &x, double fs,
     int numCoeffs = 13);

} // namespace numkit::audio
