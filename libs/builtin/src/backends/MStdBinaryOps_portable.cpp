// libs/builtin/src/backends/MStdBinaryOps_portable.cpp
//
// Scalar reference inner loops for plus / minus / times / rdivide on
// real double arrays. Compiled when NUMKIT_WITH_SIMD=OFF. The
// Highway-dispatched variant lives in MStdBinaryOps_simd.cpp.

#include "BinaryOpsLoops.hpp"

#include <cstddef>

namespace numkit::m::builtin::detail {

void plusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] + b[i];
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] - b[i];
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] * b[i];
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = a[i] / b[i];
}

} // namespace numkit::m::builtin::detail
