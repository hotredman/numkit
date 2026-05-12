// libs/signal/include/numkit/signal/transforms/hilbert.hpp
#pragma once

#include <memory_resource>
#include <utility>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Analytic signal via the discrete Hilbert transform.
///
/// Computes \f$ z[n] = x[n] + j \hat{x}[n] \f$, where \f$ \hat{x} \f$ is
/// the Hilbert transform of `x`. Implemented in the frequency domain:
/// FFT → zero out negative frequencies → double positive frequencies →
/// inverse FFT.
///
/// The result is useful for envelope detection, instantaneous frequency
/// estimation, and single-sideband modulation.
///
/// @param x   Real input signal (1-D vector).
/// @param mr  Memory resource (nullptr → process default).
/// @return    COMPLEX vector of the same length as `x`.
///
/// @code
/// Value z = hilbert(signal);
/// Value envelope = abs(z);        // amplitude envelope
/// Value phase    = angle(z);      // instantaneous phase
/// @endcode
///
/// @see envelope, envelope_full
Value hilbert(const Value &                x,
              std::pmr::memory_resource *  mr = nullptr);

/// Amplitude envelope of a real signal: `|hilbert(x)|`.
///
/// @param x   Real input signal.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE vector of the same length as `x`.
///
/// @see hilbert, envelope_full
Value envelope(const Value &                x,
               std::pmr::memory_resource *  mr = nullptr);

/// 2-output envelope (DC-symmetric form).
///
/// Computes `yupper = mean + |hilbert(x - mean)|`,
/// `ylower = mean - |hilbert(x - mean)|`. Subtracting the mean before
/// the Hilbert transform avoids edge artefacts from a DC offset.
///
/// @param x   Real input signal.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(yupper, ylower)` — bind with `auto [up, lo] = envelope_pair(x);`.
std::pair<Value, Value>
envelope_pair(const Value &                x,
              std::pmr::memory_resource *  mr = nullptr);

/// Full multi-mode envelope — matches MATLAB R2025b `envelope.m`.
///
/// @param x     Real input signal.
/// @param mode  Envelope algorithm:
///                - 0: default, FFT-based `|hilbert(x - mean)|`
///                - 1: 'analytic' — n-tap Kaiser-tapered Hilbert FIR
///                - 2: 'rms'      — sliding RMS over n-sample window
///                - 3: 'peak'     — cubic spline through local
///                                  maxima / minima with
///                                  MinPeakDistance = n
/// @param n     Window / filter length (modes 1, 2, 3); ignored for 0.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(yupper, ylower)`. For modes 0–2 the envelopes are
///              symmetric around the signal mean; mode 3 is
///              spline-interpolated and generally asymmetric.
std::pair<Value, Value>
envelope_full(const Value &                x,
              int                          mode,
              std::size_t                  n,
              std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
