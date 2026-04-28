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

} // namespace numkit::m::builtin
