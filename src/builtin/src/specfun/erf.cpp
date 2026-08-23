// src/builtin/src/specfun/erf.cpp
//
// Error function family implementations for numkit::builtin.

#include <numkit/builtin/specfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/special/special.hpp>

namespace numkit::builtin {

Value erf(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::erf(x, mr);
}

Value erfc(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::erfc(x, mr);
}

Value erfcx(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::erfcx(x, mr);
}

Value erfinv(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::erfinv(x, mr);
}

Value erfcinv(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::erfcinv(x, mr);
}

} // namespace numkit::builtin
