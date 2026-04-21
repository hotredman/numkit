// libs/builtin/src/MStdTypes.cpp

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/MStdTypes.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace numkit::m::builtin {

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
MValue numericConstructor(MType targetType, const MValue &x, Allocator *alloc)
{
    if (x.type() == targetType)
        return x;
    const size_t n = x.numel();
    MValue r = createLike(x, targetType, alloc);
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

bool valuesEqual(const MValue &a, const MValue &b, bool nanEqual)
{
    if (a.type() != b.type()) return false;
    if (a.dims().rows() != b.dims().rows() || a.dims().cols() != b.dims().cols()) return false;
    if (a.dims().is3D() != b.dims().is3D()) return false;
    if (a.dims().is3D() && a.dims().pages() != b.dims().pages()) return false;

    MType t = a.type();
    size_t n = a.numel();

    if (t == MType::DOUBLE) {
        const double *da = a.doubleData(), *db = b.doubleData();
        for (size_t i = 0; i < n; ++i) {
            if (da[i] == db[i]) continue;
            if (nanEqual && std::isnan(da[i]) && std::isnan(db[i])) continue;
            return false;
        }
        return true;
    }
    if (t == MType::SINGLE) {
        const float *fa = a.singleData(), *fb = b.singleData();
        for (size_t i = 0; i < n; ++i) {
            if (fa[i] == fb[i]) continue;
            if (nanEqual && std::isnan(fa[i]) && std::isnan(fb[i])) continue;
            return false;
        }
        return true;
    }
    if (t == MType::COMPLEX) {
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
    if (t == MType::CHAR)
        return std::memcmp(a.charData(), b.charData(), n) == 0;
    if (t == MType::LOGICAL)
        return std::memcmp(a.logicalData(), b.logicalData(), n) == 0;
    if (isIntegerType(t))
        return std::memcmp(a.rawData(), b.rawData(), n * elementSize(t)) == 0;
    if (t == MType::CELL) {
        for (size_t i = 0; i < n; ++i)
            if (!valuesEqual(a.cellAt(i), b.cellAt(i), nanEqual)) return false;
        return true;
    }
    if (t == MType::STRUCT) {
        auto &fa = a.structFields(), &fb = b.structFields();
        if (fa.size() != fb.size()) return false;
        for (auto &[k, v] : fa) {
            auto it = fb.find(k);
            if (it == fb.end()) return false;
            if (!valuesEqual(v, it->second, nanEqual)) return false;
        }
        return true;
    }
    if (t == MType::STRING)
        return a.toString() == b.toString();
    return false;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public API — numeric constructors
// ════════════════════════════════════════════════════════════════════════

MValue toDouble(Allocator &alloc, const MValue &x)
{
    return numericConstructor<double>(MType::DOUBLE, x, &alloc);
}

MValue single(Allocator &alloc, const MValue &x)
{
    return numericConstructor<float>(MType::SINGLE, x, &alloc);
}

MValue int8(Allocator &alloc, const MValue &x)   { return numericConstructor<int8_t>(MType::INT8, x, &alloc); }
MValue int16(Allocator &alloc, const MValue &x)  { return numericConstructor<int16_t>(MType::INT16, x, &alloc); }
MValue int32(Allocator &alloc, const MValue &x)  { return numericConstructor<int32_t>(MType::INT32, x, &alloc); }
MValue int64(Allocator &alloc, const MValue &x)  { return numericConstructor<int64_t>(MType::INT64, x, &alloc); }
MValue uint8(Allocator &alloc, const MValue &x)  { return numericConstructor<uint8_t>(MType::UINT8, x, &alloc); }
MValue uint16(Allocator &alloc, const MValue &x) { return numericConstructor<uint16_t>(MType::UINT16, x, &alloc); }
MValue uint32(Allocator &alloc, const MValue &x) { return numericConstructor<uint32_t>(MType::UINT32, x, &alloc); }
MValue uint64(Allocator &alloc, const MValue &x) { return numericConstructor<uint64_t>(MType::UINT64, x, &alloc); }

MValue logical(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isLogical())
        return x;
    if (x.isScalar())
        return MValue::logicalScalar(x.toScalar() != 0, p);
    MValue r = createMatrix({x.dims().rows(), x.dims().cols(),
                             x.dims().is3D() ? x.dims().pages() : 0},
                            MType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = x.elemAsDouble(i) != 0 ? 1 : 0;
    return r;
}

// ════════════════════════════════════════════════════════════════════════
// Public API — type predicates
// ════════════════════════════════════════════════════════════════════════

MValue isnumeric(Allocator &alloc, const MValue &x) { return MValue::logicalScalar(x.isNumeric(), &alloc); }
MValue islogical(Allocator &alloc, const MValue &x) { return MValue::logicalScalar(x.isLogical(), &alloc); }
MValue ischar(Allocator &alloc, const MValue &x)    { return MValue::logicalScalar(x.isChar(), &alloc); }
MValue isstring(Allocator &alloc, const MValue &x)  { return MValue::logicalScalar(x.isString(), &alloc); }
MValue iscell(Allocator &alloc, const MValue &x)    { return MValue::logicalScalar(x.isCell(), &alloc); }
MValue isstruct(Allocator &alloc, const MValue &x)  { return MValue::logicalScalar(x.isStruct(), &alloc); }
MValue isempty(Allocator &alloc, const MValue &x)   { return MValue::logicalScalar(x.isEmpty(), &alloc); }
MValue isscalar(Allocator &alloc, const MValue &x)  { return MValue::logicalScalar(x.isScalar(), &alloc); }
MValue isreal(Allocator &alloc, const MValue &x)    { return MValue::logicalScalar(!x.isComplex(), &alloc); }
MValue isinteger(Allocator &alloc, const MValue &x) { return MValue::logicalScalar(isIntegerType(x.type()), &alloc); }
MValue isfloat(Allocator &alloc, const MValue &x)   { return MValue::logicalScalar(isFloatType(x.type()), &alloc); }
MValue issingle(Allocator &alloc, const MValue &x)  { return MValue::logicalScalar(x.type() == MType::SINGLE, &alloc); }

MValue isnan(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isScalar())
        return MValue::logicalScalar(std::isnan(x.toScalar()), p);
    auto r = createLike(x, MType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = std::isnan(x.doubleData()[i]) ? 1 : 0;
    return r;
}

MValue isinf(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isScalar())
        return MValue::logicalScalar(std::isinf(x.toScalar()), p);
    auto r = createLike(x, MType::LOGICAL, p);
    for (size_t i = 0; i < x.numel(); ++i)
        r.logicalDataMut()[i] = std::isinf(x.doubleData()[i]) ? 1 : 0;
    return r;
}

// ════════════════════════════════════════════════════════════════════════
// Public API — equality + introspection
// ════════════════════════════════════════════════════════════════════════

MValue isequal(Allocator &alloc, const MValue &a, const MValue &b)
{
    return MValue::logicalScalar(valuesEqual(a, b, false), &alloc);
}

MValue isequaln(Allocator &alloc, const MValue &a, const MValue &b)
{
    return MValue::logicalScalar(valuesEqual(a, b, true), &alloc);
}

MValue classOf(Allocator &alloc, const MValue &x)
{
    return MValue::fromString(mtypeName(x.type()), &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

// Numeric-constructor adapters need the zero-arg MATLAB form:
// double(), int32(), etc. → scalar zero of that type.
template <typename T, MType targetType>
void numericConstructor_reg(Span<const MValue> args, size_t, Span<MValue> outs,
                            CallContext &ctx)
{
    Allocator *alloc = &ctx.engine->allocator();
    if (args.empty()) {
        auto r = MValue::matrix(1, 1, targetType, alloc);
        *static_cast<T *>(r.rawDataMut()) = static_cast<T>(0);
        outs[0] = std::move(r);
        return;
    }
    outs[0] = numericConstructor<T>(targetType, args[0], alloc);
}

void double_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<double, MType::DOUBLE>(args, n, outs, ctx); }

void single_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<float, MType::SINGLE>(args, n, outs, ctx); }

void int8_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<int8_t, MType::INT8>(args, n, outs, ctx); }
void int16_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<int16_t, MType::INT16>(args, n, outs, ctx); }
void int32_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<int32_t, MType::INT32>(args, n, outs, ctx); }
void int64_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<int64_t, MType::INT64>(args, n, outs, ctx); }
void uint8_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<uint8_t, MType::UINT8>(args, n, outs, ctx); }
void uint16_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<uint16_t, MType::UINT16>(args, n, outs, ctx); }
void uint32_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<uint32_t, MType::UINT32>(args, n, outs, ctx); }
void uint64_reg(Span<const MValue> args, size_t n, Span<MValue> outs, CallContext &ctx)
{ numericConstructor_reg<uint64_t, MType::UINT64>(args, n, outs, ctx); }

