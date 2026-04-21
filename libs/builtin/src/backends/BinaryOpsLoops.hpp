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

// Matrix multiply over column-major real doubles.
//  A: M×K at memory layout a[k*M + i]
//  B: K×N at memory layout b[j*K + k]
//  C: M×N written as c[j*M + i]; caller allocates, this function zeros
// and then accumulates. Loop order is (j, k, i) — SAXPY down columns of
// A with the (k,j) element of B as the scalar — cache-friendly and
// directly vectorisable with MulAdd.
void matmulDoubleLoop(const double *a, const double *b, double *c,
                      std::size_t M, std::size_t N, std::size_t K);

} // namespace numkit::m::builtin::detail
