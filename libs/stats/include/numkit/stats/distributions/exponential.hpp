// libs/stats/include/numkit/stats/distributions/exponential.hpp
//
// Exponential distribution. Parameterized by the MEAN μ
// (NOT the rate λ): f(x; μ) = exp(-x/μ) / μ. Special case Gamma(1, μ).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Exponential pdf (`y = exppdf(x, mu)`).
///
/// @f$ f(x; \mu) = e^{-x/\mu} / \mu @f$ for `x >= 0`, 0 elsewhere.
/// Parameterised by the mean `mu`, NOT the rate (use `lambda = 1/mu`).
///
/// @param x   Evaluation points (any shape).
/// @param mu  Mean of the distribution (`mu > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see expcdf, expinv, exprnd, expstat
Value exppdf(const Value &x, double mu, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential cdf (`F = expcdf(x, mu)`).
///
/// Closed form: `F(x) = 1 - exp(-x/mu)` for `x >= 0`.
///
/// @param x   Evaluation points (any shape).
/// @param mu  Mean of the distribution.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see exppdf, expinv
Value expcdf(const Value &x, double mu, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential inverse cdf (`x = expinv(p, mu)`).
///
/// Closed form: `x = -mu · log(1 - p)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param mu  Mean of the distribution.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see expcdf
Value expinv(const Value &p, double mu, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential random samples (`r = exprnd(mu, rows, cols)`).
///
/// Uses `std::exponential_distribution` with rate `1/mu`.
///
/// @param mu    Mean of the distribution.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of exponential samples.
/// @see exppdf
Value exprnd(double mu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential mean and variance (`[m, v] = expstat(mu)`).
///
/// Closed form: `m = mu`, `v = mu²`.
///
/// @param mu  Mean parameter.
/// @return    `{mean, variance}` pair.
/// @see exppdf
std::tuple<double, double> expstat(double mu);

} // namespace numkit::stats
