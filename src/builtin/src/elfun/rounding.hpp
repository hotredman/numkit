// src/builtin/src/elfun/rounding.hpp
//
// Internal header for the ceil/floor/round/fix backend in numkit::builtin.
#pragma once

#include <cstddef>

namespace numkit::builtin::detail {

void doubleCeilLoop  (const double *in, double *out, std::size_t n);
void doubleFloorLoop (const double *in, double *out, std::size_t n);
void doubleRoundLoop (const double *in, double *out, std::size_t n);
void doubleFixLoop   (const double *in, double *out, std::size_t n);

} // namespace numkit::builtin::detail
