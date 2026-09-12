// toolboxes/builtin/src/math/special/special_portable.cpp
//
// Reference scalar erf. Compiled when NK_WITH_SIMD=OFF; the Highway-
// dispatched variant (SLEEF-ported dd kernel) lives in special_highway.cpp
// and matches this file on every input outside the vectorised range.

#include <nk/builtin/specfun.hpp>

#include <nk/value/error.hpp>

#include <nk/ops/helpers.hpp>

#include <cmath>

namespace nk::builtin {

Value erf(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::erf(v); }, mr);
}

} // namespace nk::builtin
