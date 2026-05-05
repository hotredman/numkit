// libs/stats/include/numkit/stats/descriptive/descriptive.hpp
//
// Descriptive statistics — Phase 1 of the parity expansion plan.
// MATLAB-compatible signatures with explicit `dim` argument support.
// Physical home moved from libs/builtin to libs/stats in Phase 7b
// (Statistics Toolbox content per MATLAB taxonomy).
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

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

// ── var / std ─────────────────────────────────────────────────────────
// var(X)            → variance with N-1 normalization, first non-singleton dim
// var(X, w)         → w == 0 (default, N-1) or 1 (population, N)
// var(X, w, dim)    → variance along given dim
//
// Pass dim == 0 to mean "auto" (first non-singleton). normFlag must be
// 0 or 1 — anything else throws.
Value var(std::pmr::memory_resource *mr, const Value &x, int normFlag = 0, int dim = 0);
Value stdev(std::pmr::memory_resource *mr, const Value &x, int normFlag = 0, int dim = 0);

// ── median ─────────────────────────────────────────────────────────────
// median(X)         → median along first non-singleton dim
// median(X, dim)    → median along given dim
//
// MATLAB convention: for an even-length slice, returns the average of
// the two middle elements. NaN in the slice currently propagates (the
// nanmedian variant comes in Phase 2).
Value median(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// ── quantile / prctile ─────────────────────────────────────────────────
// quantile(X, p)        → p in [0,1], scalar or vector
// quantile(X, p, dim)   → along given dim
// prctile(X, p)         → same as quantile(X, p/100)
//
// When p is a vector of length k, the reduced dim of the output has
// length k instead of 1 (matching MATLAB). The default interpolation
// method is linear (between order statistics), MATLAB's default for
// quantile/prctile.
Value quantile(std::pmr::memory_resource *mr, const Value &x, const Value &p, int dim = 0);
Value prctile(std::pmr::memory_resource *mr, const Value &x, const Value &p, int dim = 0);

// ── mode ───────────────────────────────────────────────────────────────
// mode(X)               → most-frequent value along first non-singleton dim
// mode(X, dim)          → along given dim
//
// Returns (mode_value, frequency). If multiple values tie for most
// frequent, returns the smallest (MATLAB convention).
std::tuple<Value, Value>
mode(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

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
Value cov(std::pmr::memory_resource *mr, const Value &x, int normFlag = 0);
Value cov(std::pmr::memory_resource *mr, const Value &x, const Value &y, int normFlag = 0);

// corrcoef(X) / corrcoef(X, Y) — correlation coefficient matrix.
// R(i,j) = C(i,j) / sqrt(C(i,i) * C(j,j)) where C = cov(...).
Value corrcoef(std::pmr::memory_resource *mr, const Value &x);
Value corrcoef(std::pmr::memory_resource *mr, const Value &x, const Value &y);

// ── bounds ─────────────────────────────────────────────────────────────
// bounds(X[, dim]) → (min, max) along dim. Two-output form mirrors
// MATLAB's [lo, hi] = bounds(X).
std::tuple<Value, Value>
bounds(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// ── iqr ────────────────────────────────────────────────────────────────
// iqr(X[, dim]) — interquartile range = quantile(X, 0.75) - quantile(X, 0.25).
Value iqr(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// ── maxk / mink ────────────────────────────────────────────────────────
// maxk(X, k[, dim]) — k largest along dim, descending. mink — k smallest,
// ascending. NaN sorts last (MATLAB convention).
Value maxk(std::pmr::memory_resource *mr, const Value &x, int k, int dim = 0);
Value mink(std::pmr::memory_resource *mr, const Value &x, int k, int dim = 0);

// ── rmse ───────────────────────────────────────────────────────────────
// rmse(F, A[, dim]) — root-mean-square deviation of F from A.
// F and A must be broadcast-compatible.
Value rmse(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim = 0);

// ── mape ───────────────────────────────────────────────────────────────
// mape(F, A[, dim]) — mean absolute percentage error of forecast F vs
// actual A: 100 * mean(|(A - F) / A|, dim). Zero entries in A produce
// Inf in the per-element ratio (MATLAB matches this; the mean propagates
// the Inf).
Value mape(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim = 0);

// ── ecdf ───────────────────────────────────────────────────────────────
// ecdf(y) — empirical cumulative distribution function. Returns a pair
// (f, x) of column vectors of length K+1, where K is the number of
// unique values in y. f starts at 0 (at x = min(y)) and steps up to 1
// at x = max(y). Each subsequent jump is at a unique data value with
// height = (cumulative count) / N.
//
// Matches MATLAB's `[f, x] = ecdf(y)`. NaN values are excluded before
// counting (MATLAB skips them silently; we do the same).
std::tuple<Value, Value>
ecdf(std::pmr::memory_resource *mr, const Value &y);

// ── ecdfhist ───────────────────────────────────────────────────────────
// [n, c] = ecdfhist(f, x [, m]) — convert empirical CDF (`[f, x]`
// from `ecdf`) into a probability-density histogram. m is the number
// of bins (default 10). Returns:
//   n — bin heights (probability density, sums × width = 1)
//   c — bin centers
// Each bin spans equal width (range/m). Last bin includes its right
// edge; other bins are [a, b). NaN-safe for empty inputs.
std::tuple<Value, Value>
ecdfhist(std::pmr::memory_resource *mr, const Value &f, const Value &x, int m = 10);

// ── normalize ──────────────────────────────────────────────────────────
// normalize(A[, method]) — column-wise data normalisation.
//   method ∈ {"zscore" (default), "norm", "range", "center", "scale",
//             "medianiqr"}.
//     zscore     : (A − mean) / std    (population stdev, normFlag=0)
//     center     : A − mean
//     scale      : A / std
//     range      : (A − min) / (max − min)
//     norm       : A / sqrt(sum(A²))   (unit ℓ²-norm per column)
//     medianiqr  : (A − median) / iqr  (robust)
// Output has the same shape as `A`; statistics are computed column-wise.
Value normalize(std::pmr::memory_resource *mr, const Value &A,
                const std::string &method);

// ── rescale ────────────────────────────────────────────────────────────
// rescale(A[, lo, hi]) — linearly map A onto [lo, hi]. Defaults
// lo=0, hi=1. Constant-data input collapses to lo (degenerate case
// matching MATLAB).
Value rescale(std::pmr::memory_resource *mr, const Value &A,
              double lo, double hi);

// ── zscore ─────────────────────────────────────────────────────────────
// zscore(A) — alias for normalize(A, "zscore").
Value zscore(std::pmr::memory_resource *mr, const Value &A);

} // namespace numkit::stats
