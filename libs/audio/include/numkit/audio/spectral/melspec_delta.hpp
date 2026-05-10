// libs/audio/include/numkit/audio/spectral/melspec_delta.hpp
//
// Audio Cycle C: melSpectrogram + audioDelta.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::audio {

// melSpectrogram(x, fs) — mel-band power spectrogram.
// Returns (S, F, T) where:
//   S — NumBands × NumFrames mel-band power matrix
//   F — 1 × NumBands center frequencies (Hz) on the mel scale
//   T — NumFrames × 1 frame center times (seconds)
// Defaults: NumBands=32, FrequencyRange=[0, fs/2], MelStyle='oshaughnessy',
//           Window=hamming(round(0.03*fs), periodic), OverlapLength=round(0.02*fs),
//           FFTLength=numel(Window), Normalization='bandwidth',
//           SpectrumType='power', WindowNormalization=true.
std::tuple<Value, Value, Value>
melSpectrogram(std::pmr::memory_resource *mr, const Value &x, double fs,
               int numBands = 32);

// audioDelta(features [, windowLength]) — locally smoothed first
// derivative along dim 1. Default windowLength=9.
//   M = floor(windowLength/2)
//   b = (M:-1:-M) / sum((1:M).^2)
//   delta = filter(b, 1, x, [], 1)
Value audioDelta(std::pmr::memory_resource *mr, const Value &x,
                 int windowLength = 9);

} // namespace numkit::audio
