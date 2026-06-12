// toolboxes/stats/include/numkit/stats/distributions/rayleigh.hpp
//
// Rayleigh distribution. Single scale parameter b > 0.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Rayleigh pdf (`y = raylpdf(x, b)`).
///
/// @f$ f(x; b) = (x/b^2)\,e^{-x^2/(2b^2)} @f$ for `x >= 0`.
///
/// @param x   Evaluation points (any shape).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see raylcdf, raylinv, raylrnd, raylstat
Value raylpdf(const Value &x, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rayleigh cdf (`p = raylcdf(x, b)`).
///
/// Closed form: @f$ F(x) = 1 - e^{-x^2/(2b^2)} @f$ for `x >= 0`.
///
/// @param x   Evaluation points (any shape).
/// @param b   Scale parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see raylpdf, raylinv
Value raylcdf(const Value &x, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rayleigh inverse cdf (`x = raylinv(p, b)`).
///
/// Closed form: `x = b · sqrt(-2 · log(1 - p))`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param b   Scale parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see raylcdf
Value raylinv(const Value &p, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rayleigh random samples (`r = raylrnd(b, rows, cols)`).
///
/// Sampled as `X = sqrt(-2·b²·log(U))` with `U ~ Uniform(0, 1)`.
///
/// @param b     Scale parameter.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of Rayleigh samples.
/// @see raylpdf
Value raylrnd(::numkit::ops::RngContext &rng, double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rayleigh mean and variance (`[m, v] = raylstat(b)`).
///
/// Closed form: `m = b · sqrt(π/2)`, `v = b² · (2 - π/2)`.
///
/// @param b  Scale parameter.
/// @return   `{mean, variance}` pair.
/// @see raylpdf
std::tuple<double, double> raylstat(double b);

} // namespace numkit::stats
