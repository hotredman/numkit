// libs/signal/include/numkit/m/signal/digital_filtering/sosfilt.hpp
//
// Apply an SOS biquad cascade. Conversions zp2sos / tf2sos live in
// filter_implementation/conversions.hpp.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// Apply an SOS cascade. `sos` is L×6, `x` is a vector or 2D matrix
/// (columns are filtered independently). Returns y of the same shape
/// as x.
MValue sosfilt(Allocator &alloc, const MValue &sos, const MValue &x);

} // namespace numkit::m::signal
