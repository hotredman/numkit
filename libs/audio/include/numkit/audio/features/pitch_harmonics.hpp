// libs/audio/include/numkit/audio/features/pitch_harmonics.hpp
//
// Audio Cycle E: pitch + harmonicRatio.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

// pitch(x, fs) — fundamental frequency estimation per frame via NCF
// (Normalized Cross-correlation Function). Defaults: WindowLength=
// round(0.052*fs), OverlapLength=round(0.042*fs), Range=[50, 400] Hz.
// Returns column vector of f0 values per frame.
//
// KNOWN GAPs: Method='PEF'/'CEP'/'LHS'/'SRH' deferred (NCF only in v1).
// MedianFilterLength / Range / Window NV args deferred.
Value pitch(std::pmr::memory_resource *mr, const Value &x, double fs);

// harmonicRatio(x, fs) — strength of harmonic content per frame, via
// normalized autocorrelation peak. Range [0, 1]. Defaults: WindowLength=
// round(0.03*fs), OverlapLength=round(0.02*fs).
Value harmonicRatio(std::pmr::memory_resource *mr, const Value &x, double fs);

} // namespace numkit::audio
