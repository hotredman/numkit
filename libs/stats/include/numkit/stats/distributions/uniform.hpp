// libs/stats/include/numkit/stats/distributions/uniform.hpp
//
// Continuous uniform distribution on [a, b]. Closed-form throughout.
// MATLAB defaults: a = 0, b = 1.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Continuous-uniform density (`y = unifpdf(x, a, b)`) — `1/(b-a)` on [a, b].
Value unifpdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Continuous-uniform CDF (`p = unifcdf(x, a, b)`).
Value unifcdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Continuous-uniform inverse CDF (`x = unifinv(p, a, b)`) — `a + p·(b−a)`.
Value unifinv(const Value &p, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Continuous-uniform random samples (`r = unifrnd(a, b, rows, cols)`).
Value unifrnd(double a, double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Continuous-uniform mean / variance (`[m, v] = unifstat(a, b)`).
///
/// `m = (a+b)/2`, `v = (b-a)²/12`.
std::tuple<double, double> unifstat(double a, double b);

} // namespace numkit::stats
