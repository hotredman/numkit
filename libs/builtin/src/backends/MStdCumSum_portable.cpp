// libs/builtin/src/backends/MStdCumSum_portable.cpp
//
// Scalar inclusive prefix-sum. Compiled when NUMKIT_WITH_SIMD=OFF.
// The loop has a serial data dependency (`s += src[i]; dst[i] = s;`)
// so this is fundamentally one-add-per-cycle on the portable build.

#include "MStdCumSum.hpp"

#include <cstddef>

namespace numkit::m::builtin {

void cumsumScan(const double *src, double *dst, std::size_t n)
{
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        s += src[i];
        dst[i] = s;
    }
}

} // namespace numkit::m::builtin
