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
// Cycle K-3 added Method='LHS' (Subharmonic Summation, Hermes 1988).
// Cycle K-4 added Method='SRH' (Summation of Residual Harmonics,
//   Drugman & Alwan 2011 — LPC inverse + harmonic summation).
// Cycle L (partial) added 'Range' NV arg — minF/maxF override [50, 400].
// All 5 MATLAB pitch methods now shipped. pitchnn deferred (DNN runtime).
// MedianFilterLength / WindowLength / OverlapLength NV args deferred.
Value pitch(std::pmr::memory_resource *mr, const Value &x, double fs,
             double minF = 50.0, double maxF = 400.0);

// Cepstrum-based pitch method (Method='CEP'). Same I/O as pitch().
Value pitchCEP(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF = 50.0, double maxF = 400.0);

// Pitch Estimation Filter method (Method='PEF'). Same I/O as pitch().
Value pitchPEF(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF = 50.0, double maxF = 400.0);

// Log Harmonic Sum method (Method='LHS'). Same I/O as pitch().
Value pitchLHS(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF = 50.0, double maxF = 400.0);

// Summation of Residual Harmonics method (Method='SRH'). Same I/O.
Value pitchSRH(std::pmr::memory_resource *mr, const Value &x, double fs,
                double minF = 50.0, double maxF = 400.0);

// harmonicRatio(x, fs) — strength of harmonic content per frame, via
// normalized autocorrelation peak. Range [0, 1]. Defaults: WindowLength=
// round(0.03*fs), OverlapLength=round(0.02*fs).
Value harmonicRatio(std::pmr::memory_resource *mr, const Value &x, double fs);

} // namespace numkit::audio
