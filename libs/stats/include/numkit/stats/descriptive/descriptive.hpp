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
#include <utility>
#include <vector>

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

// ── partialcorr (Pearson partial correlation) ──────────────────────
// partialcorr(X, Y, Z) — partial correlation matrix between columns
// of X and Y, controlling for the variables in Z. Same number of
// rows in X, Y, Z; X and Y can have different column counts.
Value partialcorr_of(std::pmr::memory_resource *mr,
                     const Value &X, const Value &Y, const Value &Z);

// ── corr (Pearson form alias to corrcoef) ──────────────────────────
// corr(X) — auto-correlation across columns of X (same as corrcoef(X))
// corr(X, Y) — correlation matrix between X and Y columns
// (Other types 'Spearman' / 'Kendall' / 'Type' option deferred.)
Value corr_xx(std::pmr::memory_resource *mr, const Value &X);
Value corr_xy(std::pmr::memory_resource *mr, const Value &X, const Value &Y);

// ── detrend ────────────────────────────────────────────────────────
// Remove polynomial trend of order n from x (default n=1, linear).
// Returns x minus best-fit polynomial; vector form. Matrix form
// detrends each column separately.
Value detrend_of(std::pmr::memory_resource *mr, const Value &x, int order = 1);

// ── isoutlier / rmoutliers / fillmissing / rmmissing / standardizeMissing ──
Value isoutlier_of(std::pmr::memory_resource *mr, const Value &x);
Value rmoutliers_of(std::pmr::memory_resource *mr, const Value &x);
Value fillmissing_of(std::pmr::memory_resource *mr, const Value &x,
                     const std::string &method, double constVal = 0.0);
Value rmmissing_of(std::pmr::memory_resource *mr, const Value &x);
Value standardizeMissing_of(std::pmr::memory_resource *mr,
                            const Value &x, double sentinel);

