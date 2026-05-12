// libs/builtin/include/numkit/builtin/math/exp_log/exponents.hpp
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

Value sqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value exp(const Value &x, Value *hint = nullptr, std::pmr::memory_resource *mr = nullptr);
Value log(const Value &x, Value *hint = nullptr, std::pmr::memory_resource *mr = nullptr);
Value log2(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value log10(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// expm1(x) — exp(x) - 1, accurate near zero.
Value expm1(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// log1p(x) — log(1 + x), accurate near zero.
Value log1p(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// pow2(y)        — 2 .^ y elementwise.
/// pow2(f, e)     — f .* 2 .^ e elementwise (libc ldexp).
Value pow2(const Value &y, std::pmr::memory_resource *mr = nullptr);
Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr = nullptr);

/// realpow(x, y)  — x .^ y, but errors if any result would be complex
/// (negative base with non-integer exponent).
Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// reallog(x)     — log(x), errors on x < 0 (would be complex).
Value reallog(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// realsqrt(x)    — sqrt(x), errors on x < 0.
Value realsqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
