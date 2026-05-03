// libs/stats/include/numkit/stats/distributions/lognormal.hpp
//
// Lognormal distribution: log(X) ~ N(μ, σ²). MATLAB convention parameterizes
// by μ, σ of the underlying normal (NOT by the mean / std of X itself).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value lognpdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma);
Value logncdf(std::pmr::memory_resource *mr, const Value &x, double mu, double sigma);
Value logninv(std::pmr::memory_resource *mr, const Value &p, double mu, double sigma);
Value lognrnd(std::pmr::memory_resource *mr, double mu, double sigma,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> lognstat(double mu, double sigma);

} // namespace numkit::stats
