// libs/signal/include/numkit/signal/transforms/hilbert.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Analytic signal via FFT-based Hilbert transform. Returns complex-typed
/// Value of same length as input.
Value hilbert(Allocator &alloc, const Value &x);

/// Amplitude envelope of a real signal (|hilbert(x)|). Returns real-typed
/// Value.
Value envelope(Allocator &alloc, const Value &x);

} // namespace numkit::signal
