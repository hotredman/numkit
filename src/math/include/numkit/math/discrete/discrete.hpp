// toolboxes/builtin/include/numkit/builtin/math/discrete/discrete.hpp
//
// Discrete-math builtins: set operations, number theory, combinatorics.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::math {

/// @file
/// @brief Discrete-math builtins.
///
/// **Set ops** treat inputs as flat (column-major) value sets — no
/// `'rows'` flag (`uniqueRows` is a separate function), no `'stable'`
/// flag. Outputs are sorted ascending. NaN handling: NaN compares
/// unequal to itself, so each NaN counts as
/// distinct in `unique()` and is never matched in `ismember`.

// ── Set operations ───────────────────────────────────────────────────

/// @brief Sorted unique values (`c = unique(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of unique values (ascending).
/// @see uniqueWithIndices, uniqueRows
///
/// `stable` (MATLAB 'stable' setOrder) keeps values in first-occurrence
/// order instead of sorting. Default false = 'sorted'.
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Unique with index outputs
/// (`[C, ia, ic] = unique(X)`).
///
/// - `C`  : unique values, sorted ascending.
/// - `ia` : indices into `X` such that `C = X(ia)`.
/// - `ic` : indices into `C` such that `X = C(ic)` (in original order).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @param stable  MATLAB 'stable' setOrder (first-occurrence order).
/// @param last    MATLAB 'last' — `ia` selects the LAST occurrence of each
///                value (sorted order). Default false = first occurrence.
/// @return    `(C, ia, ic)` triple.
std::tuple<Value, Value, Value>
uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr,
                  bool stable = false, bool last = false);

/// @brief Unique rows of a matrix (`C = unique(X, 'rows')`).
///
/// @param x       Input matrix.
/// @param mr      Memory resource (nullptr → process default).
/// @param stable  MATLAB 'stable' setOrder: keep rows in first-occurrence
///                order instead of lexicographic sort. Default false.
/// @return        Matrix of unique rows.
/// @see uniqueRowsWithIndices
Value uniqueRows(const Value &x, std::pmr::memory_resource *mr = nullptr,
                 bool stable = false);

/// @brief Unique rows with index outputs
/// (`[C, ia, ic] = unique(X, 'rows')`).
///
/// @param x       Input matrix.
/// @param mr      Memory resource (nullptr → process default).
/// @param stable  MATLAB 'stable' setOrder (see uniqueRows). With 'stable',
///                `ia` indexes first occurrences in appearance order. Default
///                false = lexicographic sort.
/// @param last    MATLAB 'last' — `ia` selects the LAST occurrence of each
///                row (sorted order). Default false = first occurrence.
/// @return        `(C, ia, ic)` triple.
std::tuple<Value, Value, Value>
uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr,
                      bool stable = false, bool last = false);

