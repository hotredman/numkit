// libs/signal/include/numkit/signal/digital_filtering/sosfilt.hpp
//
// Apply an SOS biquad cascade. Conversions zp2sos / tf2sos live in
// filter_implementation/conversions.hpp.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Apply an SOS cascade. `sos` is L×6, `x` is a vector or 2D matrix
/// (columns are filtered independently). Returns y of the same shape
/// as x.
Value sosfilt(Allocator &alloc, const Value &sos, const Value &x);

} // namespace numkit::signal
