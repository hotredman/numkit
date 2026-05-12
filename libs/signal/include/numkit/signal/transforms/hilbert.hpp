// libs/signal/include/numkit/signal/transforms/hilbert.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Analytic signal via FFT-based Hilbert transform. Returns complex-typed
/// Value of same length as input.
Value hilbert(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Amplitude envelope of a real signal (|hilbert(x)|). Returns real-typed
/// Value.
Value envelope(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// 2-output envelope (back-compat): dispatches to envelope_full with
/// default mode. Returns yupper = mean + |hilbert(x-mean)|, ylower =
/// mean - |hilbert(x-mean)| (symmetric around the signal mean).
void envelope_pair(const Value &x, Value *yupper, Value *ylower, std::pmr::memory_resource *mr = nullptr);

/// Full multi-mode envelope (matches MATLAB R2025b `envelope.m`):
///   mode=0 default — FFT |hilbert(x-mean)|
///   mode=1 'analytic' — n-tap Kaiser-tapered Hilbert FIR
///   mode=2 'rms' — sliding RMS over n-sample window
///   mode=3 'peak' — spline through local maxima/minima with
///                   MinPeakDistance n (no DC removal)
/// For modes 0/1/2: yupper = mean + amplitude, ylower = mean - amplitude.
/// For mode 3: spline-interpolated upper / lower envelopes (asymmetric).
void envelope_full(const Value &x, int mode, std::size_t n, Value *yupper, Value *ylower, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
