// src/builtin/src/elfun/exponents.cpp
//
// Scalar exponentials, power scaling, and roots for numkit::builtin.

#include <numkit/builtin/elfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/ops/helpers.hpp>

#include <cmath>
#include <complex>
#include "_unary_hint.hpp"

namespace numkit::builtin {

Value exp(const Value &x, std::pmr::memory_resource *mr) { return exp(x, nullptr, mr); }
Value log(const Value &x, std::pmr::memory_resource *mr) { return log(x, nullptr, mr); }

Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(f, e,
        [](double ff, double ee) {
            return std::ldexp(ff, static_cast<int>(std::floor(ee)));
        }, mr);
}

Value nextpow2(const Value &n, std::pmr::memory_resource *mr)
{
    auto nextPow2Scalar = [](double v) -> double {
        if (std::isnan(v)) return v;
        double a = std::fabs(v);
        if (a <= 1.0) return 0.0;
        int exp_val;
        double frac = std::frexp(a, &exp_val);
        if (frac == 0.5) return static_cast<double>(exp_val - 1);
        return static_cast<double>(exp_val);
    };
    return unaryDouble(n, nextPow2Scalar, mr);
}

Value cbrt(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::pow(c, 1.0 / 3.0); }, mr);
    return unaryDouble(x, [](double v) { return std::cbrt(v); }, mr);
}

Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(x, n, [](double xv, double nv) {
        if (nv == 0.0) return std::nan("");
        if (xv >= 0.0) return std::pow(xv, 1.0 / nv);
        const double rounded = std::round(nv);
        if (rounded != nv) return std::nan("");
        const long long ni = static_cast<long long>(rounded);
        if (ni % 2 == 0) return std::nan("");
        return -std::pow(-xv, 1.0 / nv);
    }, mr);
}

Value pow(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(x, y, [](double xx, double yy) { return std::pow(xx, yy); }, mr);
}

Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    auto checkPair = [](double xx, double yy) {
        if (xx < 0.0 && yy != std::floor(yy)) {
            throw std::runtime_error(
                "realpow produced complex result — use power(.^) instead");
        }
        return std::pow(xx, yy);
    };
    return elementwiseDouble(x, y, checkPair, mr);
}

} // namespace numkit::builtin
