// libs/signal/include/numkit/signal/digital_filtering/filter.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Direct Form II transposed IIR/FIR filter: y = filter(b, a, x).
/// a[0] must be non-zero (gets normalized out).
/// @throws Error if a[0] == 0.
Value filter(Allocator &alloc, const Value &b, const Value &a, const Value &x);

/// Zero-phase forward-backward filtering: applies filter() twice with
/// edge-reflected padding (length 3 * max(nb, na)), reverses each pass.
/// @throws Error if a[0] == 0.
Value filtfilt(Allocator &alloc, const Value &b, const Value &a, const Value &x);

} // namespace numkit::signal
