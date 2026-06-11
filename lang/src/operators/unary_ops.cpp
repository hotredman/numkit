// toolboxes/builtin/src/lang/operators/unary_ops.cpp

#include <numkit/lang/operators/unary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <complex>
#include <cstdint>
#include <cstring>
#include <functional>

#include "unary_ops_detail.hpp"

namespace numkit::lang {

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

Value logicalNot(const Value &x, std::pmr::memory_resource *mr)
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
    // single / int* / char (any other numeric): ~x is (x == 0), element-wise.
    // (Previously this fell back to toBool(), which threw on a non-scalar
    // integer/single array.)
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

Value transposeNC(const Value &x, std::pmr::memory_resource *mr)
{
    return transpose2D(x, /*conjugate=*/false, "transpose", mr);
}

// ── Named-function adapters for unary operators ──────────────────────
// Pack 11: thin wrappers exposing the operator implementations under
// their MATLAB function names.

} // namespace numkit::lang

// ════════════════════════════════════════════════════════════════════════
// Registration — forward UnaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const Value &a) { return numkit::lang::uminus(a, engine.resource()); });
    engine.registerUnaryOp("+",  [&engine](const Value &a) { return numkit::lang::uplus(a, engine.resource()); });
    engine.registerUnaryOp("~",  [&engine](const Value &a) { return numkit::lang::logicalNot(a, engine.resource()); });
    engine.registerUnaryOp("'",  [&engine](const Value &a) { return numkit::lang::ctranspose(a, engine.resource()); });
    engine.registerUnaryOp(".'", [&engine](const Value &a) { return numkit::lang::transposeNC(a, engine.resource()); });
}

} // namespace numkit
