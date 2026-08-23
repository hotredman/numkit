// src/builtin/src/datafun/sets.cpp
//
// Set operations implementations for numkit::builtin.

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/discrete/discrete.hpp>

namespace numkit::builtin {

Value unique(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::unique(x, mr);
}

Value ismember(const Value &a, const Value &s, std::pmr::memory_resource *mr)
{
    return numkit::math::ismember(a, s, mr);
}

Value union_set(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::math::setUnion(a, b, mr);
}

Value intersect(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::math::setIntersect(a, b, mr);
}

Value setdiff(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::math::setDiff(a, b, mr);
}

Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::math::setxor(a, b, mr);
}

} // namespace numkit::builtin
