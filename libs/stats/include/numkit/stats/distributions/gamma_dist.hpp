// libs/stats/include/numkit/stats/distributions/gamma_dist.hpp
//
// Gamma distribution Gamma(a, b): a = shape, b = scale.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Gamma pdf (`y = gampdf(x, a, b)`).
///
/// @f$ f(x; a, b) = \dfrac{x^{a-1}\,e^{-x/b}}{b^{a}\,\Gamma(a)} @f$ for `x > 0`.
/// Convention: `a` is the shape, `b` is the scale (NOT rate).
///
/// @param x   Evaluation points (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see gamcdf, gaminv, gamrnd, gamstat
Value gampdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma cdf (`F = gamcdf(x, a, b)`).
///
/// Computed via the regularised lower-incomplete gamma:
/// @f$ F(x) = P(a,\ x/b) = \text{gammainc}(x/b,\ a) @f$.
///
/// @param x   Evaluation points (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see gampdf, gaminv
Value gamcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma inverse cdf (`x = gaminv(p, a, b)`).
///
/// Computed via `gammaincinv`: `x = b · P^{-1}(p; a)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see gamcdf
Value gaminv(const Value &p, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma random samples (`r = gamrnd(a, b, rows, cols)`).
///
/// Uses `std::gamma_distribution` with shape `a`, scale `b`.
///
/// @param a     Shape parameter (`a > 0`).
/// @param b     Scale parameter (`b > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of Gamma samples.
/// @see gampdf
Value gamrnd(double a, double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma mean and variance (`[m, v] = gamstat(a, b)`).
///
/// Closed form: `m = a·b`, `v = a·b²`.
///
/// @param a  Shape parameter.
/// @param b  Scale parameter.
/// @return   `{mean, variance}` pair.
/// @see gampdf
std::tuple<double, double> gamstat(double a, double b);

/// @brief Raw gamma(shape, 1) RNG (`r = randg(shape [, rows, cols])`).
///
/// MATLAB's undocumented-but-widely-used "raw" gamma sampler. Equivalent
/// to `gamrnd(shape, 1.0, rows, cols)`; uses the shared MT19937 stream
/// so `rng(seed)` reproduces the draw sequence.
///
/// @param shape  Shape parameter (`shape > 0`); array-valued shape is
///               supported via the adapter-only entry — see `randg_reg`.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of Gamma(shape, 1) samples.
/// @see gamrnd
Value randg(double shape, size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element gamma RNG (`r = randg(shapeArray)`).
///
/// Draws one sample per entry of `shapeArray`, with shape = element value.
/// Negative / zero shape entries produce NaN in the output (MATLAB
/// convention).
///
/// @param shapeArray  Array-valued shape parameter.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Array same size as `shapeArray`, one Gamma(shape, 1)
///                    draw per entry.
/// @see randg
Value randg(const Value &shapeArray,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
