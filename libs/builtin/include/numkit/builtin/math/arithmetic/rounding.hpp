// libs/builtin/include/numkit/builtin/math/arithmetic/rounding.hpp
//
// Rounding and sign builtins. abs has a SIMD backend (libs/builtin/src/
// backends/MStdAbs_*.cpp); the rest are scalar wrappers.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// `hint` — see math/elementary/exponents.hpp for the contract.
Value abs(const Value &x, Value *hint = nullptr, std::pmr::memory_resource *mr = nullptr);

Value floor(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ceil(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value round(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// fix(x) — truncate toward zero.
Value fix(const Value &x, std::pmr::memory_resource *mr = nullptr);

Value sign(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// subplus(x) — truncated positive part: max(x, 0). NaN passes through.
Value subplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
