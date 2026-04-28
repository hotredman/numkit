// libs/builtin/include/numkit/m/builtin/MStdCombinatorics.hpp
//
// Combinatorial builtins. perms now; factorial / nchoosek planned.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// perms(v) — every permutation of v as a (n!)×n matrix.
/// Rows are in reverse-lexicographic order (MATLAB convention).
/// numel(v) > 11 throws m:perms:tooLarge (12! ≈ 4.8e8 rows would
/// exhaust memory). Output type is DOUBLE; integer/logical inputs
/// are promoted.
MValue perms(Allocator &alloc, const MValue &v);

/// factorial(n) — element-wise factorial. n entries must be non-negative
/// integers; otherwise throws. n > 170 returns Inf (overflows double).
/// Output is DOUBLE, same shape as n.
MValue factorial(Allocator &alloc, const MValue &n);

/// nchoosek(n, k) — binomial coefficient C(n, k). Both arguments must
/// be non-negative integer scalars with k ≤ n; otherwise throws.
/// Computed via the symmetric multiplicative form
///   C(n, k) = ∏_{i=0..min(k,n-k)-1} (n - i) / (i + 1)
/// which avoids the n! overflow (good for n up to ~1030 before the
/// running product overflows; that's well past any realistic need).
/// The k-combinations vector form `nchoosek(v, k)` is not yet supported.
MValue nchoosek(Allocator &alloc, double n, double k);

} // namespace numkit::m::builtin
