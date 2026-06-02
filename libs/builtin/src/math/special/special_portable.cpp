// libs/builtin/src/math/special/special_portable.cpp
//
// Reference scalar erf. Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-
// dispatched variant (SLEEF-ported dd kernel) lives in special_highway.cpp
// and matches this file on every input outside the vectorised range.

#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>

namespace numkit::builtin {

Value erf(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::erf(v); }, mr);
}

} // namespace numkit::builtin
