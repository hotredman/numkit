// libs/builtin/include/numkit/m/builtin/MStdSetOps.hpp
//
// Phase-8: search and set operations on flat arrays.
//
// Inputs are treated as flat (column-major) value sets — no 'rows'
// flag, no 'stable' flag. Outputs are sorted ascending. NaN handling
// follows MATLAB convention: NaN compares unequal to itself, so each
// NaN counts as distinct in unique() and is never matched in ismember.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

/// unique(X) — sorted unique values as a row vector.
MValue unique(Allocator &alloc, const MValue &x);

/// [C, ia, ic] = unique(X)
///   C = unique values, sorted ascending.
///   ia : indices into X such that C = X(ia).
///   ic : indices into C such that X = C(ic) (in original order).
std::tuple<MValue, MValue, MValue>
uniqueWithIndices(Allocator &alloc, const MValue &x);

/// unique(X, 'rows') — unique rows of a matrix, sorted lexicographically.
/// Output shape is K×C where K is the number of distinct rows. Rows
/// containing NaN are kept distinct (each its own slot, appended after
/// the sorted non-NaN rows in input order — same convention as the flat
/// unique on NaN scalars).
MValue uniqueRows(Allocator &alloc, const MValue &x);

/// [C, ia, ic] = unique(X, 'rows')
///   C  : unique rows, lex-sorted (NaN rows appended last in input order).
///   ia : 1-based row indices into X such that C = X(ia, :).
///   ic : 1-based row indices into C such that X = C(ic, :).
std::tuple<MValue, MValue, MValue>
uniqueRowsWithIndices(Allocator &alloc, const MValue &x);

/// ismember(A, B) — for each element of A, true if found in B.
/// Output shape matches A. Output type LOGICAL.
MValue ismember(Allocator &alloc, const MValue &a, const MValue &b);

/// union/intersect/setdiff — sorted ascending, no duplicates.
MValue setUnion    (Allocator &alloc, const MValue &a, const MValue &b);
MValue setIntersect(Allocator &alloc, const MValue &a, const MValue &b);
MValue setDiff     (Allocator &alloc, const MValue &a, const MValue &b);

/// histcounts(X, edges) — counts of X falling in each bin defined by
/// `edges` (length N+1, ascending). Bins are [edges(i), edges(i+1))
/// except the last bin which is closed [edges(N), edges(N+1)].
MValue histcounts(Allocator &alloc, const MValue &x, const MValue &edges);

/// discretize(X, edges) — bin index (1-based) for each element of X.
/// Returns NaN for elements outside [edges(1), edges(end)].
MValue discretize(Allocator &alloc, const MValue &x, const MValue &edges);

} // namespace numkit::m::builtin
