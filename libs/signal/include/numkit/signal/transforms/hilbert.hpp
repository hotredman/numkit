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

/// 2-output envelope: yupper = |analytic|, ylower = -yupper. Symmetric;
/// MATLAB's default `envelope(x)` uses spline-peak interpolation
/// (asymmetric output) — that mode is deferred. See
/// audit/closed/signal/envelope.md.
void envelope_pair(std::pmr::memory_resource *mr, const Value &x,
                   Value *yupper, Value *ylower);

} // namespace numkit::signal
