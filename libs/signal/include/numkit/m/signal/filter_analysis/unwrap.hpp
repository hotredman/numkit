// libs/signal/include/numkit/m/signal/filter_analysis/unwrap.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// Unwrap radian phase by adding multiples of +/-2*pi when the jump between
/// consecutive samples exceeds pi.
MValue unwrap(Allocator &alloc, const MValue &phase);

} // namespace numkit::m::signal
