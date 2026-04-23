// libs/builtin/src/backends/MStdVarReduction.hpp
//
// Internal contract for SIMD-friendly two-pass variance used by
// var / std in MStdStats.cpp. Phase P5 of project_perf_optimization_plan.md.
//
// The previous Welford recurrence is numerically pristine but has a
// data dependency between iterations (each step needs the running mean
// and M2) → no SIMD. Two-pass trades 1-2 ULPs of precision at N=1M for
// a fully vector-parallel inner loop:
//   pass 1: SIMD sum  → mean
//   pass 2: SIMD sum-of-squared-deviations → variance / (N or N-1)
// At numkit-m's 1e-12 test tolerances the precision delta is invisible.

#pragma once

#include <cstddef>

namespace numkit::m::builtin {

// Returns (sum) of [p, p+n). SIMD-dispatched when NUMKIT_WITH_SIMD=ON.
double sumScan(const double *p, std::size_t n);

// Returns sum_{i} (p[i] - mean)^2. SIMD-dispatched, FMA where available
// for one rounding per term instead of two.
double sumSquaredDeviationsScan(const double *p, std::size_t n, double mean);

// In-place vector accumulate: dst[i] += src[i] for i in [0, n). SIMD-
// dispatched. Used by `sum(M, 2)` (column-pass row reduction): for each
// column c, accumulate M[:, c] into a running totals vector. Cached
// stores (NOT NT-stores) since dst is read every iteration.
void addInto(double *dst, const double *src, std::size_t n);

// Convenience: full two-pass variance. normFlag = 0 → sample (divide
// by n-1), 1 → population (divide by n). Matches MATLAB var(X, 0/1).
// Empty: NaN. n=1: 0 if normFlag==1 else NaN.
double varianceTwoPass(const double *p, std::size_t n, int normFlag);

} // namespace numkit::m::builtin
