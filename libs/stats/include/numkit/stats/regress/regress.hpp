// libs/stats/include/numkit/stats/regress/regress.hpp
//
// Linear regression — function form.

#pragma once

#include <limits>
#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit { class Engine; }

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

/// @brief Nonlinear least-squares fit result
/// (`[beta, R, J, CovB, MSE] = nlinfit(X, y, fun, beta0)`).
struct NlinfitResult {
    Value beta;   ///< Parameter estimates (`p × 1`).
    Value R;      ///< Residuals (`n × 1`).
    Value J;      ///< Jacobian at `beta` (`n × p`).
    Value CovB;   ///< Parameter covariance (`p × p`).
    Value MSE;    ///< Mean squared error (scalar).
};

/// @brief Nonlinear least-squares fit via Levenberg-Marquardt.
///
/// Fits `y ≈ fun(beta, X)` by minimising `||y - fun(beta, X)||²`.
/// `fun` is a function handle taking `(beta, X)` and returning a
/// length-`n` predicted vector.
///
/// Algorithm: classical LM with adaptive damping (λ scaled by 10× on
/// rejected steps, by 0.1× on accepted steps), numerical Jacobian via
/// central differences with relative step `1e-7 · max(|β|, 1)`.
///
/// KNOWN GAPs: name-value `'Weights'`, `'ErrorModel'`, `'Options'` not
/// supported. Returns `MSE = SSE / (n - p)`.
///
/// @param X      Predictor data (`n × k`).
/// @param y      Response vector (`n × 1`).
/// @param fun    Function handle `fun(beta, X)` returning predictions.
/// @param beta0  Initial parameter estimate.
/// @param engine Engine pointer for `callFunctionHandle` (required —
///               adapters pass `ctx.engine`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `{beta, R, J, CovB, MSE}`.
NlinfitResult nlinfit(const Value &X, const Value &y,
                      const Value &fun, const Value &beta0,
                      ::numkit::Engine *engine,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Parameter confidence intervals from an nlinfit fit
/// (`ci = nlparci(beta, R, J [, alpha])`).
///
/// `ci(i, :) = beta(i) ± t_{α/2, n-p} · se(i)` where
/// `se(i) = sqrt(MSE · ((J'·J)^{-1})_{ii})`.
///
/// @param beta   Parameter vector from nlinfit (`p × 1`).
/// @param R      Residuals from nlinfit (`n × 1`).
/// @param J      Jacobian from nlinfit (`n × p`).
/// @param alpha  Significance level (default 0.05 → 95% CI).
/// @param mr     Memory resource.
/// @return       `p × 2` CI matrix; `[lower, upper]` per row.
Value nlparci(const Value &beta, const Value &R, const Value &J,
              double alpha = 0.05,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Prediction confidence intervals from an nlinfit fit
/// (`[ypred, delta] = nlpredci(fun, X, beta, R, J [, alpha])`).
///
/// For each row of `X`, returns `ypred(i) = fun(beta, X(i,:))` and
/// `delta(i) = t_{α/2, n-p} · sqrt(MSE · g_i' · (J'·J)^{-1} · g_i)`
/// where `g_i = ∂fun/∂beta` at `X(i,:)`.
///
/// @param fun    Function handle.
/// @param X      Query points (`m × k`).
/// @param beta   Parameter vector.
/// @param R      Residuals from the fit.
/// @param J      Jacobian from the fit (`n × p`).
/// @param alpha  Significance level (default 0.05).
/// @param engine Engine pointer (adapter passes `ctx.engine`).
/// @param mr     Memory resource.
/// @return       Tuple `(ypred, delta)` — predictions and CI half-widths.
std::tuple<Value, Value>
nlpredci(const Value &fun, const Value &X, const Value &beta,
         const Value &R, const Value &J,
         double alpha,
         ::numkit::Engine *engine,
         std::pmr::memory_resource *mr = nullptr);

/// @brief Robust regression weight functions.
enum class RobustWeight { Bisquare, Huber };

/// @brief Robust linear regression via iteratively-reweighted least
/// squares (`[b, stats] = robustfit(X, y)`).
///
/// Fits `y = X · b + ε` while downweighting outliers. Algorithm:
///   1. Initial OLS β.
///   2. Loop: standardise residuals by `s = MAD(r) / 0.6745`, compute
///      weights `w_i = ψ(r_i / (tune · s))` (bisquare default, tune =
///      4.685; or Huber, tune = 1.345), refit β via weighted LS.
///   3. Stop on convergence or 50 iterations.
///
/// KNOWN GAPs: column-of-ones is NOT prepended automatically — pass
/// `[ones(n, 1), X]` for an intercept. Stats output struct
/// (degrees of freedom, p-values, etc.) is currently scalar `s` only.
///
/// @param X       Design matrix (`n × p`).
/// @param y       Response (`n × 1`).
/// @param weight  Weight function (`Bisquare` default).
/// @param tune    Tuning constant (`NaN` → 4.685 for bisquare, 1.345
///                for Huber).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `{b, s}` — coefficients and final robust scale.
struct RobustfitResult { Value b; Value s; };
RobustfitResult robustfit(const Value &X, const Value &y,
                           RobustWeight weight = RobustWeight::Bisquare,
                           double tune = std::numeric_limits<double>::quiet_NaN(),
                           std::pmr::memory_resource *mr = nullptr);

/// @brief Robust multivariate covariance estimate via trimmed-MCD
/// (`[sigma, mu] = robustcov(X)`).
///
/// Iterative concentration-step approach (a simplified FAST-MCD):
///   1. Start from classical mean / cov.
///   2. Compute Mahalanobis distances, keep the `h = ceil(0.75 · n)`
///      smallest, recompute mean / cov on the kept subset.
///   3. Iterate until the kept set stabilises (or 20 iterations).
///   4. Apply standard consistency correction `c = MAD / chi2inv(0.75, d)`.
///
/// KNOWN GAPs: full FAST-MCD with multiple random elemental subsets
/// (Rousseeuw-Van Driessen 1999), MVE method, OGK estimator — not in
/// v1. v1 returns a single-start estimate which differs from MATLAB
/// when the data has heavy contamination clusters.
///
/// @param X   Data matrix (`n × d`, rows = observations).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{sigma, mu}` — `d × d` robust covariance and `1 × d`
///            robust location vector.
struct RobustcovResult { Value sigma; Value mu; };
RobustcovResult robustcov(const Value &X,
                           std::pmr::memory_resource *mr = nullptr);

/// @brief GLM family of error distributions.
enum class GlmDistribution {
    Normal,
    Binomial,
    Poisson,
    Gamma,
    InverseGaussian,
};

/// @brief GLM link function.
enum class GlmLink {
    Identity,    ///< g(μ) = μ
    Logit,       ///< g(μ) = log(μ / (1 - μ))
    Log,         ///< g(μ) = log(μ)
    Reciprocal,  ///< g(μ) = 1 / μ
    Probit,      ///< g(μ) = Φ^-1(μ)
};

/// @brief GLM fit result.
struct GlmfitResult {
    Value b;     ///< Coefficients `(p + 1) × 1` — intercept first.
    Value dev;   ///< Deviance (scalar).
};

/// @brief Fit a generalised linear model
/// (`[b, dev] = glmfit(X, y, distr [, link])`).
///
/// Solves `μ = g^{-1}(X · β)` via iteratively-reweighted least
/// squares (IRLS) with the natural / canonical link by default.
///
/// Supported distributions and canonical links:
///   - `'normal'`     → identity
///   - `'binomial'`   → logit
///   - `'poisson'`    → log
///   - `'gamma'`      → reciprocal
///
/// A column of ones is **prepended automatically** to `X` for the
/// intercept term (MATLAB convention). Pass `[]` for `link` to use
/// the canonical link.
///
/// KNOWN GAPs (v1):
///   - `y` for `'binomial'` must be a proportion (`y ∈ [0, 1]`); the
///     `[successes, trials]` two-column form is not yet supported.
///   - Returned stats reduced to `dev` (deviance) only — no standard
///     errors, t-statistics, p-values, residuals struct.
///   - `'constant'` name-value pair (off/on) not supported — intercept
///     is always added.
///
/// @param X      Design matrix (`n × p`, no intercept column).
/// @param y      Response (`n × 1`).
/// @param distr  Distribution family.
/// @param link   Link function (default = canonical for `distr`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `{b, dev}` — coefficients with intercept first, and
///               total deviance.
GlmfitResult glmfit(const Value &X, const Value &y,
                     GlmDistribution distr,
                     GlmLink link = GlmLink::Identity,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluate a fitted GLM (`yhat = glmval(b, X, link)`).
///
/// Computes `yhat = g^{-1}([1, X] · b)` where `g^{-1}` is the inverse
/// of the link used in the fit. `b` is `(p + 1) × 1` (intercept first,
/// as returned by `glmfit`).
///
/// @param b      Coefficient vector from `glmfit`.
/// @param X      Query points (`m × p`, no intercept column).
/// @param link   Link function used in the fit.
/// @param mr     Memory resource.
/// @return       `m × 1` vector of predicted means.
Value glmval(const Value &b, const Value &X, GlmLink link,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
