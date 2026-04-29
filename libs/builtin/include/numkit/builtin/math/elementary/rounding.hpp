// libs/builtin/include/numkit/builtin/math/elementary/rounding.hpp
//
// Rounding and sign builtins. abs has a SIMD backend (libs/builtin/src/
// backends/MStdAbs_*.cpp); the rest are scalar wrappers.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// `hint` — see math/elementary/exponents.hpp for the contract.
Value abs(std::pmr::memory_resource *mr, const Value &x, Value *hint = nullptr);

Value floor(std::pmr::memory_resource *mr, const Value &x);
Value ceil(std::pmr::memory_resource *mr, const Value &x);
Value round(std::pmr::memory_resource *mr, const Value &x);

/// fix(x) — truncate toward zero.
Value fix(std::pmr::memory_resource *mr, const Value &x);

Value sign(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
