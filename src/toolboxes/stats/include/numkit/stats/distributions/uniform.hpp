// toolboxes/stats/include/numkit/stats/distributions/uniform.hpp
//
// Continuous uniform distribution on [a, b]. Closed-form throughout.
// Defaults: a = 0, b = 1.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Continuous-uniform pdf (`y = unifpdf(x, a, b)`).
///
/// Constant `1/(b-a)` inside `[a, b]`, 0 outside. Broadcasts over `x`.
///
/// @param x   Evaluation points (any shape).
/// @param a   Lower bound of the support.
/// @param b   Upper bound of the support (`b > a`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see unifcdf, unifinv, unifrnd, unifstat
Value unifpdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Continuous-uniform cdf (`p = unifcdf(x, a, b)`).
///
/// Closed form: `0` for `x < a`, `(x-a)/(b-a)` for `x ∈ [a, b]`, `1` for `x > b`.
///
/// @param x   Evaluation points (any shape).
/// @param a   Lower bound.
/// @param b   Upper bound (`b > a`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`, same shape as `x`.
/// @see unifpdf, unifinv
Value unifcdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Continuous-uniform inverse cdf (`x = unifinv(p, a, b)`).
///
/// Closed form: `a + p·(b-a)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param a   Lower bound.
/// @param b   Upper bound (`b > a`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see unifcdf
Value unifinv(const Value &p, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Continuous-uniform random samples (`r = unifrnd(a, b, rows, cols)`).
///
/// @param a     Lower bound.
/// @param b     Upper bound (`b > a`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of uniform samples.
/// @see unifpdf
Value unifrnd(::numkit::ops::RngContext &rng, double a, double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Continuous-uniform mean and variance (`[m, v] = unifstat(a, b)`).
///
/// Closed form: `m = (a+b)/2`, `v = (b-a)²/12`.
///
/// @param a  Lower bound.
/// @param b  Upper bound.
/// @return   `{mean, variance}` pair.
/// @see unifpdf
std::tuple<double, double> unifstat(double a, double b);

} // namespace numkit::stats
