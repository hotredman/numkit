// libs/stats/include/numkit/stats/distributions/uniform.hpp
//
// Continuous uniform distribution on [a, b]. Closed-form throughout.
// MATLAB defaults: a = 0, b = 1.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value unifpdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value unifcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value unifinv(const Value &p, double a, double b, std::pmr::memory_resource *mr = nullptr);
Value unifrnd(double a, double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> unifstat(double a, double b);

} // namespace numkit::stats
