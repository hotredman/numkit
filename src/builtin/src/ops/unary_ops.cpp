// src/builtin/src/ops/unary_ops.cpp

#include <numkit/builtin/ops.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>

#include <complex>
#include <cstdint>
#include <cstring>
#include <functional>

#include "unary_ops_detail.hpp"

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — unary operators
// ════════════════════════════════════════════════════════════════════════

Value uminus(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isEmpty()) {
        ValueType outType = x.isComplex()                   ? ValueType::COMPLEX
                        : (x.isChar() || x.isLogical()) ? ValueType::DOUBLE
                                                        : x.type();
        return createLike(x, outType, p);
    }
    if (x.isComplex())
        return unaryComplex(x, std::negate<Complex>{}, p);
    if (x.type() == ValueType::DOUBLE)
        return unaryDouble(x, std::negate<double>{}, p);
    if (x.type() == ValueType::SINGLE)
        return unaryTyped<float>(x, ValueType::SINGLE, [](float v) { return -v; }, p);
    if (isIntegerType(x.type())) {
        switch (x.type()) {
        case ValueType::INT8:   return unaryTyped<int8_t>(x, x.type(),  [](int8_t v)   { return saturateNeg(v); }, p);
        case ValueType::INT16:  return unaryTyped<int16_t>(x, x.type(), [](int16_t v)  { return saturateNeg(v); }, p);
        case ValueType::INT32:  return unaryTyped<int32_t>(x, x.type(), [](int32_t v)  { return saturateNeg(v); }, p);
        case ValueType::INT64:  return unaryTyped<int64_t>(x, x.type(), [](int64_t v)  { return saturateNeg(v); }, p);
        case ValueType::UINT8:  return unaryTyped<uint8_t>(x, x.type(), [](uint8_t)    { return uint8_t(0); },    p);
        case ValueType::UINT16: return unaryTyped<uint16_t>(x, x.type(),[](uint16_t)   { return uint16_t(0); },   p);
        case ValueType::UINT32: return unaryTyped<uint32_t>(x, x.type(),[](uint32_t)   { return uint32_t(0); },   p);
        case ValueType::UINT64: return unaryTyped<uint64_t>(x, x.type(),[](uint64_t)   { return uint64_t(0); },   p);
        default: break;
        }
    }
    throw Error("Unsupported unary -", 0, 0, "uminus", "", "numkit:uminus:unsupportedTypes");
}

Value uplus(const Value &x, std::pmr::memory_resource *)
{
    return x;
}

Value logical_not(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isLogical()) {
        if (x.isScalar())
            return Value::logicalScalar(!x.toBool(), p);
        auto r = createLike(x, ValueType::LOGICAL, p);
        const uint8_t *src = x.logicalData();
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = src[i] ? 0 : 1;
        return r;
    }
    if (x.type() == ValueType::DOUBLE) {
        if (x.isScalar())
            return Value::logicalScalar(x.toScalar() == 0.0, p);
        auto r = createLike(x, ValueType::LOGICAL, p);
        const double *src = x.doubleData();
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = (src[i] == 0.0) ? 1 : 0;
        return r;
    }
    if (x.isComplex()) {
        auto r = createLike(x, ValueType::LOGICAL, p);
        const Complex *src = x.complexData();
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = (src[i].real() == 0.0 && src[i].imag() == 0.0) ? 1 : 0;
        return r;
    }
    if (x.isScalar())
        return Value::logicalScalar(x.elemAsDouble(0) == 0.0, p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    uint8_t *dst = r.logicalDataMut();
    for (size_t i = 0; i < x.numel(); ++i)
        dst[i] = (x.elemAsDouble(i) == 0.0) ? 1 : 0;
    return r;
}

Value ctranspose(const Value &x, std::pmr::memory_resource *mr)
{
    return transpose2D(x, /*conjugate=*/true, "ctranspose", mr);
}

Value transpose(const Value &x, std::pmr::memory_resource *mr)
{
    return transpose2D(x, /*conjugate=*/false, "transpose", mr);
}

Value any(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::logicalScalar(false, mr);
    const size_t n = x.numel();
    if (x.isLogical()) {
        const uint8_t *d = x.logicalData();
        for (size_t i = 0; i < n; ++i) if (d[i]) return Value::logicalScalar(true, mr);
        return Value::logicalScalar(false, mr);
    }
    for (size_t i = 0; i < n; ++i) {
        if (x.elemAsDouble(i) != 0.0) return Value::logicalScalar(true, mr);
    }
    return Value::logicalScalar(false, mr);
}

Value all(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::logicalScalar(true, mr);
    const size_t n = x.numel();
    if (x.isLogical()) {
        const uint8_t *d = x.logicalData();
        for (size_t i = 0; i < n; ++i) if (!d[i]) return Value::logicalScalar(false, mr);
        return Value::logicalScalar(true, mr);
    }
    for (size_t i = 0; i < n; ++i) {
        if (x.elemAsDouble(i) == 0.0) return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(true, mr);
}

} // namespace numkit::builtin
