// ops/src/fused_kernels_portable.cpp
//
// Scalar fallbacks for the fused element-wise kernels (non-SIMD builds). One
// file for all kernels — scalar loops are tiny and carry no Highway
// multi-target / inliner-budget cost, so there is no reason to split them.
// Semantics match the SIMD versions bit-for-bit (std::fmin/fmax NaN rules).

#include <numkit/ops/fused_kernels.hpp>

#include <cmath>
#include <cstddef>

namespace numkit::ops {

void fusedAffineClamp(const double *x, double scale, double offset,
                      double lo, double hi, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = std::fmax(lo, std::fmin(hi, v));
    }
}

void fusedAffine(const double *x, double scale, double offset,
                 double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = scale * x[i] + offset;
}

} // namespace numkit::ops
