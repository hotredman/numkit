// libs/stats/include/numkit/stats/distributions/uniform.hpp
//
// Continuous uniform distribution on [a, b]. Closed-form throughout.
// MATLAB defaults: a = 0, b = 1.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value unifpdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);
Value unifcdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);
Value unifinv(std::pmr::memory_resource *mr, const Value &p, double a, double b);
Value unifrnd(std::pmr::memory_resource *mr, double a, double b,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> unifstat(double a, double b);

} // namespace numkit::stats
