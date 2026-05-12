// libs/builtin/include/numkit/builtin/language/operators/unary_ops.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

//// -x with type-preserving semantics. Char/logical promote to double.
//// Signed integers saturate at type min; unsigned becomes zero.
Value uminus(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// +x — identity (returns x unchanged). Provided for operator symmetry.
Value uplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// ~x — elementwise logical NOT. Non-zero → 0, zero → 1. Returns logical.
Value logicalNot(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// x' — conjugate transpose. For complex matrices, conjugates each element.
//// Throws Error on 3D input. Supports DOUBLE and COMPLEX.
Value ctranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// x.' — non-conjugate transpose. For complex matrices, does NOT conjugate.
/// Throws Error on 3D input. Supports DOUBLE and COMPLEX.
/// Note: distinct from `builtin::transpose` (matrix.cpp), which is
/// DOUBLE-only and registered as the `transpose()` function call.
Value transposeNC(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
