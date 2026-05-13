// libs/builtin/src/math/_unary_hint.hpp
//
// Internal SIMD reuse-buffer overloads for the unary functions
// sin/cos/exp/log/abs. Public 2-arg signatures live in the
// respective public headers (trig/trigonometry.hpp,
// exp_log/exponents.hpp, arithmetic/rounding.hpp).
//
// `hint` is the VM's "destination register Value" — when it's a
// uniquely-owned heap double of matching shape, the implementation
// MAY overwrite its existing buffer instead of allocating a fresh
// one. Used by NK_UNARY_ADAPTER_HINT in math/arithmetic/reductions.cpp
// to make tight loops like `z = sin(x)` reuse z's buffer.
//
// Not exposed in <numkit/builtin/...> — only the cpp side of libs/builtin
// includes this file.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

Value sin(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value cos(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value exp(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value abs(const Value &x, Value *hint, std::pmr::memory_resource *mr);

} // namespace numkit::builtin
