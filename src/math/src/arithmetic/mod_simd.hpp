// toolboxes/builtin/src/math/arithmetic/mod_simd.hpp
//
// Backend-split entry points for the SIMD-accelerated mod() fast path.
// Defined in mod_highway.cpp (NUMKIT_HIGHWAY) or mod_portable.cpp.
// Each computes r = (b != 0) ? a - floor(a/b)*b : a element-wise, the
// same formula as the scalar reference in misc.cpp. Callers must pass
// real contiguous DOUBLE buffers; broadcasting / integer / complex /
// empty cases stay on the scalar elementwiseDouble path in misc.cpp.

#pragma once

#include <cstddef>

namespace numkit::math::detail {

void modLoopVV(const double *a, const double *b, double *out, std::size_t n);
void modLoopVS(const double *a, double scalar, double *out, std::size_t n);
void modLoopSV(double scalar, const double *b, double *out, std::size_t n);

} // namespace numkit::math::detail
