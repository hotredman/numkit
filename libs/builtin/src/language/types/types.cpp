// libs/builtin/src/datatypes/numeric/types.cpp

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/types/types.hpp>

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

// ── Shape predicates ─────────────────────────────────────────────────

Value isvector(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && (d.rows() == 1 || d.cols() == 1) && x.numel() > 0;
    return Value::logicalScalar(tf, mr);
}

Value isrow(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && d.rows() == 1;
    return Value::logicalScalar(tf, mr);
}

Value iscolumn(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && d.cols() == 1;
    return Value::logicalScalar(tf, mr);
}

Value ismatrix(std::pmr::memory_resource *mr, const Value &x)
{
    return Value::logicalScalar(x.dims().ndim() <= 2, mr);
}

// ── Order predicates ─────────────────────────────────────────────────

namespace {
enum class SortMode { Ascend, Descend, Monotonic, StrictAscend, StrictDescend };

inline bool readSortMode(const Value *m, SortMode &out)
{
    out = SortMode::Ascend;
    if (!m) return true;
    if (!m->isChar() && !m->isString()) return false;
    auto s = m->toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "ascend")        { out = SortMode::Ascend;        return true; }
    if (s == "descend")       { out = SortMode::Descend;       return true; }
    if (s == "monotonic")     { out = SortMode::Monotonic;     return true; }
    if (s == "strictascend")  { out = SortMode::StrictAscend;  return true; }
    if (s == "strictdescend") { out = SortMode::StrictDescend; return true; }
    return false;
}

// Returns true if values [first, first+n) are sorted under `mode`.
inline bool runSorted(const double *first, size_t n, SortMode mode)
{
    if (n < 2) return true;
    // NaN anywhere → not sorted (matches MATLAB issorted behaviour for
    // simple ascend/descend modes).
    for (size_t i = 0; i < n; ++i)
        if (std::isnan(first[i])) return false;

    switch (mode) {
    case SortMode::Ascend:
        for (size_t i = 1; i < n; ++i) if (first[i] < first[i-1]) return false;
        return true;
    case SortMode::Descend:
        for (size_t i = 1; i < n; ++i) if (first[i] > first[i-1]) return false;
        return true;
    case SortMode::StrictAscend:
        for (size_t i = 1; i < n; ++i) if (first[i] <= first[i-1]) return false;
        return true;
    case SortMode::StrictDescend:
        for (size_t i = 1; i < n; ++i) if (first[i] >= first[i-1]) return false;
        return true;
    case SortMode::Monotonic: {
        bool asc = true, desc = true;
        for (size_t i = 1; i < n; ++i) {
            if (first[i] < first[i-1]) asc = false;
            if (first[i] > first[i-1]) desc = false;
        }
        return asc || desc;
    }
    }
    return true; // unreachable
}
} // anon

Value issorted(std::pmr::memory_resource *mr, const Value &x, const Value *mode)
{
    SortMode m = SortMode::Ascend;
    if (!readSortMode(mode, m))
        throw std::runtime_error("issorted: unrecognized sort mode");
    if (x.isEmpty() || x.isScalar())
        return Value::logicalScalar(true, mr);

    const auto &d = x.dims();
    const double *p = x.doubleData();
    if (d.ndim() <= 2 && (d.rows() == 1 || d.cols() == 1)) {
        return Value::logicalScalar(runSorted(p, x.numel(), m), mr);
    }
    // Matrix / 3-D / N-D: every column (along dim 1) must satisfy `mode`.
    const size_t r = d.rows();
    const size_t restCount = x.numel() / r;
    for (size_t k = 0; k < restCount; ++k) {
        if (!runSorted(p + k * r, r, m))
            return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(true, mr);
}

Value issortedrows(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty() || x.isScalar())
        return Value::logicalScalar(true, mr);
    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw std::runtime_error("issortedrows: input must be 2-D");
    const size_t R = d.rows(), C = d.cols();
    if (R < 2) return Value::logicalScalar(true, mr);
    const double *p = x.doubleData();
    auto getElem = [&](size_t row, size_t col) { return p[col * R + row]; };
    for (size_t i = 1; i < R; ++i) {
        for (size_t c = 0; c < C; ++c) {
            const double a = getElem(i - 1, c);
            const double b = getElem(i, c);
            if (std::isnan(a) || std::isnan(b))
                return Value::logicalScalar(false, mr);
            if (a < b) break;          // strictly less → ordered, next row.
            if (a > b)                 // strictly greater → out of order.
                return Value::logicalScalar(false, mr);
            // equal → look at next column.
        }
    }
    return Value::logicalScalar(true, mr);
}

