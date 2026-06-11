// math/src/complex/complex.cpp

#include <numkit/math/complex/complex.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>

#include <cmath>
#include <complex>

namespace numkit::math {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

Value real(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (!x.isComplex())
        return x;
    if (x.isScalar())
        return Value::scalar(x.toComplex().real(), p);
    auto r = createLike(x, ValueType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].real();
    return r;
}

Value imag(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (!x.isComplex())
        return Value::scalar(0.0, p);
    if (x.isScalar())
        return Value::scalar(x.toComplex().imag(), p);
    auto r = createLike(x, ValueType::DOUBLE, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.complexData()[i].imag();
    return r;
}

Value conj(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (!x.isComplex())
        return x;
    return unaryComplex(x, [](const Complex &c) { return std::conj(c); }, p);
}

Value complex(const Value &re, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (re.isScalar())
        return Value::complexScalar(re.toScalar(), 0.0, p);
    auto r = createLike(re, ValueType::COMPLEX, p);
    Complex *dst = r.complexDataMut();
    for (size_t i = 0; i < re.numel(); ++i)
        dst[i] = Complex(re.elemAsDouble(i), 0.0);
    return r;
}

Value complex(const Value &re, const Value &im, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (re.isScalar() && im.isScalar())
        return Value::complexScalar(re.toScalar(), im.toScalar(), p);
    const Value &shape = re.isScalar() ? im : re;
    if (!re.isScalar() && !im.isScalar() && re.dims() != im.dims())
        throw Error("complex: real and imaginary parts must have matching dimensions",
                     0, 0, "complex", "", "numkit:complex:dimagree");
    auto r = createLike(shape, ValueType::COMPLEX, p);
    Complex *dst = r.complexDataMut();
    const size_t n = shape.numel();
    const double reScalar = re.isScalar() ? re.toScalar() : 0.0;
    const double imScalar = im.isScalar() ? im.toScalar() : 0.0;
    for (size_t i = 0; i < n; ++i) {
        double r0 = re.isScalar() ? reScalar : re.elemAsDouble(i);
        double i0 = im.isScalar() ? imScalar : im.elemAsDouble(i);
        dst[i] = Complex(r0, i0);
    }
    return r;
}

Value angle(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::scalar(std::arg(x.toComplex()), p);
        auto r = createLike(x, ValueType::DOUBLE, p);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::arg(x.complexData()[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::atan2(0.0, v); }, p);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════


} // namespace numkit::math
