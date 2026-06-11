// toolboxes/stats/include/numkit/stats/test/hypothesis.hpp
//
// Parametric and non-parametric hypothesis tests.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// @file
/// @brief Hypothesis tests. Most parametric tests return `(h, p, ci, tstat)`:
/// - `h ∈ {0, 1}`: 1 if H0 is rejected at the given significance level `α`
/// - `p`: two-sided (or one-sided per `tail`) p-value
/// - `ci`: `1 × 2` or `2 × 1` confidence interval at level `1 - α`
/// - `tstat`: test statistic

/// @brief Direction of the alternative hypothesis.
enum class TestTail {
    Both,    ///< Two-sided alternative (default).
    Right,   ///< Upper-tailed (statistic > null).
    Left     ///< Lower-tailed (statistic < null).
};

/// @brief One-sample Student's t-test (`[h, p, ci, t] = ttest(x, m, alpha, tail)`).
///
/// Tests `H0: mean(x) = m` versus the alternative selected by `tail`.
///
/// @param x      Sample data (1-D).
/// @param m      Hypothesised mean (default 0).
/// @param alpha  Significance level (default 0.05).
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ci, tstat)`.
/// @see ttest2, ztest
std::tuple<Value, Value, Value, Value>
ttest(const Value &x, double m, double alpha, TestTail tail,
      std::pmr::memory_resource *mr = nullptr);

/// @brief Two-sample t-test (`[h, p, ci, t] = ttest2(x, y, alpha, tail, vartype)`).
///
/// `H0: mean(x) = mean(y)`.
///
/// @param x        First sample.
/// @param y        Second sample.
/// @param alpha    Significance level.
/// @param tail     Direction of the alternative.
/// @param vartype  `"equal"` (pooled variance) or `"unequal"` (Welch, default).
/// @param mr       Memory resource (nullptr → process default).
/// @return         `(h, p, ci, tstat)`.
/// @see ttest, vartest2
std::tuple<Value, Value, Value, Value>
ttest2(const Value &x, const Value &y, double alpha, TestTail tail,
       const std::string &vartype, std::pmr::memory_resource *mr = nullptr);

/// @brief Z-test with known σ (`[h, p, ci, z] = ztest(x, m, sigma, alpha, tail)`).
///
/// `H0: mean(x) = m` assuming the population standard deviation `sigma` is known.
///
/// @param x      Sample data.
/// @param m      Hypothesised mean.
/// @param sigma  Known population standard deviation (`sigma > 0`).
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ci, zstat)`.
/// @see ttest
std::tuple<Value, Value, Value, Value>
ztest(const Value &x, double m, double sigma, double alpha, TestTail tail,
      std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared one-sample variance test
/// (`[h, p, ci, chi2] = vartest(x, v, alpha, tail)`).
///
/// `H0: var(x) = v`.
///
/// @param x      Sample data.
/// @param v      Hypothesised variance.
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ci, chisqstat)`.
/// @see vartest2
std::tuple<Value, Value, Value, Value>
vartest(const Value &x, double v, double alpha, TestTail tail,
        std::pmr::memory_resource *mr = nullptr);

/// @brief F-test for equality of two variances
/// (`[h, p, ci, F] = vartest2(x, y, alpha, tail)`).
///
/// `H0: var(x) = var(y)`.
///
/// @param x      First sample.
/// @param y      Second sample.
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ci, Fstat)`.
/// @see vartest
std::tuple<Value, Value, Value, Value>
vartest2(const Value &x, const Value &y, double alpha, TestTail tail,
         std::pmr::memory_resource *mr = nullptr);

