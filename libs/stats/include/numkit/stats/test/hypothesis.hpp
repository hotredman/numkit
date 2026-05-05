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
