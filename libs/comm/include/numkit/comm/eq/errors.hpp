// libs/comm/include/numkit/comm/eq/errors.hpp
//
// Bit/symbol error metrics (biterr, symerr) — Communications Toolbox.

#pragma once

#include <memory_resource>
#include <utility>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @brief Bit-error count and rate (`[number, ratio] = biterr(x, y, k)`).
///
/// Counts the differing bits between the same-shape non-negative integer
/// arrays `x` and `y`, treating each element as a `k`-bit symbol. `k <= 0`
/// (default) auto-selects the smallest bit width covering all values.
/// Returns `{ number, ratio }` where `ratio = number / (numel * k)`.
/// (MATLAB's 3rd `individual` output is a v1 gap.)
///
/// @param x,y  Same-numel non-negative integer arrays.
/// @param k    Bits per symbol; `<= 0` (default) → smallest covering width.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Pair `{ number, ratio }` (total bit errors and error rate).
/// @throws Error on numel mismatch, or negative inputs on the auto-`k` path.
/// @see symerr
std::pair<Value, Value> biterr(const Value &x, const Value &y, int k = 0,
                               std::pmr::memory_resource *mr = nullptr);

/// @brief Symbol-error count and rate (`[number, ratio] = symerr(x, y)`).
///
/// Counts element-wise inequalities between the same-shape arrays `x` and
/// `y`. Returns `{ count, ratio }` with `ratio = count / numel`.
///
/// @param x,y  Same-numel arrays.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Pair `{ count, ratio }`.
/// @throws Error on numel mismatch.
/// @see biterr
std::pair<Value, Value> symerr(const Value &x, const Value &y,
                               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