/// @brief One-sample Kolmogorov-Smirnov test
/// (`[h, p, ksstat, cv] = kstest(x, cdf, alpha, tail)`).
///
/// `H0`: `x` is drawn from the reference CDF.
///
/// @param x      Sample data.
/// @param cdf    Reference CDF as a 2-column `[x_grid, F_grid]` matrix.
///               Empty → standard normal `N(0, 1)`.
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ksstat, cv)` — h, p-value, KS statistic, critical value.
/// @see kstest2
std::tuple<Value, Value, Value, Value>
kstest(const Value &x, const Value &cdf, double alpha, TestTail tail,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Two-sample Kolmogorov-Smirnov test
/// (`[h, p, ksstat, cv] = kstest2(x, y, alpha, tail)`).
///
/// `H0`: `x` and `y` come from the same continuous distribution.
///
/// @param x      First sample.
/// @param y      Second sample.
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, ksstat, cv)`.
/// @see kstest
std::tuple<Value, Value, Value, Value>
kstest2(const Value &x, const Value &y, double alpha, TestTail tail,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Jarque-Bera normality test
/// (`[h, p, jbstat, cv] = jbtest(x, alpha)`).
///
/// Small samples (`n < 2000`) use Monte-Carlo simulation under H0
/// for the p-value; large `n` uses the asymptotic χ²(2). The
/// p-value is capped at 0.5.
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, jbstat, cv)`.
/// @see jbtest(x, alpha, mctol, mr)
std::tuple<Value, Value, Value, Value>
jbtest(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);

/// @brief Anderson-Darling test for normality
/// (`[h, p, adstat, cv] = adtest(x, alpha)`).
///
/// `H0`: `x` is drawn from a normal distribution with parameters
/// estimated from the sample (`mu = mean(x)`, `sigma = std(x)`).
///
/// Algorithm: sort and z-standardise, compute
/// `A² = -n - (1/n) · Σ (2i-1) · [ln Φ(z_i) + ln(1 - Φ(z_{n+1-i}))]`,
/// then the Stephens-1986 small-sample adjustment
/// `A²* = A² · (1 + 0.75/n + 2.25/n²)`. p-value from D'Agostino-Stephens
/// (1986) piecewise rational fit for the parameters-estimated case.
///
/// @param x      Sample data (length ≥ 8 for reliable p-values).
/// @param alpha  Significance level (default 0.05).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, adstat, cv)` — decision, p-value, A²*, 5%
///               critical value (0.752 for estimated parameters).
std::tuple<Value, Value, Value, Value>
adtest(const Value &x, double alpha = 0.05,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Durbin-Watson test for first-order autocorrelation in
/// regression residuals (`[p, dw] = dwtest(r, X)`).
///
/// `H0`: residuals are uncorrelated (DW = 2). Alternative: positive
/// or negative first-order autocorrelation.
///
/// `DW = Σ_{i=2..n}(r_i - r_{i-1})² / Σ_{i=1..n} r_i²`.
///
/// p-value uses the **beta-approximation** (Durbin & Watson 1971):
/// the second-moment matches of `DW` under H0 are computed from the
/// design matrix `X` and fit to a `Beta(a, b)` on `[0, 4]`. This is
/// MATLAB's `'approximate'` method. The exact Pan-1965 algorithm is
/// a v1 KNOWN GAP — `method = 'exact'` is not supported and throws.
///
/// @param r       OLS residuals (length n, column).
/// @param X       Design matrix (n × k) used to fit the residuals.
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(p, dw)` — two-sided p-value, DW statistic.
std::tuple<Value, Value>
dwtest(const Value &r, const Value &X,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Jarque-Bera with explicit Monte-Carlo tolerance
/// (`[h, p, jbstat, cv] = jbtest(x, alpha, mctol)`).
///
/// @param x      Sample data.
/// @param alpha  Significance level.
/// @param mctol  Target MC standard-error tolerance (default 1e-3 in the
///               other overload). Pass `NaN` to force the asymptotic path
///               even for small `n`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, jbstat, cv)`.
std::tuple<Value, Value, Value, Value>
jbtest(const Value &x, double alpha, double mctol,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Non-parametric sign test
/// (`[p, h, sign] = signtest(x, y_or_m, alpha, tail)`).
///
/// `H0: median(x - m0) = 0` (one-sample) or `median(x - y) = 0` (paired).
///
/// @param x        Sample data.
/// @param y_or_m   Paired sample (same length as `x`) or scalar hypothesised
///                 median, depending on `y_or_m.numel()`.
/// @param alpha    Significance level.
/// @param tail     Direction of the alternative.
/// @param mr       Memory resource (nullptr → process default).
/// @return         `(p, h, sign)` — p-value, decision, # positive differences.
/// @see signrank
std::tuple<Value, Value, Value>
signtest(const Value &x, const Value &y_or_m, double alpha, TestTail tail,
         std::pmr::memory_resource *mr = nullptr);

/// @brief Bartlett's k-sample variance test (`[p, chi2, df] = vartestn(x, group, alpha)`).
///
/// `H0`: all group variances are equal.
///
/// @param x      Pooled sample column.
/// @param group  Group labels (same length as `x`).
/// @param alpha  Significance level.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(p, chisqstat, df)`.
/// @see vartestn_full
std::tuple<Value, Value, Value>
vartestn(const Value &x, const Value &group, double alpha,
         std::pmr::memory_resource *mr = nullptr);

/// @brief Full k-sample variance test with method selection
/// (`vartestn_full(x, group, test)`).
///
/// `test` selects the statistic:
/// - 0 = Bartlett (χ², `df2 = NaN`)
/// - 1 = Levene quadratic
/// - 2 = Levene absolute
/// - 3 = Brown-Forsythe
/// - 4 = O'Brien
///
/// For non-Bartlett tests `df1 = k - 1`, `df2 = N - k`.
///
/// @param x      Pooled sample column.
/// @param group  Group labels.
/// @param test   Statistic selector (0..4).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(p, stat, df1, df2)`.
std::tuple<Value, Value, Value, Value>
vartestn_full(const Value &x, const Value &group, int test,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Fisher's exact test for a 2×2 table
/// (`[h, p, OR, ci_lo, ci_hi] = fishertest(T, alpha, tail)`).
///
/// `T = [a b; c d]`. Returns the odds ratio `OR = a·d / (b·c)` and a
/// `1 - α` Woolf log-OR confidence interval.
///
/// @param T      `2 × 2` contingency table.
/// @param alpha  Significance level.
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, OR, ci_lo, ci_hi)`.
std::tuple<Value, Value, Value, Value, Value>
fishertest(const Value &T, double alpha, TestTail tail,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared goodness-of-fit (frequency form)
/// (`[h, p, chi2, df] = chi2gof(observed, expected, nparams, alpha)`).
///
/// `df = k - 1 - nparams`. The auto-binned distribution-fit form
/// (without `Frequency`) is not supported in this release.
///
/// @param observed  Observed count vector (length k).
/// @param expected  Expected count vector (length k).
/// @param nparams   Number of distribution parameters estimated from data
///                  (0 for fully specified null).
/// @param alpha     Significance level.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `(h, p, chi2stat, df)`.
std::tuple<Value, Value, Value, Value>
chi2gof(const Value &observed, const Value &expected, int nparams,
        double alpha, std::pmr::memory_resource *mr = nullptr);

/// @brief Wald-Wolfowitz runs test for randomness
/// (`[p, h, nruns, n1, n0, zval] = runstest(x, v, alpha, tail, method)`).
///
/// Default `v = median(x)`; values exactly equal to `v` are dropped.
/// `method` ∈ {`"exact"`, `"approximate"`}. `zval` is `NaN` in exact mode.
///
/// @param x       Sample data.
/// @param v       Threshold for the binary split (NaN → use `median(x)`).
/// @param alpha   Significance level.
/// @param tail    Direction of the alternative.
/// @param method  Computation method.
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(p, h, nruns, n1, n0, zval)`.
std::tuple<Value, Value, Value, Value, Value, Value>
runstest(const Value &x, double v, double alpha, TestTail tail,
         const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief Wilcoxon rank-sum (Mann-Whitney U) test
/// (`[p, h, ranksum_x, zval] = ranksum(x, y, alpha, tail, method)`).
///
/// `H0: median(x) = median(y)`. Default `method` is `"exact"` when both
/// samples have `< 10` observations, else `"approximate"` (normal with
/// tie and continuity correction). `zval` is `NaN` in exact mode.
///
/// @param x       First sample.
/// @param y       Second sample.
/// @param alpha   Significance level.
/// @param tail    Direction of the alternative.
/// @param method  `"exact"` or `"approximate"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(p, h, ranksum_x, zval)`.
/// @see signrank
std::tuple<Value, Value, Value, Value>
ranksum(const Value &x, const Value &y, double alpha, TestTail tail,
        const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief Wilcoxon signed-rank test
/// (`[p, h, signedrank, zval] = signrank(x, y_or_m, alpha, tail, method)`).
///
/// `H0: median(x - m0) = 0` (one-sample) or `median(x - y) = 0` (paired).
/// Default `method` = `"exact"` if `n_eff ≤ 15`, else `"approximate"`.
/// `zval` is `NaN` in exact mode.
///
/// @param x        Sample data.
/// @param y_or_m   Paired sample or scalar hypothesised median.
/// @param alpha    Significance level.
/// @param tail     Direction of the alternative.
/// @param method   `"exact"` or `"approximate"`.
/// @param mr       Memory resource (nullptr → process default).
/// @return         `(p, h, signedrank, zval)`.
/// @see ranksum, signtest
std::tuple<Value, Value, Value, Value>
signrank(const Value &x, const Value &y_or_m, double alpha, TestTail tail,
         const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief Ansari-Bradley two-sample scale (dispersion) test
/// (`[h, p, W, Wstar] = ansaribradley(x, y, alpha, tail)`).
///
/// Non-parametric `H0`: `x` and `y` have the same dispersion
/// (variances are equal). The Ansari-Bradley statistic `W` is the
/// sum, in the pooled sorted sample, of the V-shaped ranks
/// `1, 2, …, ⌈N/2⌉, …, 2, 1` over positions occupied by `x`.
///
/// Under `H0`, `W` is asymptotically normal with closed-form mean
/// and variance (Hollander & Wolfe). The standardised statistic is
/// `Wstar = (W − E[W]) / sqrt(V[W])`; the p-value follows from
/// `Φ(Wstar)`.
///
/// KNOWN GAPs: tied-rank correction (MATLAB applies a small
/// tie-correction to V[W] when ties are present) and exact
/// permutation distribution for small samples are deferred —
/// asymptotic only.
///
/// @param x      First sample.
/// @param y      Second sample.
/// @param alpha  Significance level (default 0.05).
/// @param tail   Direction of the alternative.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, W, Wstar)`.
/// @see vartest2, ranksum
std::tuple<Value, Value, Value, Value>
ansaribradley(const Value &x, const Value &y, double alpha, TestTail tail,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Lilliefors normality test
/// (`[h, p, kstat, critval] = lillietest(x, alpha)`).
///
/// Tests the composite hypothesis that `x` comes from a normal distribution
/// with unknown mean and variance. Computes the Kolmogorov-Smirnov statistic
/// against the fitted normal, applies the Stephens (1974) modified statistic
/// `D* = D·(sqrt(n) - 0.01 + 0.85/sqrt(n))`, and reads `p` / the critical
/// value from the Lilliefors table (interpolated; `p` clamped to `[1e-10, 0.5]`).
///
/// @param x      Sample (numel >= 4).
/// @param alpha  Significance level in `(0, 1)` (default 0.05).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(h, p, kstat, critval)` — reject flag, approximate
///               p-value, KS statistic `D`, and critical `D` at `alpha`.
/// @throws Error if `numel(x) < 4`, `alpha` is not in `(0, 1)`, or the
///         sample has zero variance.
std::tuple<Value, Value, Value, Value>
lillietest(const Value &x, double alpha = 0.05,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
