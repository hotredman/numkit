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
Value rms(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// rssq(x[, dim]) — sqrt(sum(x.^2)) along dim. Root-sum-of-squares.
Value rssq(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// peak2peak(x[, dim]) — max(x) - min(x) along dim. NaN propagates.
Value peak2peak(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// peak2rms(x[, dim]) — max(|x|) / rms(x) along dim.
Value peak2rms(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
