// libs/stats/include/numkit/stats/distributions/geometric.hpp
//
// Geometric distribution (number of failures before the first success).
// MATLAB convention: f(k; p) = (1-p)^k · p,   k = 0, 1, 2, ...

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value geopdf(const Value &k, double p, std::pmr::memory_resource *mr = nullptr);
Value geocdf(const Value &k, double p, std::pmr::memory_resource *mr = nullptr);
Value geoinv(const Value &q, double p, std::pmr::memory_resource *mr = nullptr);
Value geornd(double p, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> geostat(double p);

} // namespace numkit::stats
