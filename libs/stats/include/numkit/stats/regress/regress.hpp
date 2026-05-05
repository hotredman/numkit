// libs/stats/include/numkit/stats/regress/regress.hpp
//
// Linear regression — function form. Mirrors MATLAB's
// [b, bint, r, rint, stats] = regress(y, X[, alpha]).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Returns (b, bint, r, stats) where:
///   b      — coefficient column vector (p×1)
///   bint   — confidence intervals for b (p×2)
///   r      — residuals (N×1)
///   stats  — 1×4 row [R², F, p, sigma²]
/// The MATLAB `rint` (outlier-detection intervals on residuals) is
/// currently not provided.
std::tuple<Value, Value, Value, Value>
regress(std::pmr::memory_resource *mr, const Value &y, const Value &X,
        double alpha);

/// `B = ridge(y, X, k[, scaled])` — ridge regression. `k` may be a
/// scalar or vector of regularisation parameters; output has one
/// column per k. `scaled` (default 1) returns coefficients in the
/// standardised feature space (centred + unit-variance X). With
/// `scaled = 0` the output is in the original units, with an
/// intercept prepended (size = (p+1)×length(k)).
Value ridge(std::pmr::memory_resource *mr, const Value &y, const Value &X,
            const Value &k, bool scaled);

/// `[x, stdx, mse, S] = lscov(A, b[, w])` — weighted least squares.
/// `w` is an optional length-N vector of (positive) row weights;
/// omit / empty means uniform weights (= regular OLS).
/// Full N×N covariance form V intentionally not yet supported.
std::tuple<Value, Value, Value, Value>
lscov(std::pmr::memory_resource *mr, const Value &A, const Value &b,
      const Value &w);

} // namespace numkit::stats
