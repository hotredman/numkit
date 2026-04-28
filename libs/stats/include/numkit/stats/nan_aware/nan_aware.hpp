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

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::stats {

Value nansum   (Allocator &alloc, const Value &x, int dim = 0);
Value nanmean  (Allocator &alloc, const Value &x, int dim = 0);
Value nanmax   (Allocator &alloc, const Value &x, int dim = 0);
Value nanmin   (Allocator &alloc, const Value &x, int dim = 0);
Value nanvar   (Allocator &alloc, const Value &x, int normFlag = 0, int dim = 0);
Value nanstdev (Allocator &alloc, const Value &x, int normFlag = 0, int dim = 0);
Value nanmedian(Allocator &alloc, const Value &x, int dim = 0);

} // namespace numkit::stats
