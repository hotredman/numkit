// libs/stats/include/numkit/stats/distributions/lognormal.hpp
//
// Lognormal distribution: log(X) ~ N(μ, σ²). MATLAB convention parameterizes
// by μ, σ of the underlying normal (NOT by the mean / std of X itself).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value lognpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value logncdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value logninv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value lognrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> lognstat(double mu, double sigma);

} // namespace numkit::stats
