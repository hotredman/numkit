// libs/builtin/src/lang/operators/unary_ops.cpp

#include <numkit/builtin/language/operators/unary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <complex>
#include <cstdint>
#include <cstring>
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

namespace {

// Generic 2-D transpose shared by `.'` (transposeNC) and `'`
// (ctranspose). `conjugate` only affects COMPLEX input (negate the
// imaginary part). Type-preserving across DOUBLE / SINGLE / CHAR /
// LOGICAL / integer (raw byte rearrange), COMPLEX (per-element, optional
// conjugate) and CELL (per-cell move). STRING / STRUCT / FUNC_HANDLE are
// unsupported. Matches MATLAB: `A.'` / `A'` / transpose(A) / ctranspose(A)
// preserve the input class for all of these.
Value transpose2D(const Value &x, bool conjugate, const char *fnName,
                  std::pmr::memory_resource *p)
{
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, fnName, "", "numkit:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    const ValueType t = x.type();

    if (t == ValueType::COMPLEX) {
        if (x.isScalar())
            return Value::complexScalar(conjugate ? std::conj(x.toComplex())
                                                  : x.toComplex(), p);
        auto r = Value::complexMatrix(cols, rows, p);
        Complex *dst = r.complexDataMut();
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j) {
                const Complex v = x.complexElem(i, j);
                dst[i * cols + j] = conjugate ? std::conj(v) : v;
            }
        return r;
    }

    if (t == ValueType::CELL) {
        auto r = Value::cell(cols, rows, p);
        // r(j,i) = x(i,j): r idx (col-major, cols rows) = i*cols + j;
        //                  x idx (col-major, rows rows) = j*rows + i.
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.cellAt(i * cols + j) = x.cellAt(j * rows + i);
        return r;
    }

    // OBJECT arrays transpose like CELL (conjugate is a no-op for objects).
    if (t == ValueType::OBJECT)
        return x.objectTranspose(p);

    if (t == ValueType::STRING || t == ValueType::STRUCT ||
        t == ValueType::FUNC_HANDLE)
        throw Error("Transpose not supported for this type",
                     0, 0, fnName, "", "numkit:transpose:unsupportedType");

    // POD path: DOUBLE / SINGLE / CHAR / LOGICAL / int* — raw bytes.
    if (t == ValueType::DOUBLE && x.isScalar())
        return Value::scalar(x.toScalar(), p);
    const size_t es = elementSize(t);
    auto r = Value::matrix(cols, rows, t, p);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            std::memcpy(dst + (i * cols + j) * es,
                        src + (j * rows + i) * es, es);
    return r;
}

} // namespace

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
