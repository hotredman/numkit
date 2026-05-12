// libs/stats/include/numkit/stats/distributions/hypergeom.hpp
//
// Hypergeometric distribution. MATLAB convention: parameters (M, K, N) where
//   M = population size, K = #successes in population, N = sample size.
// f(k; M, K, N) = C(K, k) · C(M-K, N-k) / C(M, N).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value hygepdf(const Value &k, double M, double K, double N, std::pmr::memory_resource *mr = nullptr);
Value hygecdf(const Value &k, double M, double K, double N, std::pmr::memory_resource *mr = nullptr);
Value hygeinv(const Value &q, double M, double K, double N, std::pmr::memory_resource *mr = nullptr);
Value hygernd(double M, double K, double N, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> hygestat(double M, double K, double N);

} // namespace numkit::stats
