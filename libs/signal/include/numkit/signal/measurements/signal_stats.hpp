// libs/signal/include/numkit/signal/measurements/signal_stats.hpp
//
// Signal-side reductions: rms, rssq, peak2peak, peak2rms.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// Root-mean-square along a dimension.
///
/// Computes \f$ y = \sqrt{\frac{1}{N}\sum x^2} \f$ along the chosen axis.
///
/// @param x    Real input array.
/// @param dim  Reduction axis (1-based). `0` (default) →
///             first non-singleton dimension.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced array with `dim` collapsed to length 1.
///
/// @see rssq, peak2rms
Value rms(const Value &                x,
          int                          dim = 0,
          std::pmr::memory_resource *  mr  = nullptr);

/// Root-sum-of-squares along a dimension.
///
/// Computes \f$ y = \sqrt{\sum x^2} \f$ along `dim`.
///
/// @param x    Real input array.
/// @param dim  Reduction axis. `0` → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced array.
///
/// @see rms
Value rssq(const Value &                x,
           int                          dim = 0,
           std::pmr::memory_resource *  mr  = nullptr);

/// Peak-to-peak amplitude along a dimension.
///
/// Computes `max(x) - min(x)` along `dim`. NaN propagates (any NaN in
/// the slice yields NaN).
///
/// @param x    Real input array.
/// @param dim  Reduction axis. `0` → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced array.
Value peak2peak(const Value &                x,
                int                          dim = 0,
                std::pmr::memory_resource *  mr  = nullptr);

/// Peak-to-RMS ratio along a dimension.
///
/// Computes \f$ y = \max|x| / \mathrm{rms}(x) \f$ along `dim`. Indicates
/// the crest factor of a signal.
///
/// @param x    Real input array.
/// @param dim  Reduction axis. `0` → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced array.
///
/// @see rms, peak2peak
Value peak2rms(const Value &                x,
               int                          dim = 0,
               std::pmr::memory_resource *  mr  = nullptr);

} // namespace numkit::signal
