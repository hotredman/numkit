// libs/builtin/include/numkit/builtin/math/elementary/discrete.hpp
//
// Discrete-math builtins:
//   - Set operations:        unique, ismember, union, intersect, setdiff,
//                            histcounts, discretize
//   - Number theory:         primes, isprime, factor
//   - Combinatorics:         perms, factorial, nchoosek

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Set operations
// ════════════════════════════════════════════════════════════════════════
//
// Inputs are treated as flat (column-major) value sets — no 'rows' flag,
// no 'stable' flag. Outputs are sorted ascending. NaN handling follows
// MATLAB convention: NaN compares unequal to itself, so each NaN counts
// as distinct in unique() and is never matched in ismember.

/// unique(X) — sorted unique values as a row vector.
Value unique(Allocator &alloc, const Value &x);

/// [C, ia, ic] = unique(X)
///   C = unique values, sorted ascending.
///   ia : indices into X such that C = X(ia).
///   ic : indices into C such that X = C(ic) (in original order).
std::tuple<Value, Value, Value>
uniqueWithIndices(Allocator &alloc, const Value &x);

/// unique(X, 'rows') — unique rows of a matrix, sorted lexicographically.
Value uniqueRows(Allocator &alloc, const Value &x);

/// [C, ia, ic] = unique(X, 'rows')
std::tuple<Value, Value, Value>
uniqueRowsWithIndices(Allocator &alloc, const Value &x);

/// ismember(A, B) — for each element of A, true if found in B.
Value ismember(Allocator &alloc, const Value &a, const Value &b);

/// union/intersect/setdiff — sorted ascending, no duplicates.
Value setUnion    (Allocator &alloc, const Value &a, const Value &b);
Value setIntersect(Allocator &alloc, const Value &a, const Value &b);
Value setDiff     (Allocator &alloc, const Value &a, const Value &b);

/// histcounts(X, edges) — counts of X per bin defined by `edges`.
Value histcounts(Allocator &alloc, const Value &x, const Value &edges);

/// discretize(X, edges) — bin index (1-based); NaN for out-of-range elements.
Value discretize(Allocator &alloc, const Value &x, const Value &edges);

// ════════════════════════════════════════════════════════════════════════
// Number theory
// ════════════════════════════════════════════════════════════════════════

/// primes(n) — row vector of all primes ≤ n. n < 2 → empty 1×0 row.
/// Sieve of Eratosthenes; output type DOUBLE (matches MATLAB).
Value primes(Allocator &alloc, double n);

/// isprime(x) — element-wise primality. LOGICAL output, same shape
/// as x. Non-integer / negative / NaN entries → false.
Value isprime(Allocator &alloc, const Value &x);

/// factor(n) — prime-factor decomposition. Returns a row vector of
/// primes whose product is n (with multiplicity). MATLAB conventions:
///   factor(0) → [0], factor(1) → [1].
Value factor(Allocator &alloc, double n);

// ════════════════════════════════════════════════════════════════════════
// Combinatorics
// ════════════════════════════════════════════════════════════════════════

/// perms(v) — every permutation of v as a (n!)×n matrix in reverse-lex
/// order. Caps at numel(v) ≤ 11 (12! is too large to materialize).
Value perms(Allocator &alloc, const Value &v);

/// factorial(n) — element-wise factorial. n entries must be non-negative
/// integers; n > 170 returns Inf. Output is DOUBLE, same shape as n.
Value factorial(Allocator &alloc, const Value &n);

/// nchoosek(n, k) — binomial coefficient C(n, k). Both arguments are
/// non-negative integer scalars with k ≤ n. Vector-input form (k-
/// combinations) is not yet supported.
Value nchoosek(Allocator &alloc, double n, double k);

} // namespace numkit::builtin