void logical_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("logical: requires 1 argument", 0, 0, "logical", "",
                     "m:logical:nargin");
    outs[0] = logical(ctx.engine->allocator(), args[0]);
}

// ── Simple predicate adapters ────────────────────────────────────────────
#define NK_PRED_REG(FN)                                                             \
    void FN##_reg(Span<const MValue> args, size_t, Span<MValue> outs,               \
                  CallContext &ctx)                                                 \
    {                                                                               \
        if (args.empty())                                                           \
            throw MError(#FN ": requires 1 argument", 0, 0, #FN, "",                \
                         "m:" #FN ":nargin");                                  \
        outs[0] = FN(ctx.engine->allocator(), args[0]);                             \
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

#undef NK_PRED_REG

void isequal_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("isequal requires at least 2 arguments", 0, 0, "isequal", "",
                     "m:isequal:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], false);
    outs[0] = MValue::logicalScalar(eq, &ctx.engine->allocator());
}

void isequaln_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("isequaln requires at least 2 arguments", 0, 0, "isequaln", "",
                     "m:isequaln:nargin");
    bool eq = true;
    for (size_t i = 1; i < args.size() && eq; ++i)
        eq = valuesEqual(args[0], args[i], true);
    outs[0] = MValue::logicalScalar(eq, &ctx.engine->allocator());
}

void class_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("class: requires 1 argument", 0, 0, "class", "",
                     "m:class:nargin");
    outs[0] = classOf(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keep the StdLibrary::registerTypeFunctions hook empty;
// actual wiring happens in MStdLibrary.cpp via Phase-6c function pointers.
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerTypeFunctions(Engine &)
{
    // Intentionally empty — see StdLibrary::install() in MStdLibrary.cpp.
}

} // namespace numkit::m
