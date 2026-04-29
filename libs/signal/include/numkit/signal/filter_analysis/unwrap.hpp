// libs/signal/include/numkit/signal/filter_analysis/unwrap.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Unwrap radian phase by adding multiples of +/-2*pi when the jump between
/// consecutive samples exceeds pi.
Value unwrap(std::pmr::memory_resource *mr, const Value &phase);

} // namespace numkit::signal
