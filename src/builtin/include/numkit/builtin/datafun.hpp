// src/builtin/include/numkit/builtin/datafun.hpp
//
// Pure C++ Data analysis, reductions, random number generation, set operations.
#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <numkit/value/value.hpp>
#include <numkit/ops/rng.hpp>

namespace numkit::builtin {

using ::numkit::ops::RngContext;
using ::numkit::ops::rand;
using ::numkit::ops::randn;
using ::numkit::ops::randND;
using ::numkit::ops::randnND;
using ::numkit::ops::randi;
using ::numkit::ops::randperm;

/// @file
/// @brief Data analysis, reductions, search, set operations, and random distributions.
///
/// Provides a clean, engine-free C++ API for reduction operators (sum, mean, min, max, prod),
/// pseudo-random number distributions, and set operations.

// ── Reductions ──────────────────────────────────────────────────────────────

// ── Reductions ──────────────────────────────────────────────────────────────

/// @brief Sum of array elements along first non-singleton dimension (`sum(x)`).
/// @param x Input array.
/// @param mr Memory resource for allocations.
/// @return Sum array.
/// @see prod, mean, cumsum
Value sum(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Sum of array elements along specified dimension (`sum(x, dim)`).
/// @param x Input array.
/// @param dim Dimension along which to sum (1-based, 0 = first non-singleton).
/// @param mr Memory resource.
/// @return Sum array.
/// @see prod, mean, cumsum
Value sum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Product of array elements along first non-singleton dimension (`prod(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Product array.
/// @see sum, mean, cumprod
Value prod(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Product of array elements along specified dimension (`prod(x, dim)`).
/// @param x Input array.
/// @param dim Dimension along which to operate (1-based, 0 = first non-singleton).
/// @param mr Memory resource.
/// @return Product array.
/// @see sum, mean, cumprod
Value prod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean (arithmetic average) of array elements (`mean(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Mean array.
/// @see sum, median
Value mean(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean (arithmetic average) of array elements along dimension (`mean(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension (1-based, 0 = first non-singleton).
/// @param mr Memory resource.
/// @return Mean array.
/// @see sum, median
Value mean(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array. Returns tuple `(values, indices)` (`[M, I] = max(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Tuple containing `{max_values, max_indices}`.
/// @see min, maxOmitNan
std::tuple<Value, Value> max(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array along specified dimension (`[M, I] = max(x, [], dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Tuple containing `{max_values, max_indices}`.
std::tuple<Value, Value> max(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Element-wise maximum of two arrays (`max(a, b)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Element-wise maximum array.
Value max(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array along dimension omitting NaN values (`max(x, [], dim, 'omitnan')`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Tuple containing `{max_values, max_indices}`.
std::tuple<Value, Value> maxOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Element-wise maximum of two arrays omitting NaNs (`max(a, b, 'omitnan')`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Element-wise maximum.
Value maxOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array. Returns tuple `(values, indices)` (`[M, I] = min(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Tuple containing `{min_values, min_indices}`.
/// @see max, minOmitNan
std::tuple<Value, Value> min(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array along dimension (`[M, I] = min(x, [], dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Tuple containing `{min_values, min_indices}`.
std::tuple<Value, Value> min(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Element-wise minimum of two arrays (`min(a, b)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Element-wise minimum.
Value min(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array along dimension omitting NaNs (`min(x, [], dim, 'omitnan')`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Tuple containing `{min_values, min_indices}`.
std::tuple<Value, Value> minOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Element-wise minimum of two arrays omitting NaNs (`min(a, b, 'omitnan')`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Element-wise minimum.
Value minOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative sum of array elements (`cumsum(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Cumulative sum array.
/// @see sum, cumprod
Value cumsum(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative sum of array elements along specified dimension (`cumsum(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative sum array.
Value cumsum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative product of array elements (`cumprod(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Cumulative product array.
/// @see prod, cumsum
Value cumprod(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative product of array elements along specified dimension (`cumprod(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative product array.
Value cumprod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative maximum of array elements (`cummax(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Cumulative maximum array.
/// @see max, cummin
Value cummax(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative maximum of array elements along dimension (`cummax(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative maximum array.
Value cummax(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative minimum of array elements (`cummin(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Cumulative minimum array.
/// @see min, cummax
Value cummin(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative minimum of array elements along dimension (`cummin(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param mr Memory resource.
/// @return Cumulative minimum array.
Value cummin(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Differences between adjacent elements (`diff(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return First-order differences array.
Value diff(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Nth-order differences between elements along dimension (`diff(x, n, dim)`).
/// @param x Input array.
/// @param n Difference order (default 1).
/// @param dim Operating dimension (default 0 for first non-singleton).
/// @param mr Memory resource.
/// @return Nth-order difference array.
Value diff(const Value &x, int n, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief True if any element along dimension is nonzero (`any(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension (0 for first non-singleton).
/// @param mr Memory resource.
/// @return Logical array.
Value anyOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief True if all elements along dimension are nonzero (`all(x, dim)`).
/// @param x Input array.
/// @param dim Operating dimension (0 for first non-singleton).
/// @param mr Memory resource.
/// @return Logical array.
Value allOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Random Number Generation ────────────────────────────────────────────────
// See RngContext-taking declarations re-exported at header top.

/// @brief Binning method for histograms.
enum class HistBinMethod {
    Auto,
    Scott,
    Fd,
    Integers,
    Sturges,
    Sqrt
};

/// @brief Histogram normalization type.
enum class HistNorm {
    Count,
    Probability,
    CountDensity,
    Pdf,
    CumCount,
    Cdf
};

/// @brief Specification for automatic histogram bin estimation.
struct HistBinSpec {
    HistBinMethod method = HistBinMethod::Auto;
    bool hasNumBins = false;
    double numBins = 0.0;
    bool hasBinWidth = false;
    double binWidth = 0.0;
    bool hasBinLimits = false;
    double limLo = 0.0;
    double limHi = 0.0;
};

// ── Set Operations & Discrete Analysis ──────────────────────────────────────

/// @brief Unique values in array (`unique(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @param stable True to preserve original order, false to return sorted values.
/// @return Vector of unique values.
/// @see ismember, setUnion
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Unique rows of 2-D matrix (`unique(x, 'rows')`).
/// @param x 2-D matrix.
/// @param mr Memory resource.
/// @param stable True to preserve order.
/// @return Matrix containing unique rows.
Value uniqueRows(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Unique values with index mappings (`[C, ia, ic] = unique(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @param stable True for occurrence ordering.
/// @param last True to return last occurrence index instead of first.
/// @return Tuple containing `{C, ia, ic}`.
std::tuple<Value, Value, Value> uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false, bool last = false);

/// @brief Unique rows with index mappings (`[C, ia, ic] = unique(x, 'rows')`).
/// @param x 2-D matrix.
/// @param mr Memory resource.
/// @param stable True for occurrence ordering.
/// @param last True to return last occurrence index.
/// @return Tuple containing `{C, ia, ic}`.
std::tuple<Value, Value, Value> uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false, bool last = false);

/// @brief Tests if array elements are members of set (`ismember(a, b)`).
/// @param a Query array.
/// @param b Set array.
/// @param mr Memory resource.
/// @return Logical array matching shape of `a`.
/// @see unique, setIntersect
Value ismember(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests membership for complex/general types with location indices (`[lia, loc] = ismember(...)`).
/// @param a Query array.
/// @param b Set array.
/// @param wantLoc True to return matched index locations.
/// @param mr Memory resource.
/// @return Pair `{lia, loc}`.
std::pair<Value, Value> ismemberComplex(const Value &a, const Value &b, bool wantLoc, std::pmr::memory_resource *mr);

/// @brief Set union of two arrays (`union(a, b)`).
/// @param a First set array.
/// @param b Second set array.
/// @param mr Memory resource.
/// @param stable True to keep occurrence order.
/// @return Combined unique values.
/// @see setIntersect, setDiff
Value setUnion(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Set intersection of two arrays (`intersect(a, b)`).
/// @param a First set array.
/// @param b Second set array.
/// @param mr Memory resource.
/// @param stable True to keep order.
/// @return Common unique values.
/// @see setUnion, setDiff
Value setIntersect(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Set difference (`setdiff(a, b)`).
/// @param a First set array.
/// @param b Second set array.
/// @param mr Memory resource.
/// @param stable True to keep order.
/// @return Elements in `a` not in `b`.
/// @see setUnion, setxor
Value setDiff(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Set exclusive OR (`setxor(a, b)`).
/// @param a First set array.
/// @param b Second set array.
/// @param mr Memory resource.
/// @return Elements in either `a` or `b` but not both.
/// @see setUnion, setIntersect
Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Groups numeric data into bins (`discretize(x, edges)`).
/// @param x Data array.
/// @param edges Bin edges vector.
/// @param mr Memory resource.
/// @return Bin indices array.
Value discretize(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

/// @brief Histogram count of data into bins (`histc(x, edges)`).
/// @param x Data array.
/// @param edges Bin edges vector.
/// @param mr Memory resource.
/// @return Bin counts array.
Value histc(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes histogram bin counts (`histcounts(x, edges)`).
/// @param x Data array.
/// @param edges Bin edges vector.
/// @param mr Memory resource.
/// @return Count of values in each bin.
Value histcounts(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes normalized histogram bin values (`histcounts(x, edges, norm)`).
/// @param x Data array.
/// @param edges Bin edges vector.
/// @param norm Normalization scheme (`Count`, `Probability`, `Pdf`, etc.).
/// @param mr Memory resource.
/// @return Normalized bin values.
Value histcounts(const Value &x, const Value &edges, HistNorm norm, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes histogram with automatic bin edges selection (`histcounts(x, spec)`).
/// @param x Data array.
/// @param spec Bin specification struct.
/// @param mr Memory resource.
/// @return Bin counts array.
Value histcountsAutoEdges(const Value &x, const HistBinSpec &spec, std::pmr::memory_resource *mr = nullptr);

/// @brief True if all elements in array are unique (`allunique(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical scalar.
Value allunique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number of unique elements in array (`numunique(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Count of distinct values.
Value numunique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Membership test within numerical tolerance (`ismembertol(a, s, tol)`).
/// @param a Query array.
/// @param s Set array.
/// @param tol Relative/absolute tolerance.
/// @param mr Memory resource.
/// @return Logical array.
Value ismembertol(const Value &a, const Value &s, double tol = 1e-6, std::pmr::memory_resource *mr = nullptr);

/// @brief Unique elements within numerical tolerance (`uniquetol(x, tol)`).
/// @param x Input array.
/// @param tol Relative/absolute tolerance.
/// @param mr Memory resource.
/// @return Unique elements within tolerance.
Value uniquetol(const Value &x, double tol = 1e-6, std::pmr::memory_resource *mr = nullptr);

// ── Grouping Operations ─────────────────────────────────────────────────────

/// @brief Result of `findgroups` — `[G, ID]`.
struct FindgroupsResult {
    Value G;   ///< Group index per element (shape of input; NaN = missing).
    Value ID;  ///< Column of unique non-NaN values (sorted ascending).
};

/// @brief Assign 1-based group IDs (`[G, ID] = findgroups(g)`).
///
/// `G[i]` is the group ID of `g[i]`, based on the sorted-unique order of the
/// finite values of `g`; `ID` is the column of those unique values. `NaN`
/// entries are treated as missing (`G[i] = NaN`, excluded from `ID`),
/// matching MATLAB R2025b. `G` takes the shape of `g`.
///
/// @param g Grouping variable (numeric / logical / char).
/// @param mr Memory resource.
/// @return @ref FindgroupsResult `{ G, ID }`.
/// @see groupcounts, groupsummary, grouptransform
FindgroupsResult findgroups(const Value &g, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of `groupcounts` — `[C, GR, P]`.
struct GroupcountsResult {
    Value C;   ///< Count per group (trailing NaN bucket if `g` has any NaN).
    Value GR;  ///< Group representatives (sorted unique; NaN trailing).
    Value P;   ///< Percentage `100 * count / total` per group.
};

/// @brief Count elements per group (`[C, GR, P] = groupcounts(g)`).
///
/// `C` is the per-group element count over the sorted-unique values of `g`;
/// `GR` the matching representatives; `P` the percentages. `NaN` entries
/// form a single trailing bucket (matching MATLAB R2025b).
///
/// @param g Grouping variable.
/// @param mr Memory resource.
/// @return @ref GroupcountsResult `{ C, GR, P }`.
/// @see findgroups, groupsummary
GroupcountsResult groupcounts(const Value &g, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of the array form of `groupsummary` — `[B, BG, BC]`.
struct GroupsummaryResult {
    Value B;   ///< `nGroups x cols(A)` per-group, per-column reduction.
    Value BG;  ///< Group representatives (sorted unique; NaN trailing).
    Value BC;  ///< Element count per group.
};

/// @brief Per-group reduction (`[B, BG, BC] = groupsummary(A, G, method)`).
///
/// Groups the rows of `A` (length-`size(A,1)` grouping vector `G`) and
/// applies `method` per group, per column. Supported `method` strings:
/// `"sum"`, `"mean"`, `"median"`, `"max"`, `"min"`, `"std"`, `"var"`,
/// `"numunique"`, `"nnz"`, `"mode"`, `"all"`, `"any"`. `NaN` group values
/// form a single trailing bucket.
///
/// @param A Column vector or matrix (DOUBLE).
/// @param G Grouping vector, length `size(A,1)`.
/// @param method Reduction method string.
/// @param mr Memory resource.
/// @return @ref GroupsummaryResult `{ B, BG, BC }`.
/// @see findgroups, groupcounts, grouptransform
GroupsummaryResult groupsummary(const Value &A, const Value &G,
                                const std::string &method,
                                std::pmr::memory_resource *mr = nullptr);

/// @brief Group transformation (`grouptransform(A, G, method)`).
///
/// Supported string methods: `"meancenter"`, `"zscore"`, `"norm"`,
/// `"rescale"`, `"meanfill"`, `"linearfill"`.
///
/// @param A Column vector or matrix (DOUBLE).
/// @param G Grouping vector, length `size(A,1)`.
/// @param method Transformation method string.
/// @param mr Memory resource.
/// @return Transformed array with same shape as @p A.
/// @see groupsummary, findgroups
Value grouptransform(const Value &A, const Value &G,
                     const std::string &method,
                     std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
