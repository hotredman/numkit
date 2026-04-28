// libs/signal/include/numkit/m/signal/measurements/findpeaks.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

/// findpeaks(x) — strict local maxima (x[i-1] < x[i] > x[i+1]).
/// Returns (peakValues, peakIndices_1based) as row vectors.
std::tuple<MValue, MValue>
findpeaks(Allocator &alloc, const MValue &x);

} // namespace numkit::m::signal
