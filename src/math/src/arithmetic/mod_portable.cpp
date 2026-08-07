// toolboxes/builtin/src/math/arithmetic/mod_portable.cpp
//
// Scalar reference for the mod() fast-path entry points. Compiled when
// NUMKIT_HIGHWAY=OFF; the Highway-dispatched variant lives in
// mod_highway.cpp and is bit-identical (same separate-mul/sub formula).

#include "mod_simd.hpp"

#include <cmath>

namespace numkit::math::detail {

void modLoopVV(const double *a, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = (b[i] != 0) ? a[i] - std::floor(a[i] / b[i]) * b[i] : a[i];
}

void modLoopVS(const double *a, double s, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = (s != 0) ? a[i] - std::floor(a[i] / s) * s : a[i];
}

void modLoopSV(double s, const double *b, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = (b[i] != 0) ? s - std::floor(s / b[i]) * b[i] : s;
}

} // namespace numkit::math::detail
