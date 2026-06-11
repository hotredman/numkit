// toolboxes/builtin/src/math/arithmetic/rounding_portable.cpp
//
// Reference scalar ceil/floor/round/fix over a double array.
// Compiled when NUMKIT_WITH_SIMD=OFF.

#include "rounding.hpp"

#include <cmath>
#include <cstddef>

namespace numkit::math::detail {

void doubleCeilLoop(const double *in, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = std::ceil(in[i]);
}

void doubleFloorLoop(const double *in, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = std::floor(in[i]);
}

void doubleRoundLoop(const double *in, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = std::round(in[i]);
}

void doubleFixLoop(const double *in, double *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = std::trunc(in[i]);
}

} // namespace numkit::math::detail
