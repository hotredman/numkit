// libs/stats/include/numkit/stats/distributions/weibull.hpp
//
// Weibull distribution. MATLAB convention: a = scale, b = shape, so
// f(x) = (b/a)·(x/a)^(b-1)·exp(-(x/a)^b). Note std::weibull_distribution
// takes (shape, scale) — opposite order.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value wblpdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);
Value wblcdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);
Value wblinv(std::pmr::memory_resource *mr, const Value &p, double a, double b);
Value wblrnd(std::pmr::memory_resource *mr, double a, double b,
             size_t rows = 1, size_t cols = 1);
std::tuple<double, double> wblstat(double a, double b);

} // namespace numkit::stats
