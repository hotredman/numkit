// libs/builtin/include/numkit/m/builtin/math/elementary/discrete.hpp
//
// Discrete-math builtins:
//   - Set operations:        unique, ismember, union, intersect, setdiff,
//                            histcounts, discretize
//   - Number theory:         primes, isprime, factor
//   - Combinatorics:         perms, factorial, nchoosek

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Set operations
// ════════════════════════════════════════════════════════════════════════
//
// Inputs are treated as flat (column-major) value sets — no 'rows' flag,
// no 'stable' flag. Outputs are sorted ascending. NaN handling follows
// MATLAB convention: NaN compares unequal to itself, so each NaN counts
// as distinct in unique() and is never matched in ismember.

/// unique(X) — sorted unique values as a row vector.
MValue unique(Allocator &alloc, const MValue &x);

/// [C, ia, ic] = unique(X)
///   C = unique values, sorted ascending.
///   ia : indices into X such that C = X(ia).
///   ic : indices into C such that X = C(ic) (in original order).
std::tuple<MValue, MValue, MValue>
uniqueWithIndices(Allocator &alloc, const MValue &x);

/// unique(X, 'rows') — unique rows of a matrix, sorted lexicographically.
MValue uniqueRows(Allocator &alloc, const MValue &x);

/// [C, ia, ic] = unique(X, 'rows')
std::tuple<MValue, MValue, MValue>
uniqueRowsWithIndices(Allocator &alloc, const MValue &x);

/// ismember(A, B) — for each element of A, true if found in B.
MValue ismember(Allocator &alloc, const MValue &a, const MValue &b);

/// union/intersect/setdiff — sorted ascending, no duplicates.
MValue setUnion    (Allocator &alloc, const MValue &a, const MValue &b);
MValue setIntersect(Allocator &alloc, const MValue &a, const MValue &b);
MValue setDiff     (Allocator &alloc, const MValue &a, const MValue &b);

/// histcounts(X, edges) — counts of X per bin defined by `edges`.
MValue histcounts(Allocator &alloc, const MValue &x, const MValue &edges);

/// discretize(X, edges) — bin index (1-based); NaN for out-of-range elements.
MValue discretize(Allocator &alloc, const MValue &x, const MValue &edges);

// ════════════════════════════════════════════════════════════════════════
// Number theory
// ════════════════════════════════════════════════════════════════════════

/// primes(n) — row vector of all primes ≤ n. n < 2 → empty 1×0 row.
/// Sieve of Eratosthenes; output type DOUBLE (matches MATLAB).
MValue primes(Allocator &alloc, double n);

/// isprime(x) — element-wise primality. LOGICAL output, same shape
/// as x. Non-integer / negative / NaN entries → false.
MValue isprime(Allocator &alloc, const MValue &x);

/// factor(n) — prime-factor decomposition. Returns a row vector of
/// primes whose product is n (with multiplicity). MATLAB conventions:
///   factor(0) → [0], factor(1) → [1].
MValue factor(Allocator &alloc, double n);

// ════════════════════════════════════════════════════════════════════════
// Combinatorics
// ════════════════════════════════════════════════════════════════════════

/// perms(v) — every permutation of v as a (n!)×n matrix in reverse-lex
/// order. Caps at numel(v) ≤ 11 (12! is too large to materialize).
MValue perms(Allocator &alloc, const MValue &v);

/// factorial(n) — element-wise factorial. n entries must be non-negative
/// integers; n > 170 returns Inf. Output is DOUBLE, same shape as n.
MValue factorial(Allocator &alloc, const MValue &n);

/// nchoosek(n, k) — binomial coefficient C(n, k). Both arguments are
/// non-negative integer scalars with k ≤ n. Vector-input form (k-
/// combinations) is not yet supported.
MValue nchoosek(Allocator &alloc, double n, double k);

} // namespace numkit::m::builtin
