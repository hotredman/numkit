// libs/audio/include/numkit/audio/spectral/shape_descriptors.hpp
//
// MATLAB Audio Toolbox spectral shape descriptors. All take EITHER:
//   * (x, fs)  — x is column vector signal, fs is sample rate. Internal
//                STFT with rectwin(round(fs*0.03)), overlap=round(fs*0.02),
//                FFTLength=winLen. Returns per-frame column vector.
//   * (X, F)   — X is power spectrum (or magnitude), F is frequency
//                vector. X may be M-by-N (one spectrum per column). Returns
//                per-column metric value as a row vector.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

Value spectralCentroid    (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralSpread      (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralRolloffPoint(std::pmr::memory_resource *mr, const Value &x, const Value &f,
                           double percentile = 0.95);
Value spectralDecrease    (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralSlope       (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralFlux        (std::pmr::memory_resource *mr, const Value &x, const Value &f,
                           double p = 2.0);

// ── Cycle I: spectralCrest/Entropy/Flatness/Kurtosis/Skewness ─────────
// All match MATLAB R2025b Signal Toolbox semantics (per-frame STFT for
// time-domain input). Frequency moments (Kurtosis/Skewness) use the
// X-weighted central-moment formula from spectralSkewness.m / Kurtosis.m.
Value spectralCrest       (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralEntropy     (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralFlatness    (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralKurtosis    (std::pmr::memory_resource *mr, const Value &x, const Value &f);
Value spectralSkewness    (std::pmr::memory_resource *mr, const Value &x, const Value &f);

} // namespace numkit::audio
