// libs/stats/include/numkit/stats/distributions/geometric.hpp
//
// Geometric distribution (number of failures before the first success).
// MATLAB convention: f(k; p) = (1-p)^k · p,   k = 0, 1, 2, ...

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value geopdf(std::pmr::memory_resource *mr, const Value &k, double p);
Value geocdf(std::pmr::memory_resource *mr, const Value &k, double p);
Value geoinv(std::pmr::memory_resource *mr, const Value &q, double p);
Value geornd(std::pmr::memory_resource *mr, double p,
             size_t rows = 1, size_t cols = 1);
std::tuple<double, double> geostat(double p);

} // namespace numkit::stats
