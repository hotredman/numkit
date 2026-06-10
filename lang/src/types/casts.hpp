// toolboxes/builtin/src/language/types/casts.hpp
//
// Internal header shared by casts_highway.cpp / casts_portable.cpp
// and consumed by types.cpp's numericConstructor fast-path.
//
// Each `doubleToIntXX` function performs MATLAB-spec int cast on a
// double array: NaN→0, round half-away-from-zero, saturate to target.

#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit::lang::detail {

void doubleToInt8 (const double *in, int8_t   *out, std::size_t n);
void doubleToInt16(const double *in, int16_t  *out, std::size_t n);
void doubleToInt32(const double *in, int32_t  *out, std::size_t n);
void doubleToInt64(const double *in, int64_t  *out, std::size_t n);
void doubleToUInt8 (const double *in, uint8_t  *out, std::size_t n);
void doubleToUInt16(const double *in, uint16_t *out, std::size_t n);
void doubleToUInt32(const double *in, uint32_t *out, std::size_t n);
void doubleToUInt64(const double *in, uint64_t *out, std::size_t n);

} // namespace numkit::lang::detail
