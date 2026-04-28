// libs/signal/include/numkit/m/signal/transforms/hilbert.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// Analytic signal via FFT-based Hilbert transform. Returns complex-typed
/// MValue of same length as input.
MValue hilbert(Allocator &alloc, const MValue &x);

/// Amplitude envelope of a real signal (|hilbert(x)|). Returns real-typed
/// MValue.
MValue envelope(Allocator &alloc, const MValue &x);

} // namespace numkit::m::signal
