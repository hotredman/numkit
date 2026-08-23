// src/builtin/src/specfun/gamma_beta.cpp
//
// Gamma and Beta family implementations for numkit::builtin.

#include <numkit/builtin/specfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/special/special.hpp>

namespace numkit::builtin {

Value gamma(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::gammaFn(x, mr);
}

Value gammaln(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::gammaln(x, mr);
}

Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr)
{
    return numkit::math::gammainc(x, a, mr);
}

Value gammaincinv(const Value &y, const Value &a, std::pmr::memory_resource *mr)
{
    return numkit::math::gammaincinv(y, a, mr);
}

Value psi(const Value &x, std::pmr::memory_resource *mr)
{
    return numkit::math::psi(x, mr);
}

Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return numkit::math::beta(z, w, mr);
}

Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return numkit::math::betaln(z, w, mr);
}

Value betainc(const Value &x, const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return numkit::math::betainc(x, z, w, mr);
}

Value betaincinv(const Value &y, const Value &z, const Value &w, std::pmr::memory_resource *mr)
{
    return numkit::math::betaincinv(y, z, w, mr);
}

} // namespace numkit::builtin
