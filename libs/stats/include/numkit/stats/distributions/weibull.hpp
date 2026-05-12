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

Value wblpdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value wblcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value wblinv(const Value &p, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value wblrnd(double a, double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> wblstat(double a, double b);

} // namespace numkit::stats
