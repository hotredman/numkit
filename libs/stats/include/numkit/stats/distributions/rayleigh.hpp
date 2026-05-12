// libs/stats/include/numkit/stats/distributions/rayleigh.hpp
//
// Rayleigh distribution. Single scale parameter b > 0.
// X = sqrt(-2·b²·log(U)),  U ~ Uniform(0,1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Rayleigh density (`y = raylpdf(x, b)`).
///
/// @f$ f(x; b) = (x/b^2)\,e^{-x^2/(2 b^2)} @f$ for x ≥ 0, `b > 0`.
Value raylpdf(const Value &x, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Rayleigh CDF (`p = raylcdf(x, b)`) — @f$ F = 1 - e^{-x^2/(2 b^2)} @f$.
Value raylcdf(const Value &x, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Rayleigh inverse CDF (`x = raylinv(p, b)`).
Value raylinv(const Value &p, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Rayleigh random samples (`r = raylrnd(b, rows, cols)`).
///
/// `X = sqrt(-2·b²·log(U))` with `U ~ Uniform(0, 1)`.
Value raylrnd(double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Rayleigh mean / variance (`[m, v] = raylstat(b)`).
///
/// `m = b·sqrt(π/2)`, `v = b²·(2 − π/2)`.
std::tuple<double, double> raylstat(double b);

} // namespace numkit::stats
