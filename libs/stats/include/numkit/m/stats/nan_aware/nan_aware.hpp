// libs/stats/include/numkit/m/stats/nan_aware/nan_aware.hpp
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

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::stats {

MValue nansum   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmean  (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmax   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmin   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanvar   (Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);
MValue nanstdev (Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);
MValue nanmedian(Allocator &alloc, const MValue &x, int dim = 0);

} // namespace numkit::m::stats
