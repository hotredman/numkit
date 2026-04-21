// libs/builtin/include/numkit/m/builtin/MStdUnaryOps.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

// ── Unary arithmetic ─────────────────────────────────────────────────
/// -x with type-preserving semantics. Char/logical promote to double.
/// Signed integers saturate at type min; unsigned becomes zero.
MValue uminus(Allocator &alloc, const MValue &x);

/// +x — identity (returns x unchanged). Provided for operator symmetry.
MValue uplus(Allocator &alloc, const MValue &x);

// ── Logical ──────────────────────────────────────────────────────────
/// ~x — elementwise logical NOT. Non-zero → 0, zero → 1. Returns logical.
MValue logicalNot(Allocator &alloc, const MValue &x);

// ── Transposes ───────────────────────────────────────────────────────
/// x' — conjugate transpose. For complex matrices, conjugates each element.
/// Throws MError on 3D input. Supports DOUBLE and COMPLEX.
MValue ctranspose(Allocator &alloc, const MValue &x);

/// x.' — non-conjugate transpose. For complex matrices, does NOT conjugate.
/// Throws MError on 3D input. Supports DOUBLE and COMPLEX.
/// Note: distinct from `builtin::transpose` (MStdMatrix), which is
/// DOUBLE-only and registered as the `transpose()` function call.
MValue transposeNC(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
