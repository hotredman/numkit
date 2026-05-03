// libs/builtin/src/math/arithmetic/isfinite.hpp
//
// Internal header for the isnan/isinf/isfinite backend. The double
// fast-path lives in isfinite_{highway,portable}.cpp; consumed from
// language/types/types.cpp.

#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit::builtin::detail {

void doubleIsNaNLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsInfLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsFiniteLoop(const double *in, uint8_t *out, std::size_t n);

} // namespace numkit::builtin::detail
