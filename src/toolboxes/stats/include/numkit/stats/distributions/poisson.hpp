// toolboxes/stats/include/numkit/stats/distributions/poisson.hpp
//
// Poisson distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Poisson pmf (`y = poisspdf(k, lambda)`).
///
/// @f$ f(k; \lambda) = \lambda^{k}\,e^{-\lambda} / k! @f$ for `k = 0, 1, 2, …`.
/// Returns 0 for negative or non-integer `k`. Broadcasts elementwise over `k`.
///
/// @param k       Evaluation points (any shape).
/// @param lambda  Rate parameter, `lambda >= 0`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Array of pmf values, same shape as `k`.
/// @see poisscdf, poissinv, poissrnd, poisstat
Value poisspdf(const Value &k, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Poisson cumulative distribution (`F = poisscdf(k, lambda)`).
///
/// Computed via the upper regularised gamma identity
/// @f$ F(k) = Q(\lfloor k\rfloor + 1,\ \lambda)
///          = 1 - \text{gammainc}(\lambda, \lfloor k\rfloor + 1) @f$.
/// Numerically stable for the full `k`-range.
///
/// @param k       Evaluation points (any shape).
/// @param lambda  Rate parameter, `lambda >= 0`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Array of cdf values in `[0, 1]`, same shape as `k`.
/// @see poisspdf, poissinv
Value poisscdf(const Value &k, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Poisson inverse cdf (`k = poissinv(p, lambda)`).
///
/// Smallest integer `k` such that `poisscdf(k, lambda) >= p`.
///
/// @param p       Probability levels in `[0, 1]` (any shape).
/// @param lambda  Rate parameter, `lambda >= 0`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Quantile array, same shape as `p`.
/// @see poisscdf
Value poissinv(const Value &p, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Poisson random samples (`r = poissrnd(lambda, rows, cols)`).
///
/// Uses `std::poisson_distribution` on the shared RNG.
///
/// @param lambda  Rate parameter, `lambda >= 0`.
/// @param rows    Output rows (default 1).
/// @param cols    Output columns (default 1).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `rows × cols` matrix of Poisson samples.
/// @see poisspdf
Value poissrnd(::numkit::ops::RngContext &rng, double lambda, size_t rows = 1, size_t cols = 1,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Poisson mean and variance (`[m, v] = poisstat(lambda)`).
///
/// Closed form: `m = lambda`, `v = lambda`.
///
/// @param lambda  Rate parameter.
/// @return        `{mean, variance}` pair.
/// @see poisspdf
std::tuple<double, double> poisstat(double lambda);

} // namespace numkit::stats
