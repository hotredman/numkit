// libs/builtin/include/numkit/builtin/math/discrete/discrete.hpp
//
// Discrete-math builtins: set operations, number theory, combinatorics.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

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
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unique with index outputs
/// (`[C, ia, ic] = unique(X)`).
///
/// - `C`  : unique values, sorted ascending.
/// - `ia` : indices into `X` such that `C = X(ia)`.
/// - `ic` : indices into `C` such that `X = C(ic)` (in original order).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(C, ia, ic)` triple.
std::tuple<Value, Value, Value>
uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unique rows of a matrix (`C = unique(X, 'rows')`).
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Matrix of unique rows (lexicographically sorted).
/// @see uniqueRowsWithIndices
Value uniqueRows(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unique rows with index outputs
/// (`[C, ia, ic] = unique(X, 'rows')`).
///
/// @param x   Input matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(C, ia, ic)` triple.
std::tuple<Value, Value, Value>
uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr);

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

} // namespace numkit::builtin
