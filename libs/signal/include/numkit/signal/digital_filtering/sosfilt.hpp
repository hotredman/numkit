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
Value sosfilt(const Value &sos, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Zero-phase forward+backward SOS cascade with Gustafsson initial
/// conditions and reflection padding (matches MATLAB filtfilt(d, x)
/// when d is a digitalFilter SOS object). Numerically stable for
/// high-order IIR designs where direct (b, a) form is ill-conditioned.
Value sosfiltfilt(const Value &sos, const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
