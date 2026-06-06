// libs/signal/include/numkit/signal/measurements/findpeaks.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Locate strict local maxima in a signal.
///
/// A sample at position i is a peak iff `x[i-1] < x[i] > x[i+1]`. The
/// endpoints (i = 0 or last) are never returned as peaks.
///
/// @param x   Real 1-D signal.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(peakValues, peakIndices)`. Indices are 1-based;
///            both outputs are row vectors of the same
///            length, empty when no peaks exist.
///
/// @note Plateaus (`x[i] == x[i+1]`) do not count as peaks under the
///       strict-inequality rule.
///
/// @code
/// auto [pks, locs] = findpeaks({1.0, 3.0, 1.0, 5.0, 2.0});
/// // pks  = {3.0, 5.0}; locs = {2, 4}
/// @endcode
std::tuple<Value, Value>
findpeaks(const Value &                x,
          std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
