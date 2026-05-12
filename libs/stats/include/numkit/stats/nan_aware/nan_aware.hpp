// libs/stats/include/numkit/stats/nan_aware/nan_aware.hpp
//
// NaN-aware reductions. All-NaN slice handling matches MATLAB:
//   * nansum  → returns 0    (treat NaN as additive identity)
//   * others  → return NaN   (no defined value when nothing is observed)
// For partial-NaN slices, NaNs are skipped and the count of valid
// observations is the divisor for nanmean/nanvar/nanstd.
//
// nanvar/nanstd take the same normalization flag as var/std (0 = N-1
// where N is the non-NaN count, 1 = N).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// NaN-aware sum (`y = nansum(x, dim)`). All-NaN slices return 0.
Value nansum(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// NaN-aware mean (`y = nanmean(x, dim)`). All-NaN slices return NaN.
Value nanmean(const Value &x, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// NaN-aware maximum (`y = nanmax(x, dim)`).
Value nanmax(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// NaN-aware minimum (`y = nanmin(x, dim)`).
Value nanmin(const Value &x, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// NaN-aware variance (`y = nanvar(x, normFlag, dim)`). `normFlag`
/// follows @ref var (0 = N-1, 1 = N), where N is the count of
/// non-NaN observations.
Value nanvar(const Value &x, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// NaN-aware standard deviation (`y = nanstdev(x, normFlag, dim)`).
Value nanstdev(const Value &x, int normFlag = 0, int dim = 0,
               std::pmr::memory_resource *mr = nullptr);

/// NaN-aware median (`y = nanmedian(x, dim)`).
Value nanmedian(const Value &x, int dim = 0,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
