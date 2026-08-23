// src/builtin/src/polyfun/interpolation.cpp
//
// Interpolation and piecewise polynomial implementations for numkit::builtin.

#include <numkit/builtin/polyfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/math/interp/interp.hpp>

namespace numkit::builtin {

Value interp1(const Value &x, const Value &v, const Value &xq, const std::string &method, std::pmr::memory_resource *mr)
{
    return numkit::math::interp1(x, v, xq, method, mr);
}

Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method, std::pmr::memory_resource *mr)
{
    return numkit::math::interp2(x, y, v, xq, yq, method, mr);
}

Value spline(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::spline(x, y, xq, mr);
}

Value pchip(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::pchip(x, y, xq, mr);
}

Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr)
{
    return numkit::math::mkpp(breaks, coefs, mr);
}

Value unmkpp(const Value &pp, std::pmr::memory_resource *mr)
{
    (void)mr;
    if (pp.isStruct() && pp.hasField("breaks"))
        return pp.field("breaks");
    return Value();
}

Value ppval(const Value &pp, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::ppval(pp, xq, mr);
}

} // namespace numkit::builtin
