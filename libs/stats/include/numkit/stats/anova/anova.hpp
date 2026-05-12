// libs/stats/include/numkit/stats/anova/anova.hpp
//
// One-way ANOVA + dummy-coding helper.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// `anova1(y, group)` — one-way ANOVA. Returns (p, F, df_between,
/// df_within, ss_between, ss_within). The engine wrapper packs the
/// auxiliary outputs into a 4×6 cell-array table to match MATLAB's
/// `[p, tbl] = anova1(y, group, 'off')` form.
std::tuple<double, double, double, double, double, double>
anova1(const Value &y, const Value &group, std::pmr::memory_resource *mr = nullptr);

/// `kruskalwallis(y, group)` — Kruskal-Wallis non-parametric one-way
/// ANOVA. Returns (p, H, df, ssRanks). Tie-corrected H statistic.
std::tuple<Value, Value, Value, Value>
kruskalwallis(const Value &y, const Value &group, std::pmr::memory_resource *mr = nullptr);

/// `dummyvar(group)` — convert categorical labels to indicator columns.
/// `group` is a length-N vector of integer / numeric labels (or string
/// labels — we normalise via toString conversion). Result is N×K where
/// K is the number of distinct labels (sorted ascending for numeric).
Value dummyvar(const Value &group, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
