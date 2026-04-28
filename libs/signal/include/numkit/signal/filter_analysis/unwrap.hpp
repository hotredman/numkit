// libs/signal/include/numkit/signal/filter_analysis/unwrap.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Unwrap radian phase by adding multiples of +/-2*pi when the jump between
/// consecutive samples exceeds pi.
Value unwrap(Allocator &alloc, const Value &phase);

} // namespace numkit::signal
