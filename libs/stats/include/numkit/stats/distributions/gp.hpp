// libs/stats/include/numkit/stats/distributions/gp.hpp
//
// Generalized Pareto distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Generalised Pareto pdf (`y = gppdf(x, k, sigma, theta)`).
///
/// Three sub-families depending on the shape `k`:
/// - `k = 0` → exponential with mean `sigma`, threshold `theta`
/// - `k > 0` → heavy-tailed, support `x >= theta`
/// - `k < 0` → bounded support `x ∈ [theta, theta - sigma/k]`
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter (any real).
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param theta  Threshold / location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see gpcdf, gpinv, gprnd, gpstat
Value gppdf(const Value &x, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Pareto cdf (`p = gpcdf(x, k, sigma, theta)`).
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param theta  Threshold parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see gppdf, gpinv
Value gpcdf(const Value &x, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Pareto inverse cdf (`x = gpinv(p, k, sigma, theta)`).
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param theta  Threshold parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see gpcdf
Value gpinv(const Value &p, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Pareto random samples (`r = gprnd(k, sigma, theta, rows, cols)`).
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param theta  Threshold parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of GP samples.
/// @see gppdf
Value gprnd(double k, double sigma, double theta,
            size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Pareto mean and variance (`[m, v] = gpstat(k, sigma, theta)`).
///
/// Mean defined only for `k < 1`; variance defined only for `k < 1/2`.
/// Returns `NaN` outside those ranges.
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param theta  Threshold parameter.
/// @return       `{mean, variance}` pair.
/// @see gppdf
std::tuple<double, double>
gpstat(double k, double sigma, double theta);

/// @brief Generalised Pareto MLE fit (`[khat, sigmahat] = gpfit(x)`).
///
/// Uses the probability-weighted-moments estimator
/// (Hosking & Wallis 1987): for sorted `x` and `F̂_i = (i - 0.35)/n`,
/// `β_0 = mean(x)`, `β_1 = mean(F̂_i · x_(i))`,
/// `k̂ = 2 - β_0/(β_0 - 2β_1)`, `σ̂ = 2·β_0·β_1/(β_0 - 2β_1)`.
///
/// PWM matches MLE asymptotically; for typical sample sizes the gap to
/// MATLAB's Grimshaw MLE is a few percent. Threshold `θ` is assumed 0
/// (the MATLAB convention for `gpfit`).
///
/// KNOWN GAPs: CI second output, `alpha` argument, and the `options`
/// struct are deferred; full Grimshaw 1993 MLE refinement deferred.
///
/// @param x   Observations (must be ≥ 0; threshold θ = 0).
/// @param mr  Memory resource.
/// @return    `[khat, sigmahat]` as a `1 × 2` row.
/// @see gppdf, gplike
Value gpfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
