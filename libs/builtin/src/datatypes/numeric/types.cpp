// libs/builtin/src/datatypes/numeric/types.cpp

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/datatypes/numeric/types.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Implementation helpers
// ════════════════════════════════════════════════════════════════════════

namespace {

template <typename T>
T saturateCast(double v)
{
    if (std::isnan(v)) return 0;
    v = std::round(v);
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v < lo) return std::numeric_limits<T>::min();
    if (v > hi) return std::numeric_limits<T>::max();
    return static_cast<T>(v);
}

template <typename T>
Value numericConstructor(ValueType targetType, const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == targetType)
        return x;
    const size_t n = x.numel();
    Value r = createLike(x, targetType, mr);
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t i = 0; i < n; ++i) {
        double v = x.elemAsDouble(i);
        if constexpr (std::is_integral_v<T>)
            dst[i] = saturateCast<T>(v);
        else
            dst[i] = static_cast<T>(v);
    }
    return r;
}

bool valuesEqual(const Value &a, const Value &b, bool nanEqual)
{
    if (a.type() != b.type()) return false;
    if (a.dims().rows() != b.dims().rows() || a.dims().cols() != b.dims().cols()) return false;
    if (a.dims().is3D() != b.dims().is3D()) return false;
    if (a.dims().is3D() && a.dims().pages() != b.dims().pages()) return false;

    ValueType t = a.type();
    size_t n = a.numel();

    if (t == ValueType::DOUBLE) {
        const double *da = a.doubleData(), *db = b.doubleData();
        for (size_t i = 0; i < n; ++i) {
            if (da[i] == db[i]) continue;
            if (nanEqual && std::isnan(da[i]) && std::isnan(db[i])) continue;
            return false;
        }
        return true;
    }
    if (t == ValueType::SINGLE) {
        const float *fa = a.singleData(), *fb = b.singleData();
        for (size_t i = 0; i < n; ++i) {
            if (fa[i] == fb[i]) continue;
            if (nanEqual && std::isnan(fa[i]) && std::isnan(fb[i])) continue;
            return false;
        }
        return true;
    }
    if (t == ValueType::COMPLEX) {
        const Complex *ca = a.complexData(), *cb = b.complexData();
        for (size_t i = 0; i < n; ++i) {
            if (ca[i] == cb[i]) continue;
            if (nanEqual) {
                bool rEq = (ca[i].real() == cb[i].real())
                           || (std::isnan(ca[i].real()) && std::isnan(cb[i].real()));
                bool iEq = (ca[i].imag() == cb[i].imag())
                           || (std::isnan(ca[i].imag()) && std::isnan(cb[i].imag()));
                if (rEq && iEq) continue;
            }
            return false;
        }
        return true;
    }
    if (t == ValueType::CHAR)
        return std::memcmp(a.charData(), b.charData(), n) == 0;
    if (t == ValueType::LOGICAL)
        return std::memcmp(a.logicalData(), b.logicalData(), n) == 0;
    if (isIntegerType(t))
        return std::memcmp(a.rawData(), b.rawData(), n * elementSize(t)) == 0;
    if (t == ValueType::CELL) {
        for (size_t i = 0; i < n; ++i)
            if (!valuesEqual(a.cellAt(i), b.cellAt(i), nanEqual)) return false;
        return true;
    }
    if (t == ValueType::STRUCT) {
        auto &fa = a.structFields(), &fb = b.structFields();
        if (fa.size() != fb.size()) return false;
        for (auto &[k, v] : fa) {
            auto it = fb.find(k);
            if (it == fb.end()) return false;
            if (!valuesEqual(v, it->second, nanEqual)) return false;
        }
        return true;
    }
    if (t == ValueType::STRING)
        return a.toString() == b.toString();
    return false;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public API — numeric constructors
// ════════════════════════════════════════════════════════════════════════

Value toDouble(std::pmr::memory_resource *mr, const Value &x)
{
    return numericConstructor<double>(ValueType::DOUBLE, x, mr);
}

Value single(std::pmr::memory_resource *mr, const Value &x)
{
    return numericConstructor<float>(ValueType::SINGLE, x, mr);
}

Value int8(std::pmr::memory_resource *mr, const Value &x)   { return numericConstructor<int8_t>(ValueType::INT8, x, mr); }
Value int16(std::pmr::memory_resource *mr, const Value &x)  { return numericConstructor<int16_t>(ValueType::INT16, x, mr); }
Value int32(std::pmr::memory_resource *mr, const Value &x)  { return numericConstructor<int32_t>(ValueType::INT32, x, mr); }
Value int64(std::pmr::memory_resource *mr, const Value &x)  { return numericConstructor<int64_t>(ValueType::INT64, x, mr); }
Value uint8(std::pmr::memory_resource *mr, const Value &x)  { return numericConstructor<uint8_t>(ValueType::UINT8, x, mr); }
Value uint16(std::pmr::memory_resource *mr, const Value &x) { return numericConstructor<uint16_t>(ValueType::UINT16, x, mr); }
Value uint32(std::pmr::memory_resource *mr, const Value &x) { return numericConstructor<uint32_t>(ValueType::UINT32, x, mr); }
Value uint64(std::pmr::memory_resource *mr, const Value &x) { return numericConstructor<uint64_t>(ValueType::UINT64, x, mr); }

Value logical(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isLogical())
        return x;
    if (x.isScalar())
        return Value::logicalScalar(x.toScalar() != 0, p);
    Value r = createLike(x, ValueType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = x.elemAsDouble(i) != 0 ? 1 : 0;
    return r;
}

// ════════════════════════════════════════════════════════════════════════
// Public API — type predicates
// ════════════════════════════════════════════════════════════════════════

Value isnumeric(std::pmr::memory_resource *mr, const Value &x) { return Value::logicalScalar(x.isNumeric(), mr); }
Value islogical(std::pmr::memory_resource *mr, const Value &x) { return Value::logicalScalar(x.isLogical(), mr); }
Value ischar(std::pmr::memory_resource *mr, const Value &x)    { return Value::logicalScalar(x.isChar(), mr); }
Value isstring(std::pmr::memory_resource *mr, const Value &x)  { return Value::logicalScalar(x.isString(), mr); }
Value iscell(std::pmr::memory_resource *mr, const Value &x)    { return Value::logicalScalar(x.isCell(), mr); }
Value isstruct(std::pmr::memory_resource *mr, const Value &x)  { return Value::logicalScalar(x.isStruct(), mr); }
Value isempty(std::pmr::memory_resource *mr, const Value &x)   { return Value::logicalScalar(x.isEmpty(), mr); }
Value isscalar(std::pmr::memory_resource *mr, const Value &x)  { return Value::logicalScalar(x.isScalar(), mr); }
Value isreal(std::pmr::memory_resource *mr, const Value &x)    { return Value::logicalScalar(!x.isComplex(), mr); }
Value isinteger(std::pmr::memory_resource *mr, const Value &x) { return Value::logicalScalar(isIntegerType(x.type()), mr); }
Value isfloat(std::pmr::memory_resource *mr, const Value &x)   { return Value::logicalScalar(isFloatType(x.type()), mr); }
Value issingle(std::pmr::memory_resource *mr, const Value &x)  { return Value::logicalScalar(x.type() == ValueType::SINGLE, mr); }

Value isnan(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isnan(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = std::isnan(x.doubleData()[i]) ? 1 : 0;
    return r;
}

Value isinf(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isinf(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = std::isinf(x.doubleData()[i]) ? 1 : 0;
    return r;
}

Value isfinite(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isfinite(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = std::isfinite(x.doubleData()[i]) ? 1 : 0;
    return r;
}

// ════════════════════════════════════════════════════════════════════════
// Public API — equality + introspection
// ════════════════════════════════════════════════════════════════════════

Value isequal(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return Value::logicalScalar(valuesEqual(a, b, false), mr);
}

Value isequaln(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return Value::logicalScalar(valuesEqual(a, b, true), mr);
}

Value classOf(std::pmr::memory_resource *mr, const Value &x)
{
    return Value::fromString(mtypeName(x.type()), mr);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

// Numeric-constructor adapters need the zero-arg MATLAB form:
// double(), int32(), etc. → scalar zero of that type.
template <typename T, ValueType targetType>
void numericConstructor_reg(Span<const Value> args, size_t, Span<Value> outs,
                            CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        auto r = Value::matrix(1, 1, targetType, mr);
        *static_cast<T *>(r.rawDataMut()) = static_cast<T>(0);
        outs[0] = std::move(r);
        return;
    }
    outs[0] = numericConstructor<T>(targetType, args[0], mr);
}

void double_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<double, ValueType::DOUBLE>(args, n, outs, ctx); }

void single_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<float, ValueType::SINGLE>(args, n, outs, ctx); }

void int8_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int8_t, ValueType::INT8>(args, n, outs, ctx); }
void int16_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int16_t, ValueType::INT16>(args, n, outs, ctx); }
void int32_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int32_t, ValueType::INT32>(args, n, outs, ctx); }
void int64_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<int64_t, ValueType::INT64>(args, n, outs, ctx); }
void uint8_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint8_t, ValueType::UINT8>(args, n, outs, ctx); }
void uint16_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint16_t, ValueType::UINT16>(args, n, outs, ctx); }
void uint32_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint32_t, ValueType::UINT32>(args, n, outs, ctx); }
void uint64_reg(Span<const Value> args, size_t n, Span<Value> outs, CallContext &ctx)
{ numericConstructor_reg<uint64_t, ValueType::UINT64>(args, n, outs, ctx); }

