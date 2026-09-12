// src/builtin/src/elfun/mod_simd.hpp
//
// Backend-split entry points for the SIMD-accelerated mod() fast path in nk::builtin.
#pragma once

#include <cstddef>

namespace nk::builtin::detail {

void modLoopVV(const double *a, const double *b, double *out, std::size_t n);
void modLoopVS(const double *a, double scalar, double *out, std::size_t n);
void modLoopSV(double scalar, const double *b, double *out, std::size_t n);

} // namespace nk::builtin::detail
