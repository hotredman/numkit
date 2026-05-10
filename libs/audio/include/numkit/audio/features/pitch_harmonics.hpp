// libs/audio/include/numkit/audio/features/pitch_harmonics.hpp
//
// Audio Cycle E: pitch + harmonicRatio.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

// pitch(x, fs) — fundamental frequency estimation per frame.
// Method='NCF' (default, Normalized Cross-correlation Function).
// Defaults: WindowLength=round(0.052*fs), OverlapLength=round(0.042*fs),
// Range=[50, 400] Hz. Returns column vector of f0 values per frame.
//
// Cycle K added Method='CEP' (Cepstrum, Noll 1967) via pitchCEP().
// Cycle K-2 added Method='PEF' (Pitch Estimation Filter, Gonzalez &
// Brookes 2011) via pitchPEF().
// KNOWN GAPs: Method='LHS'/'SRH' deferred (need libs/signal::fft fix
// for non-power-of-2 sizes — fft length = round(fs)).
// MedianFilterLength / Range / Window NV args deferred.
Value pitch(std::pmr::memory_resource *mr, const Value &x, double fs);

// Cepstrum-based pitch method (Method='CEP'). Same I/O as pitch().
Value pitchCEP(std::pmr::memory_resource *mr, const Value &x, double fs);

// Pitch Estimation Filter method (Method='PEF'). Same I/O as pitch().
Value pitchPEF(std::pmr::memory_resource *mr, const Value &x, double fs);

// harmonicRatio(x, fs) — strength of harmonic content per frame, via
// normalized autocorrelation peak. Range [0, 1]. Defaults: WindowLength=
// round(0.03*fs), OverlapLength=round(0.02*fs).
Value harmonicRatio(std::pmr::memory_resource *mr, const Value &x, double fs);

} // namespace numkit::audio
