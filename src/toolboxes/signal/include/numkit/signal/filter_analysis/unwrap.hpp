/// @file unwrap.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/filter_analysis/unwrap.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Unwrap a wrapped radian phase sequence.
///
/// Walks `phase` from start to end; whenever the jump between
/// consecutive samples exceeds π in magnitude, adds ∓2π to all
/// subsequent samples to restore continuity. This recovers the
/// "true" continuous phase from a sequence that has been wrapped
/// into the principal branch `(-π, π]`.
///
/// @param phase  Wrapped phase in radians (real vector or matrix).
///               For matrices: unwraps along the first non-singleton
///               dimension.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Unwrapped phase, same shape as `phase`.
///
/// @code
/// auto [H, w] = freqz(b, a);
/// Value phi   = unwrap(angle(H));   // continuous phase response
/// @endcode
Value unwrap(const Value &                phase,
             std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