// ── range / mad / geomean / harmmean / moment / trimmean ──────────────
// All take optional dim; default dim=0 means first non-singleton.
Value range_of(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value mad_of(std::pmr::memory_resource *mr, const Value &x, int flag = 0, int dim = 0);
Value geomean_of(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value harmmean_of(std::pmr::memory_resource *mr, const Value &x, int dim = 0);
Value moment_of(std::pmr::memory_resource *mr, const Value &x, int order, int dim = 0);
Value trimmean_of(std::pmr::memory_resource *mr, const Value &x, double pct, int dim = 0);

// ── prepareCurveData ───────────────────────────────────────────────────
// prepareCurveData(x, y[, w]) — strip NaN/Inf rows from paired data and
// return column vectors. With three arguments, weights w are also
// filtered (w == 0 is kept; only NaN/Inf in any of x/y/w drop the row).
// Always returns column vectors; empty input → 0×1 columns.
std::tuple<Value, Value, Value>
prepareCurveData(std::pmr::memory_resource *mr,
                 const Value &x, const Value &y, const Value &w);

// ── prepareSurfaceData ─────────────────────────────────────────────────
// prepareSurfaceData(x, y, z) — strip NaN/Inf entries from three-way
// data and return column vectors. Inputs may be vectors of equal length
// or matrices that share a shape (e.g. meshgrid output). All three are
// linearised in column-major order before filtering.
std::tuple<Value, Value, Value>
prepareSurfaceData(std::pmr::memory_resource *mr,
                   const Value &x, const Value &y, const Value &z);

// ── datastats ──────────────────────────────────────────────────────────
// datastats(x) — descriptive struct for a single dataset; engine-side
// wraps the 7 returned scalars into a struct {num, max, min, mean,
// median, range, std}. Matches Curve Fitting Toolbox conventions.
// NaN values in `x` propagate via the underlying reductions (matching
// MATLAB's datastats behaviour).
std::tuple<Value, Value, Value, Value, Value, Value, Value>
datastats(std::pmr::memory_resource *mr, const Value &x);

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

// ── ksdensity ──────────────────────────────────────────────────────────
// [f, xi, bw] = ksdensity(x[, pts, 'Bandwidth', bw]) — Gaussian kernel
// density estimate. Default 100-point grid centred on the data range,
// Silverman's-rule bandwidth (might differ slightly from MATLAB's
// internal heuristic). Empty `pts` requests the auto grid.
std::tuple<Value, Value, Value>
ksdensity(std::pmr::memory_resource *mr, const Value &x, const Value &pts,
          double bw_user);

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

// ── tiedrank ───────────────────────────────────────────────────────────
// `[r, tieadj] = tiedrank(x)` — ranks adjusted for ties. Equal values
// share the average of their would-be sequential ranks. Vector input
// returns a scalar tieadj; matrix input applies column-wise and
// tieadj is a 1-by-cols row. NaN values keep NaN rank (skipped from
// the ranking sequence).
std::pair<Value, Value>
tiedrank(std::pmr::memory_resource *mr, const Value &x);

// ── corrcov ────────────────────────────────────────────────────────────
// `[R, sigma] = corrcov(C)` — derive a correlation matrix R from a
// covariance matrix C. R(i,j) = C(i,j) / sqrt(C(i,i) * C(j,j));
// sigma(i) = sqrt(C(i,i)) returned as a row vector. Negative diagonal
// entries throw; off-diagonal divisions by zero return NaN.
std::pair<Value, Value>
corrcov(std::pmr::memory_resource *mr, const Value &C);

// ── tabulate ───────────────────────────────────────────────────────────
// `T = tabulate(x)` — frequency table. Returns a 3-column
// [value, count, percent] matrix. Dense layout (rows for k = 1..max)
// when all non-NaN values are positive integers; otherwise sparse
// (one row per unique non-NaN value sorted ascending). NaN values
// are excluded both from the row set and from the percentage
// denominator.
Value tabulate(std::pmr::memory_resource *mr, const Value &x);

// ── cholcov ────────────────────────────────────────────────────────────
// `[T, p] = cholcov(SIGMA)` — Cholesky-like factor of a (possibly
// singular) covariance matrix. Returns T such that T'*T == SIGMA
// and the non-PD count p:
//   PD       -> T = upper-tri n×n,  p = 0
//   PSD < n  -> T = r×n,            p = 0
//   indef    -> T = empty 0×0,      p = #(eig <= -tol)
std::pair<Value, Value>
cholcov(std::pmr::memory_resource *mr, const Value &SIGMA);

// ── crosstab ───────────────────────────────────────────────────────────
// `[T, chi2, p] = crosstab(x [, y])` — contingency table.
//   single-arg: T is a column vector of frequency counts of unique x.
//   two-arg:    T(i,j) = count of pairs (x_k, y_k) with x_k = unique_x(i)
//               and y_k = unique_y(j). chi-square test of independence
//               supplied alongside.
// Numeric input only for v1; cell/string deferred.
std::tuple<Value, double, double>
crosstab(std::pmr::memory_resource *mr, const Value &x, const Value *y_opt);

// ── grpstats ───────────────────────────────────────────────────────────
// Per-group statistics. `fn_names` is empty for default (mean) or a
// list of one or more aggregator names from {mean, std, sum, numel,
// min, max, var, sem}. Returns one Value per fn name, each (Ng × C)
// where Ng is the number of unique non-NaN groups and C is the
// number of columns of X.
std::vector<Value>
grpstats(std::pmr::memory_resource *mr, const Value &X, const Value &group,
         const std::vector<std::string> &fn_names);

// ── nearcorr ───────────────────────────────────────────────────────────
// `Y = nearcorr(A)` — nearest correlation matrix to A in Frobenius norm
// (Higham 2002, IMA J. Numer. Anal. 22 (3): 329-343). Alternating
// projections between the PSD cone and the unit-diagonal subspace,
// with Dykstra's correction. Y is symmetric, PSD and has unit diagonal.
// Defaults: tol = 1e-10, maxits = 100. The 'tolconv'/'maxits' name-value
// parameters are deferred for v1.
Value nearcorr(std::pmr::memory_resource *mr, const Value &A);

} // namespace numkit::stats
