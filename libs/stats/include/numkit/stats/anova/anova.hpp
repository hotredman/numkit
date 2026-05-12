// libs/stats/include/numkit/stats/anova/anova.hpp
//
// One-way ANOVA + dummy-coding helper.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// One-way ANOVA (`[p, F, df_b, df_w, ss_b, ss_w] = anova1(y, group)`).
///
/// Returns the standard test statistics: between-group p-value `p`,
/// F-statistic, between/within degrees of freedom, and between/within
/// sums-of-squares. The engine wrapper packs the auxiliary outputs
/// into a 4×6 cell-array table to match MATLAB's
/// `[p, tbl] = anova1(y, group, 'off')` form.
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

/// Convert categorical labels to indicator columns (`X = dummyvar(group)`).
///
/// Result is N×K where K is the number of distinct labels (sorted
/// ascending for numeric labels; string labels are normalised via
/// `toString` conversion first). Useful as the design matrix for
/// regression with categorical predictors.
Value dummyvar(const Value &group,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
