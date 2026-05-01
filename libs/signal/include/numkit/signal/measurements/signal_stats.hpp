// libs/signal/include/numkit/signal/measurements/signal_stats.hpp
//
// Signal-side reductions: rms, rssq, peak2peak, peak2rms.
// All accept an optional `dim` argument (1-based, MATLAB convention).
// dim == 0 means "first non-singleton dim".

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// rms(x[, dim]) — sqrt(mean(x.^2)) along dim.
Value rms(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

/// rssq(x[, dim]) — sqrt(sum(x.^2)) along dim. Root-sum-of-squares.
Value rssq(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

/// peak2peak(x[, dim]) — max(x) - min(x) along dim. NaN propagates.
Value peak2peak(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

/// peak2rms(x[, dim]) — max(|x|) / rms(x) along dim.
Value peak2rms(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

} // namespace numkit::signal
