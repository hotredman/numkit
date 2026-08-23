// src/builtin/src/polyfun/integration.cpp
//
// Numerical integration implementations for numkit::builtin.

#include <numkit/builtin/polyfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/integration/integration.hpp>

namespace numkit::builtin {

Value trapz(const Value &y, std::pmr::memory_resource *mr)
{
    return numkit::math::trapz(y, mr);
}

Value trapz(const Value &x, const Value &y, int /*dim*/, std::pmr::memory_resource *mr)
{
    return numkit::math::trapz(x, y, mr);
}

Value cumtrapz(const Value &y, std::pmr::memory_resource *mr)
{
    return numkit::math::cumtrapz(y, mr);
}

Value cumtrapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr)
{
    if (dim > 0) {
        return numkit::math::cumtrapzDim(y, dim, mr);
    }
    return numkit::math::cumtrapz(x, y, mr);
}

} // namespace numkit::builtin
