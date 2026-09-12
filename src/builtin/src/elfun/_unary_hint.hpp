// src/builtin/src/elfun/_unary_hint.hpp
//
// Internal SIMD reuse-buffer overloads for the unary functions sin/cos/exp/log/abs.
#pragma once

#include <memory_resource>
#include <nk/value/value.hpp>

namespace nk::builtin {

Value sin(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value cos(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value exp(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr);
Value abs(const Value &x, Value *hint, std::pmr::memory_resource *mr);

} // namespace nk::builtin
