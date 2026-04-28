// libs/builtin/src/backends/cumsum_portable.cpp
//
// Scalar inclusive prefix-sum. Compiled when NUMKIT_WITH_SIMD=OFF.
// The loop has a serial data dependency (`s += src[i]; dst[i] = s;`)
// so this is fundamentally one-add-per-cycle on the portable build.

#include "cumsum.hpp"

#include <cmath>
#include <cstddef>

namespace numkit::builtin {

void cumsumScan(const double *src, double *dst, std::size_t n)
{
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        s += src[i];
        dst[i] = s;
    }
}

void cumprodScan(const double *src, double *dst, std::size_t n)
{
    double s = 1.0;
    for (std::size_t i = 0; i < n; ++i) {
        s *= src[i];
        dst[i] = s;
    }
}

void cummaxScan(const double *src, double *dst, std::size_t n)
{
    if (n == 0) return;
    // Walk leading NaN, preserve as NaN.
    std::size_t i = 0;
    for (; i < n && std::isnan(src[i]); ++i) dst[i] = std::nan("");
    if (i == n) return;
    double acc = src[i];
    dst[i] = acc;
    for (++i; i < n; ++i) {
        const double xi = src[i];
        if (!std::isnan(xi) && xi > acc) acc = xi;
        dst[i] = acc;
    }
}

void cumminScan(const double *src, double *dst, std::size_t n)
{
    if (n == 0) return;
    std::size_t i = 0;
    for (; i < n && std::isnan(src[i]); ++i) dst[i] = std::nan("");
    if (i == n) return;
    double acc = src[i];
    dst[i] = acc;
    for (++i; i < n; ++i) {
        const double xi = src[i];
        if (!std::isnan(xi) && xi < acc) acc = xi;
        dst[i] = acc;
    }
}

} // namespace numkit::builtin
