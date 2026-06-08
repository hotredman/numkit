// toolboxes/stats/include/numkit/stats/fit/fit.hpp
//
// Distribution fitters and likelihood scoring (function-form).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @file
/// @brief Distribution fitters (`*fit`) and negative-log-likelihood
/// scoring (`*like`).
///
/// Each fitter returns scalar (or per-element) parameter estimates plus
/// `1 × 2` confidence intervals at level `1 - alpha`. Default
/// `alpha = 0.05` (95% CI).

/// @brief Normal MLE without censoring
/// (`[muhat, sigmahat, muci, sigmaci] = normfit(x, alpha)`).
///
/// `muhat = mean(x)`, `sigmahat = std(x, normFlag=0)` (sample, N-1).
/// CIs: t-based for `mu`, χ² for `sigma`.
///
/// @param x      Sample data.
/// @param alpha  Significance level (default 0.05 in `*_reg`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(muhat, sigmahat, muci, sigmaci)`.
/// @see normlike
std::tuple<Value, Value, Value, Value>
normfit(const Value &x, double alpha,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Normal MLE with optional censoring and frequencies.
///
/// Wald CIs (analytic Fisher information) + log-σ transform for the
/// censored case.
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param cens   Right-censoring mask (`cens[i] != 0` → censored).
///               Pass `Value::Empty` for no censoring.
/// @param freq   Per-sample weights. `Value::Empty` → all ones.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(muhat, sigmahat, muci, sigmaci)`.
/// @see normfit(x, alpha, mr)
std::tuple<Value, Value, Value, Value>
normfit(const Value &x, double alpha, const Value &cens, const Value &freq,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Poisson MLE (`[lhat, lci] = poissfit(x, alpha)`).
///
/// `lambda = mean(x)`. Exact CI from χ² inversion of the cumulative
/// Poisson tail.
///
/// @param x      Sample counts.
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(lhat, lci)`.
std::tuple<Value, Value>
poissfit(const Value &x, double alpha,
         std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential MLE
/// (`[muhat, muci] = expfit(x, alpha, cens, freq)`).
///
/// `mu = Σ(freq·x) / Σ(freq·(1 - cens))`. Exact CI via `χ²(2D)` where
/// `D = Σ(freq·(1 - cens))` is the event count.
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param cens   Right-censoring mask (`Value::Empty` → no censoring).
/// @param freq   Per-sample weights (`Value::Empty` → ones).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(muhat, muci)`.
/// @see explike
std::tuple<Value, Value>
expfit(const Value &x, double alpha,
       const Value &cens = Value::Empty, const Value &freq = Value::Empty,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Continuous-uniform MLE
/// (`[ahat, bhat, aci, bci] = unifit(x, alpha)`).
///
/// `a = min(x)`, `b = max(x)`. CI based on `(b - a) · (alpha^{-1/n} - 1)`.
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(ahat, bhat, aci, bci)`.
std::tuple<Value, Value, Value, Value>
unifit(const Value &x, double alpha,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal MLE without censoring
/// (`[parm, pci] = lognfit(x, alpha)`).
///
/// MLE of `mu`, `sigma` on `log(x)`. `parm` is a `1 × 2` row;
/// `pci` is `2 × 2` (col 1 = mu CI, col 2 = sigma CI; row 1 = lower,
/// row 2 = upper).
///
/// @param x      Sample data (positive).
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(parm, pci)`.
/// @see lognlike
std::tuple<Value, Value>
lognfit(const Value &x, double alpha,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal MLE with censoring and frequencies.
///
/// Closed-form weighted moments when `freq` is supplied without
/// censoring; EM-iterated MLE on `log(x)` with numeric Hessian for
/// CIs when right-censored.
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param cens   Right-censoring mask (`Value::Empty` → no censoring).
/// @param freq   Per-sample weights (`Value::Empty` → ones).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(parm, pci)`.
/// @see lognfit(x, alpha, mr)
std::tuple<Value, Value>
lognfit(const Value &x, double alpha, const Value &cens, const Value &freq,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial parameter CI (`[phat, pci] = binofit(x, n, alpha)`).
///
/// Clopper–Pearson exact binomial CI for `x` successes out of `n` trials.
/// Vector inputs produce a column vector `phat` and `N × 2` `pci`.
///
/// @param x      Success counts (scalar or vector).
/// @param n      Trial counts (matching shape).
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(phat, pci)`.
std::tuple<Value, Value>
binofit(const Value &x, const Value &n, double alpha,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Rayleigh MLE (`[shat, sci] = raylfit(x, alpha)`).
///
/// `σ̂ = sqrt(Σx² / (2N))`. CI from χ² inversion of
/// `2N · σ̂² ~ σ² · χ²(2N)`.
///
/// @param x      Sample data (positive).
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(shat, sci)`.
std::tuple<Value, Value>
raylfit(const Value &x, double alpha,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Normal negative-log-likelihood (`nLogL = normlike(mu, sigma, x, cens, freq)`).
///
/// Handles the `(censoring, freq)` optional args:
/// - `censoring[i] != 0` → element `i` is right-censored; contribute
///   `-log(1 - F(z_i)) = -log(0.5·erfc(z_i/√2))` instead of `-log(f)`.
/// - `freq[i]` (default 1) → multiplies the element's contribution.
///   `freq[i] == 0` removes the element from the sum.
///
/// Boundary handling: `sigma <= 0` → `NaN`; any `NaN` in `x` → `NaN`;
/// empty `x` → 0.
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param x      Sample data.
/// @param cens   Right-censoring mask (empty Value for none).
/// @param freq   Per-sample weights (empty Value for ones).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar `-Σ log f(x_i; θ)`.
/// @see normfit
double normlike(double mu, double sigma, const Value &x,
                const Value &cens, const Value &freq,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential nLogL (`nLogL = explike(mu, x)`).
///
/// @param mu  Mean parameter.
/// @param x   Sample data.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar negative log-likelihood.
/// @see expfit
double explike(double mu, const Value &x,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal nLogL (`nLogL = lognlike(mu, sigma, x)`).
///
/// @param mu     Mean of `log(x)`.
/// @param sigma  Std of `log(x)`.
/// @param x      Sample data (positive).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar negative log-likelihood.
/// @see lognfit
double lognlike(double mu, double sigma, const Value &x,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma nLogL (`nLogL = gamlike(a, b, x)`).
///
/// @param a   Shape parameter.
/// @param b   Scale parameter.
/// @param x   Sample data.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar negative log-likelihood.
double gamlike(double a, double b, const Value &x,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Beta nLogL (`nLogL = betalike(a, b, x)`).
///
/// @param a   First shape parameter.
/// @param b   Second shape parameter.
/// @param x   Sample data in `(0, 1)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar negative log-likelihood.
double betalike(double a, double b, const Value &x,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull nLogL (`nLogL = wbllike(a, b, x)`).
///
/// @param a   Scale parameter.
/// @param b   Shape parameter.
/// @param x   Sample data.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar negative log-likelihood.
double wbllike(double a, double b, const Value &x,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV nLogL (`nLogL = evlike(mu, sigma, x)`).
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param x      Sample data.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar negative log-likelihood.
double evlike(double mu, double sigma, const Value &x,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Extreme Value nLogL (`nLogL = gevlike(k, sigma, mu, x)`).
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param x      Sample data.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar negative log-likelihood.
double gevlike(double k, double sigma, double mu, const Value &x,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Pareto nLogL (`nLogL = gplike(k, sigma, x)`, theta = 0).
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param x      Sample data.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar negative log-likelihood.
double gplike(double k, double sigma, const Value &x,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
