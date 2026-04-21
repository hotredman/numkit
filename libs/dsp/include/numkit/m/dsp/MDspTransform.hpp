// libs/dsp/include/numkit/m/dsp/MDspTransform.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::dsp {

/// Unwrap radian phase by adding multiples of +/-2*pi when the jump between
/// consecutive samples exceeds pi.
MValue unwrap(Allocator &alloc, const MValue &phase);

/// Analytic signal via FFT-based Hilbert transform. Returns complex-typed
/// MValue of same length as input.
MValue hilbert(Allocator &alloc, const MValue &x);

/// Amplitude envelope of a real signal (|hilbert(x)|). Returns real-typed
/// MValue.
MValue envelope(Allocator &alloc, const MValue &x);

} // namespace numkit::m::dsp
