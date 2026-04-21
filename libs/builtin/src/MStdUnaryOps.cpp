// libs/builtin/src/MStdUnaryOps.cpp

#include <numkit/m/builtin/MStdUnaryOps.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <complex>
#include <cstdint>
#include <functional>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — unary operators
// ════════════════════════════════════════════════════════════════════════

MValue uminus(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isEmpty()) {
        MType outType = x.isComplex()                   ? MType::COMPLEX
                        : (x.isChar() || x.isLogical()) ? MType::DOUBLE
                                                        : x.type();
        return createLike(x, outType, p);
    }
    if (x.isComplex())
        return unaryComplex(x, std::negate<Complex>{}, p);
    if (x.type() == MType::DOUBLE)
        return unaryDouble(x, std::negate<double>{}, p);
    if (x.type() == MType::SINGLE)
        return unaryTyped<float>(x, MType::SINGLE, [](float v) { return -v; }, p);
    if (isIntegerType(x.type())) {
        switch (x.type()) {
        case MType::INT8:   return unaryTyped<int8_t>(x, x.type(),  [](int8_t v)   { return saturateNeg(v); }, p);
        case MType::INT16:  return unaryTyped<int16_t>(x, x.type(), [](int16_t v)  { return saturateNeg(v); }, p);
        case MType::INT32:  return unaryTyped<int32_t>(x, x.type(), [](int32_t v)  { return saturateNeg(v); }, p);
        case MType::INT64:  return unaryTyped<int64_t>(x, x.type(), [](int64_t v)  { return saturateNeg(v); }, p);
        case MType::UINT8:  return unaryTyped<uint8_t>(x, x.type(), [](uint8_t)    { return uint8_t(0); },    p);
        case MType::UINT16: return unaryTyped<uint16_t>(x, x.type(),[](uint16_t)   { return uint16_t(0); },   p);
        case MType::UINT32: return unaryTyped<uint32_t>(x, x.type(),[](uint32_t)   { return uint32_t(0); },   p);
        case MType::UINT64: return unaryTyped<uint64_t>(x, x.type(),[](uint64_t)   { return uint64_t(0); },   p);
        default: break;
        }
    }
    throw MError("Unsupported unary -", 0, 0, "uminus", "", "m:uminus:unsupportedTypes");
}

MValue uplus(Allocator &, const MValue &x)
{
    return x;
}

MValue logicalNot(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isLogical()) {
        if (x.isScalar())
            return MValue::logicalScalar(!x.toBool(), p);
        auto r = createLike(x, MType::LOGICAL, p);
        const uint8_t *src = x.logicalData();
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = src[i] ? 0 : 1;
        return r;
    }
    if (x.type() == MType::DOUBLE) {
        if (x.isScalar())
            return MValue::logicalScalar(x.toScalar() == 0.0, p);
        auto r = createLike(x, MType::LOGICAL, p);
        const double *src = x.doubleData();
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = (src[i] == 0.0) ? 1 : 0;
        return r;
    }
    return MValue::logicalScalar(!x.toBool(), p);
}

MValue ctranspose(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.dims().is3D())
        throw MError("transpose is not defined for N-D arrays",
                     0, 0, "ctranspose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();

    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::complexScalar(std::conj(x.toComplex()), p);
        auto r = MValue::complexMatrix(cols, rows, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.complexDataMut()[i * cols + j] = std::conj(x.complexElem(i, j));
        return r;
    }
    if (x.type() == MType::DOUBLE) {
        if (x.isScalar())
            return MValue::scalar(x.toScalar(), p);
        auto r = MValue::matrix(cols, rows, MType::DOUBLE, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.elem(j, i) = x(i, j);
        return r;
    }
    throw MError("Transpose not supported for this type",
                 0, 0, "ctranspose", "", "m:transpose:unsupportedType");
}

MValue transposeNC(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.dims().is3D())
        throw MError("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();

    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::complexScalar(x.toComplex(), p);
        auto r = MValue::complexMatrix(cols, rows, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.complexDataMut()[i * cols + j] = x.complexElem(i, j);
        return r;
    }
    if (x.type() == MType::DOUBLE) {
        if (x.isScalar())
            return MValue::scalar(x.toScalar(), p);
        auto r = MValue::matrix(cols, rows, MType::DOUBLE, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.elem(j, i) = x(i, j);
        return r;
    }
    throw MError("Transpose not supported for this type",
                 0, 0, "transpose", "", "m:transpose:unsupportedType");
}

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward UnaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerUnaryOps(Engine &engine)
{
    engine.registerUnaryOp("-",  [&engine](const MValue &a) { return builtin::uminus(engine.allocator(), a); });
    engine.registerUnaryOp("+",  [&engine](const MValue &a) { return builtin::uplus(engine.allocator(), a); });
    engine.registerUnaryOp("~",  [&engine](const MValue &a) { return builtin::logicalNot(engine.allocator(), a); });
    engine.registerUnaryOp("'",  [&engine](const MValue &a) { return builtin::ctranspose(engine.allocator(), a); });
    engine.registerUnaryOp(".'", [&engine](const MValue &a) { return builtin::transposeNC(engine.allocator(), a); });
}

} // namespace numkit::m
