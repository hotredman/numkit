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

Value nansum   (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanmean  (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanmax   (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanmin   (const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanvar   (const Value &x, int normFlag = 0, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanstdev (const Value &x, int normFlag = 0, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value nanmedian(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
