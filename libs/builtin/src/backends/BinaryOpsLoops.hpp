// libs/builtin/src/backends/BinaryOpsLoops.hpp
//
// Private declarations for the backend-split binary-op inner loops.
// Implemented by exactly one of backends/MStdBinaryOps_{portable,simd}.cpp
// depending on NUMKIT_WITH_SIMD. Callers (MStdBinaryOps.cpp) use these
// only for the 2D same-shape DOUBLE fast path — every other shape
// (broadcasting, 3D, type mismatch) goes through the general
// elementwiseDouble() helper in MStdHelpers.hpp unchanged.

#pragma once

#include <cstddef>

namespace numkit::m::builtin::detail {

void plusLoop   (const double *a, const double *b, double *out, std::size_t n);
void minusLoop  (const double *a, const double *b, double *out, std::size_t n);
void timesLoop  (const double *a, const double *b, double *out, std::size_t n);
void rdivideLoop(const double *a, const double *b, double *out, std::size_t n);

} // namespace numkit::m::builtin::detail
