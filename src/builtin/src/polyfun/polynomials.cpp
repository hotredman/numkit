// src/builtin/src/polyfun/polynomials.cpp
//
// Polynomial implementations for numkit::builtin.

#include <numkit/builtin/polyfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/poly/polynomials.hpp>

namespace numkit::builtin {

Value roots(const Value &p, std::pmr::memory_resource *mr)
{
    return numkit::math::roots(p, mr);
}

Value poly(const Value &r, std::pmr::memory_resource *mr)
{
    return numkit::math::poly(r, mr);
}

Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::polyval(p, x, mr);
}

Value polyder(const Value &p, std::pmr::memory_resource *mr)
{
    return numkit::math::polyder(p, mr);
}

Value polyint(const Value &p, double k, std::pmr::memory_resource *mr)
{
    return numkit::math::polyint(p, k, mr);
}

Value polyfit(const Value &x, const Value &y, size_t n, std::pmr::memory_resource *mr)
{
    return numkit::math::polyfit(x, y, n, mr);
}

} // namespace numkit::builtin
