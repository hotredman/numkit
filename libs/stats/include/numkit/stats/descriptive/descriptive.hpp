// libs/stats/include/numkit/stats/descriptive/descriptive.hpp
//
// Descriptive statistics — Phase 1 of the parity expansion plan.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

/// @file
/// @brief Descriptive statistics.
///
/// **Conventions across this header:**
/// - `dim` is 1-based.
/// - `dim == 0` means "use the first non-singleton dim" — the
///   behaviour when the argument is omitted.
/// - Vector / scalar inputs ignore `dim` — the whole input collapses
///   to a scalar.
/// - For `var`/`std`: `normFlag == 0` → divide by `N-1` (unbiased,
///   default), `normFlag == 1` → divide by `N` (population).

/// @brief Sample / population variance along `dim` (`y = var(X, normFlag, dim)`).
///
/// @param x         Input array.
/// @param normFlag  `0` → divide by `N-1` (default, sample), `1` → divide by `N`.
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Variance reduced along `dim`.
/// @throws Error    `normFlag` not in `{0, 1}` (`m:var:badNormFlag`).
/// @see stdev
Value var(const Value &x, int normFlag = 0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Sample / population standard deviation (`y = std(X, normFlag, dim)`).
///
/// Computed as `sqrt(var(X, normFlag, dim))`.
///
/// @param x         Input array.
/// @param normFlag  `0` (default, unbiased) or `1` (population).
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Standard deviation reduced along `dim`.
/// @see var
Value stdev(const Value &x, int normFlag = 0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Median along `dim` (`y = median(X, dim)`).
///
/// For an even-length slice, returns the average of the two middle
/// elements. NaN currently propagates (use `nanmedian` to skip them).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Median reduced along `dim`.
/// @see quantile, prctile
Value median(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Empirical quantile (`q = quantile(X, p, dim)`).
///
/// Linear interpolation between order statistics (the default).
/// When `p` is a length-`k` vector, the reduced dim of the output has
/// length `k` (one quantile per requested level).
///
/// @param x    Input array.
/// @param p    Probability level(s) in `[0, 1]`; scalar or vector.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Quantile(s) along `dim`.
/// @see prctile, median
Value quantile(const Value &x, const Value &p, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Empirical percentile (`q = prctile(X, p, dim)`).
///
/// Equivalent to `quantile(X, p/100, dim)`.
///
/// @param x    Input array.
/// @param p    Percentile level(s) in `[0, 100]`.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Percentile(s) along `dim`.
/// @see quantile
Value prctile(const Value &x, const Value &p, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Mode and frequency (`[m, f] = mode(X, dim)`).
///
/// Returns the most-frequent value (`m`) and its count (`f`). Ties
/// are broken by returning the smallest value.
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(mode_value, frequency)` along `dim`.
std::tuple<Value, Value>
mode(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

// nan-aware reductions and higher moments moved to companion headers:
//   <numkit/stats/nan_aware/nan_aware.hpp>
//   <numkit/stats/moments/moments.hpp>

/// @brief Sample / population covariance (`C = cov(X, normFlag)`).
///
/// For vector `X` returns `var(X)`; for `n × p` matrix returns the
/// `p × p` sample covariance `C = (X - mean(X))' · (X - mean(X)) / (n - 1)`.
/// 2-D path only — 3-D / N-D inputs throw.
///
/// @param x         Input array.
/// @param normFlag  `0` (default, sample, divide by `n-1`) or `1` (population, by `n`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Covariance matrix.
/// @see corrcoef
Value cov(const Value &x, int normFlag = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Joint covariance of two vectors (`C = cov(X, Y, normFlag)`).
///
/// Returns a `2 × 2` covariance matrix of two equal-length vectors.
///
/// @param x         First vector.
/// @param y         Second vector (same length as `x`).
/// @param normFlag  `0` (sample) or `1` (population).
/// @param mr        Memory resource (nullptr → process default).
/// @return          `2 × 2` joint covariance matrix.
/// @see corrcoef
Value cov(const Value &x, const Value &y, int normFlag = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Pearson correlation matrix (`R = corrcoef(X)`).
///
/// `R(i, j) = C(i, j) / sqrt(C(i, i) · C(j, j))` where `C = cov(X)`.
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Correlation matrix.
/// @see cov, corr_xx
Value corrcoef(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Joint correlation matrix (`R = corrcoef(X, Y)`).
///
/// @param x   First vector / matrix.
/// @param y   Second vector / matrix (compatible shape).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Joint correlation matrix.
/// @see corrcoef
Value corrcoef(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Min and max along `dim` (`[lo, hi] = bounds(X, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(min, max)` reduced along `dim`.
std::tuple<Value, Value>
bounds(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Interquartile range (`r = iqr(X, dim)`).
///
/// Computed as `quantile(X, 0.75) - quantile(X, 0.25)` along `dim`.
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     IQR reduced along `dim`.
/// @see quantile
Value iqr(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief `k` largest values along `dim` (`y = maxk(X, k, dim)`).
///
/// Output is sorted descending. NaN sorts last.
///
/// @param x    Input array.
/// @param k    Number of values to return.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `k` largest values along `dim`.
/// @see mink
Value maxk(const Value &x, int k, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief `k` smallest values along `dim` (`y = mink(X, k, dim)`).
///
/// Output is sorted ascending. NaN sorts last.
///
/// @param x    Input array.
/// @param k    Number of values to return.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `k` smallest values along `dim`.
/// @see maxk
Value mink(const Value &x, int k, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Root-mean-square error (`e = rmse(F, A, dim)`).
///
/// `e = sqrt(mean((F - A)².data, dim))`.
///
/// @param f    Forecast / predicted values.
/// @param a    Actual / reference values (broadcast-compatible with `f`).
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     RMSE reduced along `dim`.
/// @see mape
Value rmse(const Value &f, const Value &a, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean absolute percentage error (`e = mape(F, A, dim)`).
///
/// `e = 100 · mean(|(A - F) / A|, dim)`. Zero entries in `A` produce
/// `Inf` in the ratio.
///
/// @param f    Forecast values.
/// @param a    Actual values.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     MAPE reduced along `dim`.
/// @see rmse
Value mape(const Value &f, const Value &a, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Partial correlation controlling for Z
/// (`R = partialcorr(X, Y, Z)`).
///
/// Returns the partial correlation matrix between columns of `X` and
/// columns of `Y` after removing the linear contribution of `Z`.
///
/// @param X   `n × p` first set of variables.
/// @param Y   `n × q` second set of variables (`Y.rows == X.rows`).
/// @param Z   `n × r` controlling variables.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `p × q` partial-correlation matrix.
/// @see corrcoef
Value partialcorr_of(const Value &X, const Value &Y, const Value &Z, std::pmr::memory_resource *mr = nullptr);

/// @brief Auto-correlation across columns of X (`R = corr(X)`).
///
/// Equivalent to `corrcoef(X)`. Non-Pearson type options ("Spearman",
/// "Kendall") deferred.
///
/// @param X   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Correlation matrix across columns.
/// @see corrcoef
Value corr_xx(const Value &X, std::pmr::memory_resource *mr = nullptr);

/// @brief Cross-correlation between columns of X and Y (`R = corr(X, Y)`).
///
/// @param X   First matrix.
/// @param Y   Second matrix (`Y.rows == X.rows`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Correlation matrix between columns of `X` and `Y`.
/// @see corrcoef
Value corr_xy(const Value &X, const Value &Y, std::pmr::memory_resource *mr = nullptr);

/// @brief Remove polynomial trend (`y = detrend(x, order)`).
///
/// Returns `x` minus its best-fit polynomial of the given order.
/// Vectors detrend whole; matrices detrend per column.
///
/// @param x      Input array.
/// @param order  Polynomial order (default 1 = linear).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Detrended array, same shape as `x`.
Value detrend_of(const Value &x, int order = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Outlier mask (`tf = isoutlier(x)`).
///
/// Default method: median + 3·MAD test per dimension.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL mask, same shape as `x`.
/// @see rmoutliers
Value isoutlier_of(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Remove outliers (`y = rmoutliers(x)`).
///
/// Drops entries flagged by @ref isoutlier_of along the first
/// non-singleton dim.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `x` with outlier entries dropped.
/// @see isoutlier_of
Value rmoutliers_of(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Fill missing values (`y = fillmissing(x, method, constVal)`).
///
/// Replaces `NaN` / `missing` entries using the named method.
/// Supported methods: `"constant"`, `"previous"`, `"next"`,
/// `"nearest"`, `"linear"`, `"spline"`, `"pchip"`, `"makima"`,
/// `"movmean"`, `"movmedian"`.
///
/// @param x         Input array.
/// @param method    Fill method name.
/// @param constVal  Constant value for `"constant"` method (default 0).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Array with missing values filled.
/// @see rmmissing_of, standardizeMissing_of
Value fillmissing_of(const Value &x, const std::string &method, double constVal = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Remove rows / cols containing missing values
/// (`y = rmmissing(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Compact array with missing entries dropped.
/// @see fillmissing_of
Value rmmissing_of(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert sentinel value to missing (`y = standardizeMissing(x, sentinel)`).
///
/// Replaces `sentinel` entries with NaN.
///
/// @param x         Input array.
/// @param sentinel  Value to treat as missing.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Array with `sentinel` → NaN.
Value standardizeMissing_of(const Value &x, double sentinel, std::pmr::memory_resource *mr = nullptr);

/// @brief Range = `max - min` along `dim` (`r = range(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Range reduced along `dim`.
Value range_of(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean absolute deviation (`y = mad(x, flag, dim)`).
///
/// `flag = 0` (default): mean absolute deviation from mean
///   (`mean(|x - mean(x)|)`).
/// `flag = 1`: median absolute deviation from median.
///
/// @param x     Input array.
/// @param flag  Method selector (0 = MAD from mean, 1 = MAD from median).
/// @param dim   1-based dimension; 0 → first non-singleton dim.
/// @param mr    Memory resource (nullptr → process default).
/// @return      MAD reduced along `dim`.
Value mad_of(const Value &x, int flag = 0, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Geometric mean (`g = geomean(x, dim)`).
///
/// `g = (prod(x))^(1/n)` = `exp(mean(log(x)))`. Requires all entries `> 0`.
///
/// @param x    Input array (positive values).
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Geometric mean reduced along `dim`.
/// @see harmmean_of
Value geomean_of(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Harmonic mean (`h = harmmean(x, dim)`).
///
/// `h = n / sum(1/x)`. Requires all entries `> 0`.
///
/// @param x    Input array (positive values).
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Harmonic mean reduced along `dim`.
/// @see geomean_of
Value harmmean_of(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief k-th central moment (`m = moment(x, k, dim)`).
///
/// `m = mean((x - mean(x))^k, dim)`. `order = 2` reproduces population
/// variance (i.e. `var` with `normFlag = 1`).
///
/// @param x      Input array.
/// @param order  Moment order (integer `>= 1`).
/// @param dim    1-based dimension; 0 → first non-singleton dim.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `order`-th central moment reduced along `dim`.
Value moment_of(const Value &x, int order, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Trimmed mean (`m = trimmean(x, pct, dim)`).
///
/// Mean after dropping the smallest and largest `pct/2` percent of values.
///
/// @param x    Input array.
/// @param pct  Total trim percentage in `[0, 100)`.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Trimmed mean reduced along `dim`.
Value trimmean_of(const Value &x, double pct, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Curve-data cleanup (`[xo, yo, wo] = prepareCurveData(x, y, w)`).
///
/// Strips entries where any of `x`, `y`, `w` is `NaN` or `Inf` and
/// returns column vectors. `w == 0` entries are KEPT; only invalid
/// floats drop a row. Empty input → `0 × 1` columns.
///
/// @param x   x-coordinates.
/// @param y   y-coordinates.
/// @param w   Optional weights (pass `Value::Empty` to skip).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cleaned `(x, y, w)` columns.
std::tuple<Value, Value, Value>
prepareCurveData(const Value &x, const Value &y, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Surface-data cleanup (`[xo, yo, zo] = prepareSurfaceData(x, y, z)`).
///
/// Linearises (column-major) and strips `NaN` / `Inf`. Inputs may be
/// equal-length vectors or shape-matched matrices.
///
/// @param x   x-coordinates.
/// @param y   y-coordinates.
/// @param z   z-coordinates.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cleaned `(x, y, z)` columns.
std::tuple<Value, Value, Value>
prepareSurfaceData(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Dataset descriptive summary (`datastats(x)`).
///
/// Returns the seven curve-fitting descriptors. `NaN` values
/// propagate via the underlying reductions.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(num, max, min, mean, median, range, std)` tuple of scalars.
std::tuple<Value, Value, Value, Value, Value, Value, Value>
datastats(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Empirical cumulative distribution function (`[f, x] = ecdf(y)`).
///
/// Returns column vectors of length `K + 1`, where `K` is the number
/// of unique values in `y`. `f` starts at 0 and steps up to 1 at
/// `x = max(y)`. Each jump has height `count / N`. NaN excluded silently.
///
/// @param y   Input samples.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(f, x)` columns describing the step function.
/// @see ecdfhist, ksdensity
std::tuple<Value, Value>
ecdf(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Kernel density estimate (`[f, xi, bw] = ksdensity(x, pts, bw_user)`).
///
/// Gaussian kernel; auto bandwidth via Silverman's rule when
/// `bw_user <= 0`. Empty `pts` requests the default 100-point grid
/// centred on the data range.
///
/// @param x        Input samples.
/// @param pts      Evaluation grid (column vector) or `Value::Empty` for auto.
/// @param bw_user  User-specified bandwidth (≤ 0 → Silverman's rule).
/// @param mr       Memory resource (nullptr → process default).
/// @return         `(f, xi, bw)` — densities, grid, chosen bandwidth.
/// @see ecdf
std::tuple<Value, Value, Value>
ksdensity(const Value &x, const Value &pts, double bw_user, std::pmr::memory_resource *mr = nullptr);

/// @brief Histogram from empirical CDF (`[n, c] = ecdfhist(f, x, m)`).
///
/// Converts the output of @ref ecdf into a probability-density
/// histogram. Each bin spans equal width `range/m`; last bin
/// includes its right edge.
///
/// @param f   `f` column from @ref ecdf.
/// @param x   `x` column from @ref ecdf.
/// @param m   Number of bins (default 10).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(n, c)` — bin heights, bin centres.
/// @see ecdf
std::tuple<Value, Value>
ecdfhist(const Value &f, const Value &x, int m = 10, std::pmr::memory_resource *mr = nullptr);

/// @brief Column-wise normalisation (`Y = normalize(A, method)`).
///
/// Supported `method`:
/// - `"zscore"` (default): `(A - mean) / std` (population std, normFlag=0)
/// - `"center"`: `A - mean`
/// - `"scale"`: `A / std`
/// - `"range"`: `(A - min) / (max - min)`
/// - `"norm"`: `A / sqrt(sum(A²))` (unit ℓ² norm per column)
/// - `"medianiqr"`: `(A - median) / iqr` (robust)
///
/// @param A       Input matrix.
/// @param method  Normalisation method name (see list above).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Normalised matrix, same shape as `A`.
/// @see rescale, zscore
Value normalize(const Value &A, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// @brief Linear range remap (`Y = rescale(A, lo, hi)`).
///
/// Linearly maps `A` onto `[lo, hi]`. Constant input collapses to `lo`.
///
/// @param A   Input array.
/// @param lo  Target lower bound (default 0 via overload).
/// @param hi  Target upper bound (default 1 via overload).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Rescaled array, same shape as `A`.
/// @see normalize
Value rescale(const Value &A, double lo, double hi, std::pmr::memory_resource *mr = nullptr);

/// @brief Z-score normalisation (`Y = zscore(A)`).
///
/// Convenience alias for `normalize(A, "zscore")`.
///
/// @param A   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Z-score-normalised matrix.
/// @see normalize
Value zscore(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Tie-corrected ranks (`[r, tieadj] = tiedrank(x)`).
///
/// Equal values share the average of their would-be sequential ranks.
/// Vector input → scalar `tieadj`; matrix input applies column-wise
/// and `tieadj` is a `1 × cols` row. NaN entries keep `NaN` rank.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(ranks, tie_adjustment)` pair.
std::pair<Value, Value>
tiedrank(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Correlation matrix from covariance (`[R, sigma] = corrcov(C)`).
///
/// `R(i, j) = C(i, j) / sqrt(C(i, i) · C(j, j))`;
/// `sigma(i) = sqrt(C(i, i))` returned as a row vector.
/// Throws on negative diagonal; zero diagonal yields `NaN` off-diagonal.
///
/// @param C   Covariance matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(R, sigma)` pair.
/// @throws Error  Negative diagonal entry (`m:corrcov:negDiag`).
std::pair<Value, Value>
corrcov(const Value &C, std::pmr::memory_resource *mr = nullptr);

/// @brief Frequency table (`T = tabulate(x)`).
///
/// Returns a 3-column `[value, count, percent]` matrix. Dense layout
/// for positive-integer inputs (rows for `k = 1..max(x)`); otherwise
/// sparse (one row per unique value, sorted ascending). NaN entries
/// excluded both from the row set and the percentage denominator.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `[value, count, percent]` matrix.
Value tabulate(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cholesky-like factor of a covariance matrix
/// (`[T, p] = cholcov(SIGMA)`).
///
/// Returns `T` with `T'·T == SIGMA` and a non-PD count `p`:
/// - PD (n×n):       `T` upper-triangular `n × n`, `p = 0`
/// - PSD (rank `r < n`): `T` is `r × n`, `p = 0`
/// - indefinite:      `T` empty `0 × 0`, `p = #(eig <= -tol)`
///
/// @param SIGMA  Symmetric covariance matrix.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(T, p)` pair.
std::pair<Value, Value>
cholcov(const Value &SIGMA, std::pmr::memory_resource *mr = nullptr);

/// @brief Contingency table (`[T, chi2, p] = crosstab(x, y_opt)`).
///
/// Single-arg form (pass `Value::Empty` for `y_opt`): `T` is a column
/// vector of frequency counts of unique `x`. Two-arg: `T(i, j)` counts
/// pairs `(x_k, y_k)` where `x_k = unique_x(i)` and `y_k = unique_y(j)`;
/// `chi2` and `p` give a χ² test of independence. Numeric input only
/// in v1; cell / string deferred.
///
/// @param x      Input data column.
/// @param y_opt  Second column (`Value::Empty` for single-arg form).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(T, chi2, p)` — table, χ² statistic, p-value.
std::tuple<Value, double, double>
crosstab(const Value &x, const Value &y_opt = Value::Empty,
         std::pmr::memory_resource *mr = nullptr);

/// @brief Per-group statistics (`grpstats(X, group, fn_names)`).
///
/// Returns one Value per requested aggregator. Default (`fn_names`
/// empty) is `{ "mean" }`. Supported: `mean`, `std`, `sum`, `numel`,
/// `min`, `max`, `var`, `sem`. Each output is `Ng × C` where `Ng`
/// is the number of unique non-NaN groups and `C = size(X, 2)`.
///
/// @param X         `N × C` data matrix.
/// @param group     `N × 1` group labels.
/// @param fn_names  Aggregator names; empty → defaults to `{ "mean" }`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Vector of result Values, one per aggregator.
std::vector<Value>
grpstats(const Value &X, const Value &group, const std::vector<std::string> &fn_names, std::pmr::memory_resource *mr = nullptr);

/// @brief Nearest correlation matrix (`Y = nearcorr(A)`).
///
/// Higham's (2002) alternating-projections algorithm with Dykstra's
/// correction. Returns a symmetric, PSD, unit-diagonal matrix closest
/// (in Frobenius norm) to `A`. Defaults: `tol = 1e-10`, `maxits = 100`.
/// `tolconv` / `maxits` name-value parameters deferred.
///
/// @param A   Input symmetric matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Nearest valid correlation matrix.
Value nearcorr(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
