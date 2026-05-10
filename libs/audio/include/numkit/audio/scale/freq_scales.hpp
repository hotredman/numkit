// libs/audio/include/numkit/audio/scale/freq_scales.hpp
//
// Frequency-scale conversions (Mel / Bark / ERB / phon-sone) used in
// audio feature extraction. All elementwise, double in/out.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

// Mel scale (O'Shaughnessy 1987 default form).
Value hz2mel(std::pmr::memory_resource *mr, const Value &hz);
Value mel2hz(std::pmr::memory_resource *mr, const Value &mel);

// Bark scale (Traunmüller 1990 with low/high-frequency corrections).
Value hz2bark(std::pmr::memory_resource *mr, const Value &hz);
Value bark2hz(std::pmr::memory_resource *mr, const Value &bark);

// ERB scale (Glasberg & Moore 1990 with MATLAB-exact constants).
Value hz2erb(std::pmr::memory_resource *mr, const Value &hz);
Value erb2hz(std::pmr::memory_resource *mr, const Value &erb);

// Loudness conversions (ISO 532-1).
Value phon2sone(std::pmr::memory_resource *mr, const Value &phon);
Value sone2phon(std::pmr::memory_resource *mr, const Value &sone);

} // namespace numkit::audio
