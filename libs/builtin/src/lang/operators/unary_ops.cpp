// libs/builtin/src/lang/operators/unary_ops.cpp

#include <numkit/builtin/lang/operators/unary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <complex>
#include <cstdint>
#include <functional>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — unary operators
// ════════════════════════════════════════════════════════════════════════

Value uminus(std::pmr::memory_resource *mr, const Value &x)
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
    throw Error("Unsupported unary -", 0, 0, "uminus", "", "m:uminus:unsupportedTypes");
}

Value uplus(std::pmr::memory_resource *, const Value &x)
{
    return x;
}

Value logicalNot(std::pmr::memory_resource *mr, const Value &x)
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
    return Value::logicalScalar(!x.toBool(), p);
}

Value ctranspose(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "ctranspose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();

    if (x.isComplex()) {
        if (x.isScalar())
            return Value::complexScalar(std::conj(x.toComplex()), p);
        auto r = Value::complexMatrix(cols, rows, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.complexDataMut()[i * cols + j] = std::conj(x.complexElem(i, j));
        return r;
    }
    if (x.type() == ValueType::DOUBLE) {
        if (x.isScalar())
            return Value::scalar(x.toScalar(), p);
        auto r = Value::matrix(cols, rows, ValueType::DOUBLE, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.elem(j, i) = x(i, j);
        return r;
    }
    throw Error("Transpose not supported for this type",
                 0, 0, "ctranspose", "", "m:transpose:unsupportedType");
}

Value transposeNC(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();

    if (x.isComplex()) {
        if (x.isScalar())
            return Value::complexScalar(x.toComplex(), p);
        auto r = Value::complexMatrix(cols, rows, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.complexDataMut()[i * cols + j] = x.complexElem(i, j);
        return r;
    }
    if (x.type() == ValueType::DOUBLE) {
        if (x.isScalar())
            return Value::scalar(x.toScalar(), p);
        auto r = Value::matrix(cols, rows, ValueType::DOUBLE, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.elem(j, i) = x(i, j);
        return r;
    }
    throw Error("Transpose not supported for this type",
                 0, 0, "transpose", "", "m:transpose:unsupportedType");
}

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward UnaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const Value &a) { return builtin::uminus(engine.resource(), a); });
    engine.registerUnaryOp("+",  [&engine](const Value &a) { return builtin::uplus(engine.resource(), a); });
    engine.registerUnaryOp("~",  [&engine](const Value &a) { return builtin::logicalNot(engine.resource(), a); });
    engine.registerUnaryOp("'",  [&engine](const Value &a) { return builtin::ctranspose(engine.resource(), a); });
    engine.registerUnaryOp(".'", [&engine](const Value &a) { return builtin::transposeNC(engine.resource(), a); });
}

} // namespace numkit
