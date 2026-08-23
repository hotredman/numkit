// src/builtin/src/datafun/reductions.cpp
//
// Reductions and aggregate statistics implementations for numkit::builtin.

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/arithmetic/reductions.hpp>
#include <numkit/ops/reductions.hpp>

namespace numkit::builtin {

Value sum(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return numkit::math::sum(x, dim, mr);
}

Value prod(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return numkit::math::prod(x, dim, mr);
}

Value mean(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return numkit::math::mean(x, dim, mr);
}

Value max(const Value &a, const Value &b, int dim, std::pmr::memory_resource *mr)
{
    if (b.isEmpty()) {
        return std::get<0>(numkit::math::max(a, dim, mr));
    }
    return numkit::math::max(a, b, mr);
}

Value min(const Value &a, const Value &b, int dim, std::pmr::memory_resource *mr)
{
    if (b.isEmpty()) {
        return std::get<0>(numkit::math::min(a, dim, mr));
    }
    return numkit::math::min(a, b, mr);
}

} // namespace numkit::builtin
