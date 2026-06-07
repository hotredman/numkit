// libs/.../int_math_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by int_math.cpp + int_math_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::builtin {

namespace {

inline int64_t toInt64(double v)
{
    if (std::isnan(v) || std::isinf(v)) return 0;
    return static_cast<int64_t>(v);
}

inline int64_t gcdInt(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        const int64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

} // namespace

} // namespace numkit::builtin
