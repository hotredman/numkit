// libs/signal/include/numkit/signal/filter_analysis/unwrap.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Unwrap radian phase by adding multiples of +/-2*pi when the jump between
/// consecutive samples exceeds pi.
Value unwrap(const Value &phase, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
