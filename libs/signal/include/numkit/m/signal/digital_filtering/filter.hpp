// libs/dsp/include/numkit/m/signal/digital_filtering/filter.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// Direct Form II transposed IIR/FIR filter: y = filter(b, a, x).
/// a[0] must be non-zero (gets normalized out).
/// @throws MError if a[0] == 0.
MValue filter(Allocator &alloc, const MValue &b, const MValue &a, const MValue &x);

/// Zero-phase forward-backward filtering: applies filter() twice with
/// edge-reflected padding (length 3 * max(nb, na)), reverses each pass.
/// @throws MError if a[0] == 0.
MValue filtfilt(Allocator &alloc, const MValue &b, const MValue &a, const MValue &x);

} // namespace numkit::m::signal
