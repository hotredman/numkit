// toolboxes/stats/include/numkit/stats/anova/anova.hpp
//
// One-way ANOVA + dummy-coding helper.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// One-way ANOVA (`[p, F, df_b, df_w, ss_b, ss_w] = anova1(y, group)`).
///
/// Returns the standard test statistics: between-group p-value `p`,
/// F-statistic, between/within degrees of freedom, and between/within
/// sums-of-squares. The engine wrapper packs the auxiliary outputs
/// into a 4×6 cell-array summary table.
///
/// @param y      Response vector.
/// @param group  Grouping labels (same length as `y`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(p, F, df_between, df_within, ss_between, ss_within)`.
///
/// @see kruskalwallis
std::tuple<double, double, double, double, double, double>
anova1(const Value &y, const Value &group,
       std::pmr::memory_resource *mr = nullptr);

/// Kruskal–Wallis non-parametric one-way ANOVA
/// (`[p, H, df, ssRanks] = kruskalwallis(y, group)`).
///
/// Tie-corrected H statistic. Use as the non-parametric alternative
/// to @ref anova1 when normality is suspect.
///
/// @param y      Response vector.
/// @param group  Grouping labels.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(p, H, df, ssRanks)`.
///
/// @see anova1
std::tuple<Value, Value, Value, Value>
kruskalwallis(const Value &y, const Value &group,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Convert categorical labels to indicator columns
/// (`X = dummyvar(group)`).
///
/// Result is N×K where K is the number of distinct labels (sorted
/// ascending for numeric labels; string labels are normalised via
/// `toString` conversion first). Useful as the design matrix for
/// regression with categorical predictors.
///
/// @param group  Categorical labels (N-element vector).
/// @param mr     Memory resource (nullptr → process default).
/// @return       N×K indicator matrix.
/// @see anova1
Value dummyvar(const Value &group,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Multiple-comparison correction methods for `multcompare`.
enum class McCorrection { Bonferroni, LSD };

/// @brief Pairwise post-hoc comparisons after one-way ANOVA
/// (`c = multcompare(stats [, alpha [, ctype]])`).
///
/// `stats` is the 3rd output of `anova1` (a struct with fields
/// `gnames`, `means`, `n`, `s`, `df`). For each pair `(i, j)` of
/// groups, returns a row
///
///   `[ i, j, lower_CI, mean_diff, upper_CI, p_value ]`
///
/// where `mean_diff = means(i) - means(j)`, `p_value` is the two-sided
/// t-test p adjusted for multiplicity, and CI half-width is
/// `t_crit · s · sqrt(1/n_i + 1/n_j)`.
///
/// Supported `ctype`:
///   - `McCorrection::Bonferroni` (default in v1) — multiply each
///     unadjusted p by the number of pairs `K = k(k-1)/2`, clip to 1.
///   - `McCorrection::LSD` — Fisher's least-significant-difference;
///     no multiplicity correction (raw t-test).
///
/// KNOWN GAP: Tukey HSD (MATLAB default) requires the studentized
/// range distribution — not in v1.
///
/// @param stats  Struct from `anova1`.
/// @param alpha  Significance level (default 0.05).
/// @param ctype  Correction method.
/// @param mr     Memory resource.
/// @return       `K × 6` matrix of pairwise comparisons.
Value multcompare(const Value &stats, double alpha = 0.05,
                   McCorrection ctype = McCorrection::Bonferroni,
                   std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
