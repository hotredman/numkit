// libs/builtin/include/numkit/builtin/math/elementary/exponents.hpp
//
// Exponentials and logarithms. exp/log are SIMD-backed; the rest are
// scalar wrappers around <cmath>.
//
// `hint` (when non-null and a uniquely-owned heap double of matching
// shape) is reused as the result buffer instead of allocating a fresh
// one. After the call `*hint` is empty. Saves the per-call N-element
// mr at large N where Windows HeapAlloc spills into VirtualAlloc /
// page commit. When hint is nullptr or doesn't match, the function
// falls back to the standard mr path and `*hint` is left untouched.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

Value sqrt(std::pmr::memory_resource *mr, const Value &x);
Value exp(std::pmr::memory_resource *mr, const Value &x, Value *hint = nullptr);
Value log(std::pmr::memory_resource *mr, const Value &x, Value *hint = nullptr);
Value log2(std::pmr::memory_resource *mr, const Value &x);
Value log10(std::pmr::memory_resource *mr, const Value &x);

/// expm1(x) — exp(x) - 1, accurate near zero.
Value expm1(std::pmr::memory_resource *mr, const Value &x);

/// log1p(x) — log(1 + x), accurate near zero.
Value log1p(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
