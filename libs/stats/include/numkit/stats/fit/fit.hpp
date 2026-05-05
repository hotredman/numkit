// libs/stats/include/numkit/stats/fit/fit.hpp
//
// Distribution fitters (function-form). Each returns scalar parameter
// estimate(s) plus 1×2 confidence intervals at level (1 − alpha).
// Default alpha = 0.05 (95% CI).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// `[muhat, sigmahat, muci, sigmaci] = normfit(x[, alpha])`.
/// muhat = mean(x); sigmahat = sample std (N−1). CIs: t-based for mu,
/// chi² for sigma. NaN-free vector input expected (caller should
/// pre-filter; matches MATLAB's behaviour for vector samples).
std::tuple<Value, Value, Value, Value>
normfit(std::pmr::memory_resource *mr, const Value &x, double alpha);

/// `[lhat, lci] = poissfit(x[, alpha])` — lambda = mean(x); exact CI
/// from chi² inversion of cumulative Poisson tail.
std::tuple<Value, Value>
poissfit(std::pmr::memory_resource *mr, const Value &x, double alpha);

/// `[muhat, muci] = expfit(x[, alpha])` — μ = mean(x); exact CI
/// from chi² inversion of the gamma-distributed sum.
std::tuple<Value, Value>
expfit(std::pmr::memory_resource *mr, const Value &x, double alpha);

/// `[ahat, bhat, aci, bci] = unifit(x[, alpha])` — uniform U(a,b)
/// MLE: a=min, b=max. CI based on (b-a) · (alpha^(-1/n) − 1).
std::tuple<Value, Value, Value, Value>
unifit(std::pmr::memory_resource *mr, const Value &x, double alpha);

} // namespace numkit::stats
