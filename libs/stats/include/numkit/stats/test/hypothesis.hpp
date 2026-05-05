// libs/stats/include/numkit/stats/test/hypothesis.hpp
//
// Parametric hypothesis tests. All return a 4-tuple:
//   (h, p, ci, tstat)
// where h ∈ {0, 1} (1 = reject H0 at given α), p is the p-value,
// ci is a 1×2 (or 2×1) confidence interval at level (1 - α), and tstat
// is the test statistic.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

enum class TestTail {
    Both,    // two-sided (default)
    Right,   // x > μ₀
    Left     // x < μ₀
};

/// ttest(x[, m, alpha, tail]) — one-sample Student's t-test.
/// H0: mean(x) = m   (default m = 0).
std::tuple<Value, Value, Value, Value>
ttest(std::pmr::memory_resource *mr, const Value &x,
      double m, double alpha, TestTail tail);

/// ttest2(x, y[, alpha, tail, vartype]) — two-sample t-test.
/// vartype: "equal" (pooled variance) or "unequal" (Welch, default).
std::tuple<Value, Value, Value, Value>
ttest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
       double alpha, TestTail tail, const std::string &vartype);

/// ztest(x, m, sigma[, alpha, tail]) — z-test with known σ.
std::tuple<Value, Value, Value, Value>
ztest(std::pmr::memory_resource *mr, const Value &x,
      double m, double sigma, double alpha, TestTail tail);

/// vartest(x, v[, alpha, tail]) — chi-squared one-sample variance test.
/// H0: var(x) = v.
std::tuple<Value, Value, Value, Value>
vartest(std::pmr::memory_resource *mr, const Value &x,
        double v, double alpha, TestTail tail);

/// vartest2(x, y[, alpha, tail]) — F-test for equality of variances.
std::tuple<Value, Value, Value, Value>
vartest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
         double alpha, TestTail tail);

/// kstest(x[, cdf, alpha, tail]) — one-sample Kolmogorov-Smirnov.
/// `cdf` is a 2-column matrix [x_grid, F_grid] giving the reference CDF;
/// when empty, defaults to standard normal N(0, 1).
/// Returns (h, p, ksstat, cv).
std::tuple<Value, Value, Value, Value>
kstest(std::pmr::memory_resource *mr, const Value &x,
       const Value &cdf, double alpha, TestTail tail);

/// kstest2(x, y[, alpha, tail]) — two-sample KS.
std::tuple<Value, Value, Value, Value>
kstest2(std::pmr::memory_resource *mr, const Value &x, const Value &y,
        double alpha, TestTail tail);

/// jbtest(x[, alpha]) — Jarque-Bera normality test.
/// Returns (h, p, jbstat, cv).
std::tuple<Value, Value, Value, Value>
jbtest(std::pmr::memory_resource *mr, const Value &x, double alpha);

/// signtest(x[, m | y][, alpha, tail]) — non-parametric sign test.
/// H0: median(x - m₀) = 0 (or median(x - y) = 0 for paired).
/// Returns (p, h, sign) where `sign` = number of positive differences;
/// engine-side wraps `sign` (and other diagnostics) into a struct.
std::tuple<Value, Value, Value>
signtest(std::pmr::memory_resource *mr, const Value &x,
         const Value &y_or_m, double alpha, TestTail tail);

/// vartestn(x, group[, alpha]) — Bartlett's k-sample variance test.
/// H0: all group variances are equal. Returns (p, chisqstat, df).
std::tuple<Value, Value, Value>
vartestn(std::pmr::memory_resource *mr, const Value &x, const Value &group,
         double alpha);

/// fishertest(T[, alpha, tail]) — Fisher's exact test for a 2×2
/// contingency table T = [a b; c d]. Returns (h, p, OR, ci_lo, ci_hi)
/// where OR is the odds ratio a·d/(b·c) and the 95% (or 1−α) Woolf
/// log-OR confidence interval.
std::tuple<Value, Value, Value, Value, Value>
fishertest(std::pmr::memory_resource *mr, const Value &T,
           double alpha, TestTail tail);

/// chi2gof — frequency form: given Observed counts and Expected counts,
/// compute chi² goodness-of-fit. df = k − 1 − nparams.
/// (Auto-binned distribution-fit form — chi2gof(x) without Frequency —
///  is intentionally not supported in this release.)
std::tuple<Value, Value, Value, Value>
chi2gof(std::pmr::memory_resource *mr,
        const Value &observed, const Value &expected,
        int nparams, double alpha);

/// runstest(x[, v][, alpha, tail][, method]) — Wald-Wolfowitz runs
/// test for randomness. Default `v` = median(x); values exactly equal
/// to v are dropped. Default `method` = "exact". Returns
/// (p, h, nruns, n1, n0, zval) — the engine wrapper packs everything
/// after `h` into a stats struct (zval omitted in exact mode).
std::tuple<Value, Value, Value, Value, Value, Value>
runstest(std::pmr::memory_resource *mr, const Value &x, double v,
         double alpha, TestTail tail, const std::string &method);

/// ranksum(x, y[, alpha, tail][, method]) — Wilcoxon rank-sum
/// (Mann-Whitney U). H0: median(x) = median(y).
/// Default `method` = "exact" iff both samples have < 10 observations,
/// else "approximate" (normal w/ tie + continuity correction).
/// Returns (p, h, ranksum_x, zval). zval is NaN unless approximate.
std::tuple<Value, Value, Value, Value>
ranksum(std::pmr::memory_resource *mr, const Value &x, const Value &y,
        double alpha, TestTail tail, const std::string &method);

/// signrank(x[, m | y][, alpha, tail][, method]) — Wilcoxon signed-rank.
/// H0: median(x - m₀) = 0 (or median(x - y) = 0 for paired).
/// `method` ∈ {"exact", "approximate"}. Default: "exact" if n_eff ≤ 15,
/// otherwise "approximate". Returns (p, h, signedrank, zval). `zval` is
/// NaN unless approximate mode was used.
std::tuple<Value, Value, Value, Value>
signrank(std::pmr::memory_resource *mr, const Value &x,
         const Value &y_or_m, double alpha, TestTail tail,
         const std::string &method);

} // namespace numkit::stats
