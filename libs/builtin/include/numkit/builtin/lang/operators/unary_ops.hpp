// libs/builtin/include/numkit/builtin/lang/operators/unary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Unary arithmetic ─────────────────────────────────────────────────
/// -x with type-preserving semantics. Char/logical promote to double.
/// Signed integers saturate at type min; unsigned becomes zero.
Value uminus(std::pmr::memory_resource *mr, const Value &x);

/// +x — identity (returns x unchanged). Provided for operator symmetry.
Value uplus(std::pmr::memory_resource *mr, const Value &x);

// ── Logical ──────────────────────────────────────────────────────────
/// ~x — elementwise logical NOT. Non-zero → 0, zero → 1. Returns logical.
Value logicalNot(std::pmr::memory_resource *mr, const Value &x);

// ── Transposes ───────────────────────────────────────────────────────
/// x' — conjugate transpose. For complex matrices, conjugates each element.
/// Throws Error on 3D input. Supports DOUBLE and COMPLEX.
Value ctranspose(std::pmr::memory_resource *mr, const Value &x);

/// x.' — non-conjugate transpose. For complex matrices, does NOT conjugate.
/// Throws Error on 3D input. Supports DOUBLE and COMPLEX.
/// Note: distinct from `builtin::transpose` (matrix.cpp), which is
/// DOUBLE-only and registered as the `transpose()` function call.
Value transposeNC(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
