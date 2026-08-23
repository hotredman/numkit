// src/builtin/src/specfun/bessel.cpp
//
// Bessel, Airy, Elliptic, and Legendre implementations for numkit::builtin.

#include <numkit/builtin/specfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/special/special.hpp>

namespace numkit::builtin {

Value besselj(const Value &nu, const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::besselj(nu, z, mr);
}

Value bessely(const Value &nu, const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::bessely(nu, z, mr);
}

Value besseli(const Value &nu, const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::besseli(nu, z, mr);
}

Value besselk(const Value &nu, const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::besselk(nu, z, mr);
}

Value besselh(const Value &nu, int k, const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::besselh(nu, k, z, mr);
}

Value airy(const Value &z, std::pmr::memory_resource *mr)
{
    return numkit::math::airy(0, z, mr);
}

Value expint(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::expint(x, mr);
}

Value ellipke(const Value &m, std::pmr::memory_resource *mr)
{
    return numkit::math::ellipke(m, mr).K;
}

Value legendre(int n, const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::legendre(n, x, mr);
}

} // namespace numkit::builtin
