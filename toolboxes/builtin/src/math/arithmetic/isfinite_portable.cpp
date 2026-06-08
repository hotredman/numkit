// toolboxes/builtin/src/math/arithmetic/isfinite_portable.cpp
//
// Reference scalar isnan/isinf/isfinite over a double array. Compiled
// when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// isfinite_highway.cpp.

#include "isfinite.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace numkit::builtin::detail {

void doubleIsNaNLoop(const double *in, uint8_t *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::isnan(in[i]) ? 1 : 0;
}

void doubleIsInfLoop(const double *in, uint8_t *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::isinf(in[i]) ? 1 : 0;
}

void doubleIsFiniteLoop(const double *in, uint8_t *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
        out[i] = std::isfinite(in[i]) ? 1 : 0;
}

} // namespace numkit::builtin::detail
