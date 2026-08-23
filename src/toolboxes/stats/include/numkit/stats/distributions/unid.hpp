/// @file unid.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/unid.hpp
//
// Discrete uniform on {1, 2, ..., N}.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Discrete-uniform pmf (`y = unidpdf(k, N)`).
///
/// `f = 1/N` for `k ∈ {1, 2, …, N}`, 0 elsewhere.
///
/// @param k   Evaluation points (any shape).
/// @param N   Upper bound of support (positive integer-valued double).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pmf values, same shape as `k`.
/// @see unidcdf, unidinv, unidrnd, unidstat
Value unidpdf(const Value &k, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete-uniform cdf (`F = unidcdf(k, N)`).
///
/// Closed form: `F(k) = floor(k) / N` clipped to `[0, 1]`.
///
/// @param k   Evaluation points (any shape).
/// @param N   Upper bound of support.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see unidpdf, unidinv
Value unidcdf(const Value &k, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete-uniform inverse cdf (`k = unidinv(p, N)`).
///
/// Closed form: `k = ceil(p · N)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param N   Upper bound of support.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see unidcdf
Value unidinv(const Value &p, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete-uniform random samples (`r = unidrnd(N, rows, cols)`).
///
/// @param N     Upper bound of support.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of discrete-uniform samples.
/// @see unidpdf
Value unidrnd(::numkit::ops::RngContext &rng, double N, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete-uniform mean and variance (`[m, v] = unidstat(N)`).
///
/// Closed form: `m = (N+1)/2`, `v = (N² - 1)/12`.
///
/// @param N  Upper bound of support.
/// @return   `{mean, variance}` pair.
/// @see unidpdf
std::tuple<double, double> unidstat(double N);


/// @}
} // namespace numkit::stats
