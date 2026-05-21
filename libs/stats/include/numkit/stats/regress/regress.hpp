// libs/stats/include/numkit/stats/regress/regress.hpp
//
// Linear regression — function form.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Ordinary least-squares regression
/// (`[b, bint, r, stats] = regress(y, X, alpha)`).
///
/// Fits the linear model `y = X · b + ε` by OLS.
///
/// @param y      Response vector (`N × 1`).
/// @param X      Design matrix (`N × p`; include a column of ones for an
///               intercept if desired).
/// @param alpha  Significance level for the CIs on `b` (e.g. 0.05).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Tuple `(b, bint, r, stats)`:
///               - `b`     : `p × 1` coefficient estimates
///               - `bint`  : `p × 2` confidence intervals at level `1 - α`
///               - `r`     : `N × 1` residuals
///               - `stats` : `1 × 4` row `[R², F, p_value, sigma²]`.
///               The `rint` output (outlier intervals on residuals)
///               is not provided in this revision.
/// @see ridge, lscov
std::tuple<Value, Value, Value, Value>
regress(const Value &y, const Value &X, double alpha,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Ridge regression (`B = ridge(y, X, k, scaled)`).
///
/// Solves `(X'X + λI) β = X'y` for one or more regularisation values.
///
/// @param y       Response vector.
/// @param X       Design matrix.
/// @param k       Scalar `λ` or vector of `λ` values; output has one
///                column per entry of `k`.
/// @param scaled  When `true` (default), returns coefficients in the
///                standardised feature space (centred + unit-variance
///                `X`). When `false`, returns coefficients in the
///                original units with an intercept prepended:
///                output is `(p + 1) × length(k)`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Coefficient matrix.
/// @see regress
Value ridge(const Value &y, const Value &X, const Value &k, bool scaled,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Weighted least squares (`[x, stdx, mse, S] = lscov(A, b, w)`).
///
/// Solves `min Σ w_i · (A_i · x - b_i)²`.
///
/// @param A   Design matrix.
/// @param b   Response vector.
/// @param w   Optional length-`N` vector of positive row weights (pass
///            empty Value for uniform weights, i.e. plain OLS). Full
///            `N × N` covariance form `V` is not yet supported.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(x, stdx, mse, S)`:
///            - `x`    : coefficient estimates
///            - `stdx` : standard errors of `x`
///            - `mse`  : mean squared error
///            - `S`    : coefficient covariance matrix.
/// @see regress
std::tuple<Value, Value, Value, Value>
lscov(const Value &A, const Value &b, const Value &w,
      std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
