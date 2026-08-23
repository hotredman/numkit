// src/builtin/src/specfun/combinatorics.cpp
//
// Combinatorics and number theory implementations for numkit::builtin.

#include <numkit/builtin/specfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/discrete/discrete.hpp>
#include <numkit/math/permutations.hpp>
#include <numkit/lang/bitwise/int_math.hpp>

namespace numkit::builtin {

Value factorial(const Value &n, std::pmr::memory_resource *mr)
{
    return numkit::math::factorial(n, mr);
}

Value nchoosek(const Value &v, int k, std::pmr::memory_resource *mr)
{
    if (v.isScalar()) {
        return numkit::math::nchoosek(v.toScalar(), static_cast<double>(k), mr);
    }
    return numkit::math::nchoosekCombinations(v, static_cast<double>(k), mr);
}

Value perms(const Value &v, std::pmr::memory_resource *mr)
{
    return numkit::math::perms(v, mr);
}

Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::lang::gcd(a, b, mr);
}

Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return numkit::lang::lcm(a, b, mr);
}

} // namespace numkit::builtin
