// libs/builtin/src/lang/operators/backends/compare.hpp
//
// Internal contract between binary_ops.cpp's compareImpl and the
// SIMD/portable Compare backends. Six fast-path entry points — one per
// MATLAB cmp op — that handle the two common shapes:
//   * a and b are both DOUBLE arrays of identical 2D/3D shape, OR
//   * one operand is a DOUBLE scalar and the other is a DOUBLE array.
//
// Returns an Value::isUnset() sentinel when the inputs don't fit the
// fast path (LOGICAL/integer/complex operand, broadcasting between two
// non-scalar operands of different shape, etc.) — caller falls back to
// the generic scalar `compareImpl` loop.
//
// Phase P1.5 of project_perf_optimization_plan.md.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

Value eqFast(const Value &a, const Value &b);
Value neFast(const Value &a, const Value &b);
Value ltFast(const Value &a, const Value &b);
Value gtFast(const Value &a, const Value &b);
Value leFast(const Value &a, const Value &b);
Value geFast(const Value &a, const Value &b);

} // namespace numkit::builtin
