// libs/builtin/src/backends/MStdCompare.hpp
//
// Internal contract between MStdBinaryOps.cpp's compareImpl and the
// SIMD/portable Compare backends. Six fast-path entry points — one per
// MATLAB cmp op — that handle the two common shapes:
//   * a and b are both DOUBLE arrays of identical 2D/3D shape, OR
//   * one operand is a DOUBLE scalar and the other is a DOUBLE array.
//
// Returns an MValue::isUnset() sentinel when the inputs don't fit the
// fast path (LOGICAL/integer/complex operand, broadcasting between two
// non-scalar operands of different shape, etc.) — caller falls back to
// the generic scalar `compareImpl` loop.
//
// Phase P1.5 of project_perf_optimization_plan.md.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

MValue eqFast(const MValue &a, const MValue &b);
MValue neFast(const MValue &a, const MValue &b);
MValue ltFast(const MValue &a, const MValue &b);
MValue gtFast(const MValue &a, const MValue &b);
MValue leFast(const MValue &a, const MValue &b);
MValue geFast(const MValue &a, const MValue &b);

} // namespace numkit::m::builtin
