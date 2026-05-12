// libs/stats/include/numkit/stats/distributions/unid.hpp
//
// Discrete uniform on {1, 2, ..., N}.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Discrete-uniform pmf (`y = unidpdf(k, N)`) — `f = 1/N` for `k ∈ {1, …, N}`.
Value unidpdf(const Value &k, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Discrete-uniform CDF (`F = unidcdf(k, N)`) — `F = ⌊k⌋ / N` (clipped to [0, 1]).
Value unidcdf(const Value &k, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Discrete-uniform inverse CDF (`k = unidinv(p, N)`) — `k = ⌈p·N⌉`.
Value unidinv(const Value &p, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Discrete-uniform random samples (`r = unidrnd(N, rows, cols)`).
Value unidrnd(double N, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Discrete-uniform mean / variance (`[m, v] = unidstat(N)`).
///
/// `m = (N+1)/2`, `v = (N² − 1)/12`.
std::tuple<double, double> unidstat(double N);

} // namespace numkit::stats