Value isuniform(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty()) return Value::logicalScalar(true, mr);
    if (x.isScalar()) return Value::logicalScalar(true, mr);
    const size_t n = x.numel();
    const auto &d = x.dims();
    // Treat as 1-D: only meaningful for row/column vectors.
    if (d.ndim() > 2 || (d.rows() != 1 && d.cols() != 1))
        throw std::runtime_error("isuniform: input must be a vector");
    if (n < 2) return Value::logicalScalar(true, mr);
    const double *p = x.doubleData();
    for (size_t i = 0; i < n; ++i)
        if (!std::isfinite(p[i])) return Value::logicalScalar(false, mr);
    const double step = p[1] - p[0];
    // Tolerance: 4 * eps(max-magnitude in vector or step), follows MATLAB
    // isuniform's "approximately uniform" semantics.
    double scale = std::abs(step);
    for (size_t i = 0; i < n; ++i) scale = std::max(scale, std::abs(p[i]));
    const double tol = 4 * std::numeric_limits<double>::epsilon() * scale;
    for (size_t i = 1; i < n - 1; ++i) {
        const double s = p[i + 1] - p[i];
        if (std::abs(s - step) > tol)
            return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(true, mr);
}

// ── Numeric limits ───────────────────────────────────────────────────

namespace {
inline std::string readTypeName(const Value *t, const char *def)
{
    if (!t) return def;
    if (!t->isChar() && !t->isString())
        throw std::runtime_error("numeric-limit: type argument must be a string");
    auto s = t->toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

template <typename T>
inline Value typedScalar(std::pmr::memory_resource *mr, ValueType vt, T v)
{
    auto r = Value::matrix(1, 1, vt, mr);
    *static_cast<T *>(r.rawDataMut()) = v;
    return r;
}
} // anon

Value flintmax(std::pmr::memory_resource *mr, const Value *t)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(mr, ValueType::SINGLE, static_cast<float>(1u << 24));
    if (name == "double")
        return Value::scalar(9007199254740992.0, mr);  // 2^53 exactly
    throw std::runtime_error("flintmax: type must be 'double' or 'single'");
}

Value intmax(std::pmr::memory_resource *mr, const Value *t)
{
    auto name = readTypeName(t, "int32");
    if (name == "int8")   return typedScalar<int8_t>(mr,   ValueType::INT8,   std::numeric_limits<int8_t>::max());
    if (name == "int16")  return typedScalar<int16_t>(mr,  ValueType::INT16,  std::numeric_limits<int16_t>::max());
    if (name == "int32")  return typedScalar<int32_t>(mr,  ValueType::INT32,  std::numeric_limits<int32_t>::max());
    if (name == "int64")  return typedScalar<int64_t>(mr,  ValueType::INT64,  std::numeric_limits<int64_t>::max());
    if (name == "uint8")  return typedScalar<uint8_t>(mr,  ValueType::UINT8,  std::numeric_limits<uint8_t>::max());
    if (name == "uint16") return typedScalar<uint16_t>(mr, ValueType::UINT16, std::numeric_limits<uint16_t>::max());
    if (name == "uint32") return typedScalar<uint32_t>(mr, ValueType::UINT32, std::numeric_limits<uint32_t>::max());
    if (name == "uint64") return typedScalar<uint64_t>(mr, ValueType::UINT64, std::numeric_limits<uint64_t>::max());
    throw std::runtime_error("intmax: unsupported integer class");
}

Value intmin(std::pmr::memory_resource *mr, const Value *t)
{
    auto name = readTypeName(t, "int32");
    if (name == "int8")   return typedScalar<int8_t>(mr,   ValueType::INT8,   std::numeric_limits<int8_t>::min());
    if (name == "int16")  return typedScalar<int16_t>(mr,  ValueType::INT16,  std::numeric_limits<int16_t>::min());
    if (name == "int32")  return typedScalar<int32_t>(mr,  ValueType::INT32,  std::numeric_limits<int32_t>::min());
    if (name == "int64")  return typedScalar<int64_t>(mr,  ValueType::INT64,  std::numeric_limits<int64_t>::min());
    if (name == "uint8")  return typedScalar<uint8_t>(mr,  ValueType::UINT8,  static_cast<uint8_t>(0));
    if (name == "uint16") return typedScalar<uint16_t>(mr, ValueType::UINT16, static_cast<uint16_t>(0));
    if (name == "uint32") return typedScalar<uint32_t>(mr, ValueType::UINT32, static_cast<uint32_t>(0));
    if (name == "uint64") return typedScalar<uint64_t>(mr, ValueType::UINT64, static_cast<uint64_t>(0));
    throw std::runtime_error("intmin: unsupported integer class");
}