void logical_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("logical: requires 1 argument", 0, 0, "logical", "",
                     "m:logical:nargin");
    outs[0] = logical(ctx.engine->resource(), args[0]);
}

// ── Simple predicate adapters ────────────────────────────────────────────
#define NK_PRED_REG(FN)                                                             \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,               \
                  CallContext &ctx)                                                 \
    {                                                                               \
        if (args.empty())                                                           \
            throw Error(#FN ": requires 1 argument", 0, 0, #FN, "",                \
                         "m:" #FN ":nargin");                                  \
        outs[0] = FN(ctx.engine->resource(), args[0]);                             \
    }

NK_PRED_REG(isnumeric)
NK_PRED_REG(islogical)
NK_PRED_REG(ischar)
NK_PRED_REG(isstring)
NK_PRED_REG(iscell)
NK_PRED_REG(isstruct)
NK_PRED_REG(isempty)
NK_PRED_REG(isscalar)
NK_PRED_REG(isreal)
NK_PRED_REG(isinteger)
NK_PRED_REG(isfloat)
NK_PRED_REG(issingle)
NK_PRED_REG(isnan)
NK_PRED_REG(isinf)
NK_PRED_REG(isfinite)

#undef NK_PRED_REG

void isequal_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isequal requires at least 2 arguments", 0, 0, "isequal", "",
                     "m:isequal:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], false);
    outs[0] = Value::logicalScalar(eq, ctx.engine->resource());
}

void isequaln_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isequaln requires at least 2 arguments", 0, 0, "isequaln", "",
                     "m:isequaln:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], true);
    outs[0] = Value::logicalScalar(eq, ctx.engine->resource());
}

void class_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("class: requires 1 argument", 0, 0, "class", "",
                     "m:class:nargin");
    outs[0] = classOf(ctx.engine->resource(), args[0]);
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keep the BuiltinLibrary::registerTypeFunctions hook empty;
// actual wiring happens in library.cpp via Phase-6c function pointers.
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerTypeFunctions(Engine &)
{
    // Intentionally empty — see BuiltinLibrary::install() in library.cpp.
}

} // namespace numkit
