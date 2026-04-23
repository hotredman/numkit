// libs/builtin/src/backends/MStdNanReductions.hpp
//
// Internal contract for single-pass NaN-skipping reductions used by
// nansum / nanmean in MStdStats.cpp. SIMD-dispatched (Highway) when
// NUMKIT_WITH_SIMD=ON, scalar otherwise.
//
// Phase P2 of project_perf_optimization_plan.md — replaces the two-pass
// compactNonNan+sum pattern (one full read for compaction, one for sum)
// with a single read that masks NaN lanes to zero before accumulation
// and counts valid lanes in parallel for the mean.

#pragma once

#include <cstddef>

namespace numkit::m::builtin {

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

} // namespace numkit::m::builtin
