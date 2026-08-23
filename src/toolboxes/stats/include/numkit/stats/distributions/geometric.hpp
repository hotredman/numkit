/// @file geometric.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/geometric.hpp
//
// Geometric distribution (number of failures before the first success).
// Convention: f(k; p) = (1-p)^k · p,   k = 0, 1, 2, ...

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Geometric pmf (`y = geopdf(k, p)`).
///
/// @f$ f(k; p) = (1-p)^{k}\,p @f$ for `k = 0, 1, 2, …`. Convention:
/// `k` counts failures BEFORE the first success (Pascal's). Returns 0
/// outside support.
///
/// @param k   Evaluation points (any shape).
/// @param p   Success probability per trial, `0 < p <= 1`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pmf values, same shape as `k`.
/// @see geocdf, geoinv, geornd, geostat
Value geopdf(const Value &k, double p,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Geometric cdf (`F = geocdf(k, p)`).
///
/// Closed form: @f$ F(k) = 1 - (1-p)^{\lfloor k\rfloor + 1} @f$ for `k >= 0`.
///
/// @param k   Evaluation points (any shape).
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see geopdf, geoinv
Value geocdf(const Value &k, double p,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Geometric inverse cdf (`k = geoinv(q, p)`).
///
/// Smallest non-negative integer `k` such that `geocdf(k, p) >= q`.
///
/// @param q   Probability levels in `[0, 1]` (any shape).
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `q`.
/// @see geocdf
Value geoinv(const Value &q, double p,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Geometric random samples (`r = geornd(p, rows, cols)`).
///
/// @param p     Success probability per trial.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of geometric samples (failure counts).
/// @see geopdf
Value geornd(::numkit::ops::RngContext &rng, double p, size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Geometric mean and variance (`[m, v] = geostat(p)`).
///
/// Closed form: `m = (1-p)/p`, `v = (1-p)/p²`.
///
/// @param p  Success probability per trial.
/// @return   `{mean, variance}` pair.
/// @see geopdf
std::tuple<double, double> geostat(double p);


/// @}
} // namespace numkit::stats
