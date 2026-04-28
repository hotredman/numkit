// libs/stats/src/nan_aware/backends/nan_reductions.hpp
//
// Internal contract for single-pass NaN-skipping reductions used by
// nansum / nanmean in stats.cpp. SIMD-dispatched (Highway) when
// NUMKIT_WITH_SIMD=ON, scalar otherwise.
//
// Phase P2 of project_perf_optimization_plan.md — replaces the two-pass
// compactNonNan+sum pattern (one full read for compaction, one for sum)
// with a single read that masks NaN lanes to zero before accumulation
// and counts valid lanes in parallel for the mean.

#pragma once

#include <cstddef>

namespace numkit::stats::detail {

// Single pass over [p, p+n). NaN positions contribute 0. Returns the
// sum of the non-NaN values. Empty / all-NaN input returns 0.
double nanSumScan(const double *p, std::size_t n);

struct NanSumCount {
    double sum;
    std::size_t count;
};

// Single pass returning both the sum (NaN→0) and the count of non-NaN
// elements. The caller computes mean = count > 0 ? sum/count : NaN.
NanSumCount nanSumCountScan(const double *p, std::size_t n);

// Single-pass min / max over [p, p+n) skipping NaN. NaN lanes are
// masked to the neutral element (+inf for min, -inf for max) before the
// SIMD reduction. Returns NaN for empty / all-NaN slices (matches
// MATLAB's nanmin / nanmax convention).
double nanMaxScan(const double *p, std::size_t n);
double nanMinScan(const double *p, std::size_t n);

// Two-pass NaN-skipping variance:
//   pass 1: nanSumCountScan -> mean (NaN if count == 0)
//   pass 2: SIMD sum of (x - mean)^2 over non-NaN lanes only
// Result divided by (count - {0,1}) per the normFlag (matches MATLAB
// var(x, 0/1) semantics). All-NaN -> NaN; one valid value -> 0 for
// normFlag==1, NaN for normFlag==0.
double nanVarianceTwoPass(const double *p, std::size_t n, int normFlag);

} // namespace numkit::stats::detail
