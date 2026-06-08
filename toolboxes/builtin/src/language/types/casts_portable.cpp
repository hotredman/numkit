// toolboxes/builtin/src/language/types/casts_portable.cpp
//
// Reference scalar implementation of the double→int* fast path.
// Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant
// lives in casts_highway.cpp and matches this file bit-for-bit.
//
// MATLAB's `int*(x)` casts: NaN→0, then round half-away-from-zero,
// then saturate to the target's [min, max]. Matches std::round +
// saturate.

#include "casts.hpp"

#include <numkit/value/error.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace numkit::builtin::detail {

namespace {

template <typename T>
inline T saturate_cast(double v)
{
    if (std::isnan(v)) return 0;
    v = std::round(v);
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v < lo) return std::numeric_limits<T>::min();
    if (v > hi) return std::numeric_limits<T>::max();
    return static_cast<T>(v);
}

template <typename T>
void doubleToIntLoopScalar(const double *in, T *out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast<T>(in[i]);
}

} // namespace

void doubleToInt8 (const double *in, int8_t   *out, std::size_t n) { doubleToIntLoopScalar<int8_t  >(in, out, n); }
void doubleToInt16(const double *in, int16_t  *out, std::size_t n) { doubleToIntLoopScalar<int16_t >(in, out, n); }
void doubleToInt32(const double *in, int32_t  *out, std::size_t n) { doubleToIntLoopScalar<int32_t >(in, out, n); }
void doubleToInt64(const double *in, int64_t  *out, std::size_t n) { doubleToIntLoopScalar<int64_t >(in, out, n); }
void doubleToUInt8 (const double *in, uint8_t  *out, std::size_t n) { doubleToIntLoopScalar<uint8_t >(in, out, n); }
void doubleToUInt16(const double *in, uint16_t *out, std::size_t n) { doubleToIntLoopScalar<uint16_t>(in, out, n); }
void doubleToUInt32(const double *in, uint32_t *out, std::size_t n) { doubleToIntLoopScalar<uint32_t>(in, out, n); }
void doubleToUInt64(const double *in, uint64_t *out, std::size_t n) { doubleToIntLoopScalar<uint64_t>(in, out, n); }

} // namespace numkit::builtin::detail
