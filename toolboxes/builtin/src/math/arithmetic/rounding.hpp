// toolboxes/builtin/src/math/arithmetic/rounding.hpp
//
// Internal header for the ceil/floor/round/fix backend. Double
// fast-path lives in rounding_{highway,portable}.cpp.

#pragma once

#include <cstddef>

namespace numkit::builtin::detail {

void doubleCeilLoop  (const double *in, double *out, std::size_t n);
void doubleFloorLoop (const double *in, double *out, std::size_t n);
void doubleRoundLoop (const double *in, double *out, std::size_t n);  // round half-away-from-zero
void doubleFixLoop   (const double *in, double *out, std::size_t n);  // trunc toward zero

} // namespace numkit::builtin::detail
