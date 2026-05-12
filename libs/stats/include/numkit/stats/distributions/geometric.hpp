// libs/stats/include/numkit/stats/distributions/geometric.hpp
//
// Geometric distribution (number of failures before the first success).
// MATLAB convention: f(k; p) = (1-p)^k · p,   k = 0, 1, 2, ...

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Geometric pmf (`y = geopdf(k, p)`) — @f$ f(k; p) = (1-p)^k\,p @f$.
Value geopdf(const Value &k, double p,
             std::pmr::memory_resource *mr = nullptr);

/// Geometric CDF (`F = geocdf(k, p)`) — @f$ F(k) = 1 - (1-p)^{k+1} @f$.
Value geocdf(const Value &k, double p,
             std::pmr::memory_resource *mr = nullptr);

/// Geometric inverse CDF (`k = geoinv(q, p)`).
Value geoinv(const Value &q, double p,
             std::pmr::memory_resource *mr = nullptr);

/// Geometric random samples (`r = geornd(p, rows, cols)`).
Value geornd(double p, size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// Geometric mean / variance (`[m, v] = geostat(p)`).
///
/// `m = (1-p)/p`, `v = (1-p)/p²`.
std::tuple<double, double> geostat(double p);

} // namespace numkit::stats
