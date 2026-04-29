// libs/signal/include/numkit/signal/digital_filtering/sosfilt.hpp
//
// Apply an SOS biquad cascade. Conversions zp2sos / tf2sos live in
// filter_implementation/conversions.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Apply an SOS cascade. `sos` is L×6, `x` is a vector or 2D matrix
/// (columns are filtered independently). Returns y of the same shape
/// as x.
Value sosfilt(std::pmr::memory_resource *mr, const Value &sos, const Value &x);

} // namespace numkit::signal
