// libs/stats/include/numkit/stats/distributions/rayleigh.hpp
//
// Rayleigh distribution. Single scale parameter b > 0.
// X = sqrt(-2·b²·log(U)),  U ~ Uniform(0,1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value raylpdf(std::pmr::memory_resource *mr, const Value &x, double b);
Value raylcdf(std::pmr::memory_resource *mr, const Value &x, double b);
Value raylinv(std::pmr::memory_resource *mr, const Value &p, double b);
Value raylrnd(std::pmr::memory_resource *mr, double b,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> raylstat(double b);

} // namespace numkit::stats
