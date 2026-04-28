// libs/builtin/include/numkit/m/builtin/MStdStats.hpp
//
// Descriptive statistics — Phase 1 of the parity expansion plan.
// MATLAB-compatible signatures with explicit `dim` argument support.
//
// Conventions:
//   * dim is 1-based (matches MATLAB).
//   * dim == 0 means "use the first non-singleton dim" (matches what
//     MATLAB does when the user omits the argument).
//   * For vectors and scalars, the dim argument is ignored — the
//     entire input is reduced to a scalar.
//   * normalization flag for var/std: 0 → divide by N-1 (default,
//     unbiased estimator), 1 → divide by N (population variance).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

// ── var / std ─────────────────────────────────────────────────────────
// var(X)            → variance with N-1 normalization, first non-singleton dim
// var(X, w)         → w == 0 (default, N-1) or 1 (population, N)
// var(X, w, dim)    → variance along given dim
//
// Pass dim == 0 to mean "auto" (first non-singleton). normFlag must be
// 0 or 1 — anything else throws.
MValue var(Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);
MValue stdev(Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);

// ── median ─────────────────────────────────────────────────────────────
// median(X)         → median along first non-singleton dim
// median(X, dim)    → median along given dim
//
// MATLAB convention: for an even-length slice, returns the average of
// the two middle elements. NaN in the slice currently propagates (the
// nanmedian variant comes in Phase 2).
MValue median(Allocator &alloc, const MValue &x, int dim = 0);

// ── quantile / prctile ─────────────────────────────────────────────────
// quantile(X, p)        → p in [0,1], scalar or vector
// quantile(X, p, dim)   → along given dim
// prctile(X, p)         → same as quantile(X, p/100)
//
// When p is a vector of length k, the reduced dim of the output has
// length k instead of 1 (matching MATLAB). The default interpolation
// method is linear (between order statistics), MATLAB's default for
// quantile/prctile.
MValue quantile(Allocator &alloc, const MValue &x, const MValue &p, int dim = 0);
MValue prctile(Allocator &alloc, const MValue &x, const MValue &p, int dim = 0);

// ── mode ───────────────────────────────────────────────────────────────
// mode(X)               → most-frequent value along first non-singleton dim
// mode(X, dim)          → along given dim
//
// Returns (mode_value, frequency). If multiple values tie for most
// frequent, returns the smallest (MATLAB convention).
std::tuple<MValue, MValue>
mode(Allocator &alloc, const MValue &x, int dim = 0);

// ── NaN-aware reductions ──────────────────────────────────────────────
// All-NaN slice handling matches MATLAB:
//   * nansum  → returns 0    (treat NaN as additive identity)
//   * others  → return NaN   (no defined value when nothing is observed)
// For partial-NaN slices, NaNs are skipped and the count of valid
// observations is the divisor for nanmean/nanvar/nanstd.
//
// nanvar/nanstd take the same normalization flag as var/std (0 = N-1
// where N is the non-NaN count, 1 = N).
MValue nansum   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmean  (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmax   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanmin   (Allocator &alloc, const MValue &x, int dim = 0);
MValue nanvar   (Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);
MValue nanstdev (Allocator &alloc, const MValue &x, int normFlag = 0, int dim = 0);
MValue nanmedian(Allocator &alloc, const MValue &x, int dim = 0);

// ── skewness / kurtosis ───────────────────────────────────────────────
// skewness(X[, normFlag[, dim]]) — sample skewness E[((X-μ)/σ)^3].
//   normFlag = 1 (default): uncorrected y = m3 / m2^1.5
//   normFlag = 0: bias-corrected y *= sqrt(n*(n-1))/(n-2). Requires n ≥ 3.
// MATLAB convention; default normFlag = 1.
MValue skewness(Allocator &alloc, const MValue &x, int normFlag = 1, int dim = 0);

// kurtosis(X[, normFlag[, dim]]) — sample kurtosis (NON-excess; equals
// 3 for a normal distribution per MATLAB convention).
//   normFlag = 1 (default): uncorrected y = m4 / m2^2
//   normFlag = 0: bias-corrected
//     y = ((n-1)/((n-2)(n-3))) * ((n+1)*g2 - 3*(n-1)) + 3,
//     where g2 = m4/m2^2. Requires n ≥ 4.
MValue kurtosis(Allocator &alloc, const MValue &x, int normFlag = 1, int dim = 0);

} // namespace numkit::m::builtin