Value realmax(std::pmr::memory_resource *mr, const Value *t)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(mr, ValueType::SINGLE, std::numeric_limits<float>::max());
    if (name == "double")
        return Value::scalar(std::numeric_limits<double>::max(), mr);
    throw std::runtime_error("realmax: type must be 'double' or 'single'");
}

Value realmin(std::pmr::memory_resource *mr, const Value *t)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(mr, ValueType::SINGLE, std::numeric_limits<float>::min());
    if (name == "double")
        return Value::scalar(std::numeric_limits<double>::min(), mr);
    throw std::runtime_error("realmin: type must be 'double' or 'single'");
}

// ── Whole-array float predicates ────────────────────────────────────

Value allfinite(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty()) return Value::logicalScalar(true, mr);
    if (!x.isNumeric()) return Value::logicalScalar(false, mr);
    if (x.isScalar()) {
        if (x.isComplex()) {
            const auto c = x.toComplex();
            return Value::logicalScalar(std::isfinite(c.real()) && std::isfinite(c.imag()), mr);
        }
        return Value::logicalScalar(std::isfinite(x.toScalar()), mr);
    }
    const size_t n = x.numel();
    if (x.isComplex()) {
        const Complex *p = x.complexData();
        for (size_t i = 0; i < n; ++i)
            if (!std::isfinite(p[i].real()) || !std::isfinite(p[i].imag()))
                return Value::logicalScalar(false, mr);
        return Value::logicalScalar(true, mr);
    }
    if (isFloatType(x.type())) {
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i)
            if (!std::isfinite(p[i])) return Value::logicalScalar(false, mr);
        return Value::logicalScalar(true, mr);
    }
    // Integer / logical types are always finite.
    return Value::logicalScalar(true, mr);
}

Value anynan(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty()) return Value::logicalScalar(false, mr);
    if (!x.isNumeric()) return Value::logicalScalar(false, mr);
    if (x.isScalar()) {
        if (x.isComplex()) {
            const auto c = x.toComplex();
            return Value::logicalScalar(std::isnan(c.real()) || std::isnan(c.imag()), mr);
        }
        return Value::logicalScalar(std::isnan(x.toScalar()), mr);
    }
    const size_t n = x.numel();
    if (x.isComplex()) {
        const Complex *p = x.complexData();
        for (size_t i = 0; i < n; ++i)
            if (std::isnan(p[i].real()) || std::isnan(p[i].imag()))
                return Value::logicalScalar(true, mr);
        return Value::logicalScalar(false, mr);
    }
    if (isFloatType(x.type())) {
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i)
            if (std::isnan(p[i])) return Value::logicalScalar(true, mr);
        return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(false, mr);
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
NK_PRED_REG(isvector)
NK_PRED_REG(isrow)
NK_PRED_REG(iscolumn)
NK_PRED_REG(ismatrix)
NK_PRED_REG(issortedrows)
NK_PRED_REG(isuniform)

#undef NK_PRED_REG

void issorted_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("issorted: requires 1 argument", 0, 0, "issorted", "",
                     "m:issorted:nargin");
    const Value *mode = (args.size() >= 2) ? &args[1] : nullptr;
    outs[0] = issorted(ctx.engine->resource(), args[0], mode);
}

// flintmax/intmax/intmin/realmax/realmin all share an "optional type-name
// string" calling convention; one adapter per fn keeps error messages
// useful.
#define NK_LIMIT_REG(FN)                                                              \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) \
    {                                                                                  \
        const Value *t = args.empty() ? nullptr : &args[0];                            \
        outs[0] = FN(ctx.engine->resource(), t);                                       \
    }

NK_LIMIT_REG(flintmax)
NK_LIMIT_REG(intmax)
NK_LIMIT_REG(intmin)
NK_LIMIT_REG(realmax)
NK_LIMIT_REG(realmin)

#undef NK_LIMIT_REG

void allfinite_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("allfinite: requires 1 argument", 0, 0, "allfinite", "",
                     "m:allfinite:nargin");
    outs[0] = allfinite(ctx.engine->resource(), args[0]);
}

void anynan_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("anynan: requires 1 argument", 0, 0, "anynan", "",
                     "m:anynan:nargin");
    outs[0] = anynan(ctx.engine->resource(), args[0]);
}

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
