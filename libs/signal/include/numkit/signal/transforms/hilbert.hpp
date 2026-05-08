// libs/signal/include/numkit/signal/transforms/hilbert.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Analytic signal via FFT-based Hilbert transform. Returns complex-typed
/// Value of same length as input.
Value hilbert(std::pmr::memory_resource *mr, const Value &x);

/// Amplitude envelope of a real signal (|hilbert(x)|). Returns real-typed
/// Value.
Value envelope(std::pmr::memory_resource *mr, const Value &x);

/// 2-output envelope (back-compat): dispatches to envelope_full with
/// default mode. Returns yupper = mean + |hilbert(x-mean)|, ylower =
/// mean - |hilbert(x-mean)| (symmetric around the signal mean).
void envelope_pair(std::pmr::memory_resource *mr, const Value &x,
                   Value *yupper, Value *ylower);

/// Full multi-mode envelope (matches MATLAB R2025b `envelope.m`):
///   mode=0 default — FFT |hilbert(x-mean)|
///   mode=1 'analytic' — n-tap Kaiser-tapered Hilbert FIR
///   mode=2 'rms' — sliding RMS over n-sample window
///   mode=3 'peak' — spline through local maxima/minima with
///                   MinPeakDistance n (no DC removal)
/// For modes 0/1/2: yupper = mean + amplitude, ylower = mean - amplitude.
/// For mode 3: spline-interpolated upper / lower envelopes (asymmetric).
void envelope_full(std::pmr::memory_resource *mr, const Value &x,
                   int mode, std::size_t n,
                   Value *yupper, Value *ylower);

} // namespace numkit::signal
