// toolboxes/signal/include/numkit/signal/transforms/goertzel.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Goertzel algorithm — single-frequency DFT evaluation.
///
/// Returns the DFT of `x` at the (1-based) frequency indices in `ind`.
/// Faster than a full FFT when only a handful of bins are needed, since
/// it uses an O(N) IIR filter per output bin instead of O(N log N).
///
/// Typical use: DTMF / tone detection, where only a small fixed set of
/// frequency bins matters.
///
/// @param x    Real input signal (1-D vector).
/// @param ind  1-based DFT bin indices to evaluate (real, any shape).
///             Fractional indices are allowed (non-integer Goertzel).
/// @param mr   Memory resource (nullptr → process default).
/// @return     COMPLEX Value with the same shape as `ind`. Each entry
///             contains `X[ind[k] - 1]` where `X = fft(x, length(x))`.
///
/// @code
/// // Detect a 1.5 kHz tone in a 8 kHz-sampled signal of length 1024:
/// double bin = 1500.0 / 8000.0 * 1024.0;
/// Value Y = goertzel(x, {bin + 1.0});  // 1-based
/// double magnitude = std::abs(Y.complexData()[0]);
/// @endcode
///
/// @see fft
Value goertzel(const Value &                x,
               const Value &                ind,
               std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
