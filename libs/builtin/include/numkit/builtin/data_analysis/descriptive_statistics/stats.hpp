// libs/builtin/include/numkit/builtin/data_analysis/descriptive_statistics/stats.hpp
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

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

// ── var / std ─────────────────────────────────────────────────────────
// var(X)            → variance with N-1 normalization, first non-singleton dim
// var(X, w)         → w == 0 (default, N-1) or 1 (population, N)
// var(X, w, dim)    → variance along given dim
//
// Pass dim == 0 to mean "auto" (first non-singleton). normFlag must be
// 0 or 1 — anything else throws.
Value var(Allocator &alloc, const Value &x, int normFlag = 0, int dim = 0);
Value stdev(Allocator &alloc, const Value &x, int normFlag = 0, int dim = 0);

// ── median ─────────────────────────────────────────────────────────────
// median(X)         → median along first non-singleton dim
// median(X, dim)    → median along given dim
//
// MATLAB convention: for an even-length slice, returns the average of
// the two middle elements. NaN in the slice currently propagates (the
// nanmedian variant comes in Phase 2).
Value median(Allocator &alloc, const Value &x, int dim = 0);

// ── quantile / prctile ─────────────────────────────────────────────────
// quantile(X, p)        → p in [0,1], scalar or vector
// quantile(X, p, dim)   → along given dim
// prctile(X, p)         → same as quantile(X, p/100)
//
// When p is a vector of length k, the reduced dim of the output has
// length k instead of 1 (matching MATLAB). The default interpolation
// method is linear (between order statistics), MATLAB's default for
// quantile/prctile.
Value quantile(Allocator &alloc, const Value &x, const Value &p, int dim = 0);
Value prctile(Allocator &alloc, const Value &x, const Value &p, int dim = 0);

// ── mode ───────────────────────────────────────────────────────────────
// mode(X)               → most-frequent value along first non-singleton dim
// mode(X, dim)          → along given dim
//
// Returns (mode_value, frequency). If multiple values tie for most
// frequent, returns the smallest (MATLAB convention).
std::tuple<Value, Value>
mode(Allocator &alloc, const Value &x, int dim = 0);

// nan-aware reductions (nansum, nanmean, nanmax, nanmin, nanvar,
// nanstdev, nanmedian) and the higher moments (skewness, kurtosis)
// moved to libs/stats — Statistics Toolbox content. See:
//   <numkit/stats/nan_aware/nan_aware.hpp>
//   <numkit/stats/moments/moments.hpp>

// ── cov / corrcoef ────────────────────────────────────────────────────
//
// cov(X)            — for vector X returns var(X); for n×p matrix X
//                     returns the p×p sample covariance matrix
//                     C = (X - mean(X))' * (X - mean(X)) / (n - 1).
// cov(X, Y)         — joint covariance of two equal-length vectors,
//                     2×2 matrix.
// cov(X, normFlag)  — normFlag=0 → divide by n-1 (default, sample);
//                     normFlag=1 → divide by n (population).
// 2D matrix path only — 3D / N-D arrays throw.
Value cov(Allocator &alloc, const Value &x, int normFlag = 0);
Value cov(Allocator &alloc, const Value &x, const Value &y, int normFlag = 0);

// corrcoef(X) / corrcoef(X, Y) — correlation coefficient matrix.
// R(i,j) = C(i,j) / sqrt(C(i,i) * C(j,j)) where C = cov(...).
Value corrcoef(Allocator &alloc, const Value &x);
Value corrcoef(Allocator &alloc, const Value &x, const Value &y);

} // namespace numkit::builtin
