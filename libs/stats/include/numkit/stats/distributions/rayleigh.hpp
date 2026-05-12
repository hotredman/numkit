// libs/stats/include/numkit/stats/distributions/rayleigh.hpp
//
// Rayleigh distribution. Single scale parameter b > 0.
// X = sqrt(-2·b²·log(U)),  U ~ Uniform(0,1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value raylpdf(const Value &x, double b, std::pmr::memory_resource *mr = nullptr);
Value raylcdf(const Value &x, double b, std::pmr::memory_resource *mr = nullptr);
Value raylinv(const Value &p, double b, std::pmr::memory_resource *mr = nullptr);
Value raylrnd(double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> raylstat(double b);

} // namespace numkit::stats
