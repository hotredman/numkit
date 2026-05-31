// libs/stats/include/numkit/stats/copulas/copulas.hpp
//
// Copula density and CDF for the five MATLAB families:
//   Gaussian, t, Clayton, Frank, Gumbel.
//
// Inputs:
//   U  — n × d matrix of pseudo-uniform observations, entries in (0, 1)
//   R  — d × d correlation matrix (Gaussian, t)
//   α  — scalar association parameter (Clayton, Frank, Gumbel)
//   ν  — degrees of freedom (t)
//
// References:
//   • Nelsen, "An Introduction to Copulas", 2006.
//   • Joe, "Multivariate Models and Dependence Concepts", 1997.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// @brief Gaussian copula density (`y = copulapdf('Gaussian', U, R)`).
///
/// `c(u; R) = (det R)^{-1/2} · exp(−½·z'·(R^{-1} − I)·z)` where
/// `z_i = Φ^{-1}(u_i)`. Currently implemented for `d = 2`.
Value copulapdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr = nullptr);

/// @brief Gaussian copula CDF (`p = copulacdf('Gaussian', U, R)`).
///
/// `C(u; R) = Φ_d(Φ^{-1}(u_1), …, Φ^{-1}(u_d); 0, R)`.
Value copulacdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr = nullptr);

/// @brief Student-t copula density (`y = copulapdf('t', U, R, nu)`).
Value copulapdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Student-t copula CDF (`p = copulacdf('t', U, R, nu)`).
Value copulacdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Clayton copula density (`y = copulapdf('Clayton', U, alpha)`).
///
/// `c(u, v; α) = (1 + α) (uv)^{−(1+α)} (u^{−α} + v^{−α} − 1)^{−2 − 1/α}`
/// for `α > 0` (independence at `α → 0+`).
Value copulapdf_clayton(const Value &U, double alpha,
                        std::pmr::memory_resource *mr = nullptr);

/// @brief Clayton copula CDF (`C(u, v; α) = (u^{−α} + v^{−α} − 1)^{−1/α}`).
Value copulacdf_clayton(const Value &U, double alpha,
                        std::pmr::memory_resource *mr = nullptr);

/// @brief Frank copula density (`y = copulapdf('Frank', U, alpha)`).
Value copulapdf_frank(const Value &U, double alpha,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Frank copula CDF.
///
/// `C(u, v; α) = −(1/α) log[1 + (e^{−αu} − 1)(e^{−αv} − 1)/(e^{−α} − 1)]`
/// for `α ≠ 0`.
Value copulacdf_frank(const Value &U, double alpha,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Gumbel copula density (`y = copulapdf('Gumbel', U, alpha)`).
Value copulapdf_gumbel(const Value &U, double alpha,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Gumbel copula CDF.
///
/// `C(u, v; α) = exp[−{(−log u)^α + (−log v)^α}^{1/α}]` for `α ≥ 1`.
Value copulacdf_gumbel(const Value &U, double alpha,
                       std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
