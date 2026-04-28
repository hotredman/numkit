// libs/builtin/include/numkit/m/builtin/math/elementary/exponents.hpp
//
// Exponentials and logarithms. exp/log are SIMD-backed; the rest are
// scalar wrappers around <cmath>.
//
// `hint` (when non-null and a uniquely-owned heap double of matching
// shape) is reused as the result buffer instead of allocating a fresh
// one. After the call `*hint` is empty. Saves the per-call N-element
// alloc at large N where Windows HeapAlloc spills into VirtualAlloc /
// page commit. When hint is nullptr or doesn't match, the function
// falls back to the standard alloc path and `*hint` is left untouched.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

MValue sqrt(Allocator &alloc, const MValue &x);
MValue exp(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue log(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue log2(Allocator &alloc, const MValue &x);
MValue log10(Allocator &alloc, const MValue &x);

/// expm1(x) — exp(x) - 1, accurate near zero.
MValue expm1(Allocator &alloc, const MValue &x);

/// log1p(x) — log(1 + x), accurate near zero.
MValue log1p(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