/// @brief Membership test (`tf = ismember(A, B)`).
///
/// For each element of `A`, returns `true` if found in `B`.
///
/// @param a   Query array.
/// @param b   Reference set.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, same shape as `a`.
Value ismember(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set union (`y = union(a, b)`).
///
/// Sorted ascending, no duplicates.
///
/// @param a   First set.
/// @param b   Second set.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Union as row vector.
/// @see setIntersect, setDiff
Value setUnion(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Set intersection (`y = intersect(a, b)`).
///
/// @param a   First set.
/// @param b   Second set.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Intersection as row vector.
/// @see setUnion, setDiff
Value setIntersect(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Set difference (`y = setdiff(a, b)`).
///
/// Elements of `a` not in `b`.
///
/// @param a   First set.
/// @param b   Second set.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `a \ b` as row vector.
/// @see setUnion, setIntersect
Value setDiff(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);

/// @brief Histogram bin counts (`n = histcounts(x, edges)`).
///
/// @param x      Data array.
/// @param edges  Bin edges (length `nbins + 1`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of per-bin counts (length `nbins`).
/// @see discretize
Value histcounts(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

/// @brief Bin-count normalization mode (`'Normalization'` of `histcounts`).
enum class HistNorm {
    Count,         ///< raw counts (default)
    Probability,   ///< count / numel(x)
    CountDensity,  ///< count / binwidth
    Pdf,           ///< count / (numel(x) · binwidth)
    CumCount,      ///< cumulative count
    Cdf            ///< cumulative count / numel(x)
};

/// @brief Histogram bin counts with a normalization mode
/// (`n = histcounts(x, edges, 'Normalization', mode)`).
///
/// `Count` returns the raw count row vector (identical to the 2-arg
/// overload). The other modes scale the count vector as documented in
/// `HistNorm`. The normalization total is `numel(x)` — out-of-range values
/// are excluded from the bins but still counted in the total, matching
/// MATLAB R2025b.
///
/// @param x      Data array.
/// @param edges  Bin edges (length `nbins + 1`).
/// @param norm   Normalization mode.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Row vector of normalized bin values (length `nbins`).
/// @see histcounts, discretize
Value histcounts(const Value &x, const Value &edges, HistNorm norm,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Automatic bin-selection method (`'BinMethod'` of `histcounts`).
enum class HistBinMethod { Auto, Scott, Fd, Integers, Sturges, Sqrt };

/// @brief Options for automatic histogram bin-edge selection.
/// Mirrors MATLAB `histcounts` auto-binning: pick by `method` (default `Auto`),
/// or a fixed bin count (`hasNumBins`), or a fixed `binWidth` (`hasBinWidth`),
/// optionally restricted to `[limLo, limHi]` (`hasBinLimits`).
struct HistBinSpec {
    HistBinMethod method       = HistBinMethod::Auto;
    bool          hasNumBins   = false;  double numBins  = 0.0;
    bool          hasBinWidth  = false;  double binWidth = 0.0;
    bool          hasBinLimits = false;  double limLo = 0.0, limHi = 0.0;
};

/// @brief Choose histogram bin edges automatically (MATLAB `binpicker` rules).
///
/// Reproduces MATLAB R2025b `histcounts` auto-binning: the default `Auto` method
/// uses unit-integer bins for integer-valued data of range ≤ 50 and Scott's
/// normal-reference rule otherwise; `NumBins` / `BinWidth` / `BinLimits` and the
/// `Scott`/`Fd`/`Integers`/`Sturges`/`Sqrt` methods follow the documented rules.
/// Non-finite values are excluded from the data range. Returns a row vector of
/// edges (length `nbins + 1`).
///
/// @param x    Data array.
/// @param spec Bin-selection options.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Row vector of bin edges.
/// @see histcounts
Value histcountsAutoEdges(const Value &x, const HistBinSpec &spec,
                          std::pmr::memory_resource *mr = nullptr);

/// @brief Bin assignment (`bin = discretize(x, edges)`).
///
/// 1-based bin index per element; `NaN` for out-of-range entries.
///
/// @param x      Data array.
/// @param edges  Bin edges.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Bin index array, same shape as `x`.
/// @see histcounts
Value discretize(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

/// @brief Legacy histogram bin counts (`n = histc(x, edges)`).
///
/// Unlike `histcounts`, `n` has `length(edges)` entries: bin `k` counts
/// `edges(k) <= x < edges(k+1)` for `k = 1..end-1`, and `n(end)` counts
/// values exactly equal to `edges(end)`. A row vector yields a row;
/// a column vector or matrix is processed column-wise (`length(edges) ×
/// cols`).
///
/// @param x      Data array (vector or matrix).
/// @param edges  Ascending bin edges.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Bin counts (see shape rule above).
/// @see histcounts, discretize
Value histc(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);

// ── Number theory ────────────────────────────────────────────────────

/// @brief Primes up to `n` (`p = primes(n)`).
///
/// Sieve of Eratosthenes. `n < 2` → empty `1 × 0` row.
///
/// @param n   Upper bound.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of primes (DOUBLE).
/// @see isprime, factor
Value primes(double n, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise primality test (`tf = isprime(x)`).
///
/// Non-integer / negative / NaN entries return `false`.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, same shape as `x`.
/// @see primes
Value isprime(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Prime factorisation (`f = factor(n)`).
///
/// Returns the row of primes whose product is `n` (with multiplicity).
/// Edge cases: `factor(0) → [0]`, `factor(1) → [1]`.
///
/// @param n   Scalar to factor.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row of prime factors.
Value factor(double n, std::pmr::memory_resource *mr = nullptr);

// ── Combinatorics ────────────────────────────────────────────────────

/// @brief All permutations (`P = perms(v)`).
///
/// Returns `n! × n` matrix of all permutations of `v` in reverse-lex
/// order. Caps at `numel(v) ≤ 11` (`12!` is too large to materialise).
///
/// @param v   Input vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Permutation matrix.
/// @throws Error  `numel(v) > 11` (`m:perms:tooBig`).
/// @see nchoosek
Value perms(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise factorial (`y = factorial(n)`).
///
/// `n` entries must be non-negative integers; `n > 170` returns `Inf`.
///
/// @param n   Input array (non-negative integers).
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array of factorials, same shape as `n`.
Value factorial(const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial coefficient (`c = nchoosek(n, k)`).
///
/// Both arguments are non-negative integer scalars with `k <= n`.
/// Vector-input form (`k`-combinations) is not yet supported.
///
/// @param n   Population size.
/// @param k   Subset size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar `C(n, k)`.
/// @see perms
Value nchoosek(double n, double k, std::pmr::memory_resource *mr = nullptr);

// ── Tolerance-aware set operations ────────────────────────────────────

/// @brief Set symmetric difference (`c = setxor(a, b)`).
///
/// Sorted unique elements that appear in exactly one of `a` or `b`. This is
/// the element-wise form; the script-level `'rows'` option and the
/// `(ia, ib)` index outputs are handled by the adapter, not this entry.
///
/// @param a   First input array.
/// @param b   Second input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Sorted unique symmetric difference.
Value setxor(const Value &a, const Value &b,
             std::pmr::memory_resource *mr = nullptr);

/// @brief True if every element is distinct (`tf = allunique(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Logical scalar (`true` when `x` has no repeated values).
/// @see numunique
Value allunique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Count of distinct elements (`n = numunique(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar number of unique values.
/// @see allunique
Value numunique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tolerance-aware set membership (`tf = ismembertol(a, s, tol)`).
///
/// Logical mask (shape of `a`) marking each element of `a` that lies within
/// `tol` of some element of `s`.
///
/// @param a    Query array.
/// @param s    Set array.
/// @param tol  Absolute tolerance (default `1e-6`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Logical mask shaped like `a`.
/// @see uniquetol
Value ismembertol(const Value &a, const Value &s, double tol = 1e-6,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Tolerance-aware unique (`u = uniquetol(x, tol)`).
///
/// Unique values of `x`, collapsing entries that lie within `tol` of one
/// another.
///
/// @param x    Input array.
/// @param tol  Absolute tolerance (default `1e-6`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Sorted, tolerance-collapsed unique values.
/// @see ismembertol
Value uniquetol(const Value &x, double tol = 1e-6,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::math
