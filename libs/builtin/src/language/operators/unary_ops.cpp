// libs/builtin/src/lang/operators/unary_ops.cpp

#include <numkit/builtin/language/operators/unary_ops.hpp>
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
    return Value::logicalScalar(!x.toBool(), p);
}

Value ctranspose(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "ctranspose", "", "numkit:transpose:3DInput");
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
                 0, 0, "ctranspose", "", "numkit:transpose:unsupportedType");
}

Value transposeNC(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "numkit:transpose:3DInput");
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
                 0, 0, "transpose", "", "numkit:transpose:unsupportedType");
}

// ── Named-function adapters for unary operators ──────────────────────
// Pack 11: thin wrappers exposing the operator implementations under
// their MATLAB function names.
namespace detail {

#define NK_UNOP_REG(MATLAB_NAME, CXX_FN)                                             \
    void MATLAB_NAME##_reg(Span<const Value> args, size_t /*nargout*/,              \
                           Span<Value> outs, CallContext &ctx)                       \
    {                                                                                 \
        if (args.empty())                                                             \
            throw Error(#MATLAB_NAME ": requires 1 argument",                        \
                         0, 0, #MATLAB_NAME, "", "numkit:" #MATLAB_NAME ":nargin");        \
        outs[0] = CXX_FN(args[0], ctx.engine->resource());                           \
    }

NK_UNOP_REG(uminus,     uminus)
NK_UNOP_REG(uplus,      uplus)
NK_UNOP_REG(not,        logicalNot)
NK_UNOP_REG(ctranspose, ctranspose)

#undef NK_UNOP_REG

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward UnaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const Value &a) { return builtin::uminus(a, engine.resource()); });
    engine.registerUnaryOp("+",  [&engine](const Value &a) { return builtin::uplus(a, engine.resource()); });
    engine.registerUnaryOp("~",  [&engine](const Value &a) { return builtin::logicalNot(a, engine.resource()); });
    engine.registerUnaryOp("'",  [&engine](const Value &a) { return builtin::ctranspose(a, engine.resource()); });
    engine.registerUnaryOp(".'", [&engine](const Value &a) { return builtin::transposeNC(a, engine.resource()); });
}

} // namespace numkit
