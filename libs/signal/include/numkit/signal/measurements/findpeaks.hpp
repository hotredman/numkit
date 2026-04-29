// libs/signal/include/numkit/signal/measurements/findpeaks.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// findpeaks(x) — strict local maxima (x[i-1] < x[i] > x[i+1]).
/// Returns (peakValues, peakIndices_1based) as row vectors.
std::tuple<Value, Value>
findpeaks(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::signal
