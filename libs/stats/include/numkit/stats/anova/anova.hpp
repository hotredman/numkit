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
anova1(std::pmr::memory_resource *mr, const Value &y, const Value &group);

/// `dummyvar(group)` — convert categorical labels to indicator columns.
/// `group` is a length-N vector of integer / numeric labels (or string
/// labels — we normalise via toString conversion). Result is N×K where
/// K is the number of distinct labels (sorted ascending for numeric).
Value dummyvar(std::pmr::memory_resource *mr, const Value &group);

} // namespace numkit::stats
