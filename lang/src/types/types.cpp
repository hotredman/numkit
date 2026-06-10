// toolboxes/builtin/src/datatypes/numeric/types.cpp

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/types/types.hpp>
#include <numkit/builtin/language/strings/strings.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// Forward-declare the SIMD/portable cast backend (defined in
// language/types/casts_{highway,portable}.cpp).
#include "types_detail.hpp"

namespace numkit::builtin::detail {
void doubleToInt8 (const double *in, int8_t   *out, std::size_t n);
void doubleToInt16(const double *in, int16_t  *out, std::size_t n);
void doubleToInt32(const double *in, int32_t  *out, std::size_t n);
void doubleToInt64(const double *in, int64_t  *out, std::size_t n);
void doubleToUInt8 (const double *in, uint8_t  *out, std::size_t n);
void doubleToUInt16(const double *in, uint16_t *out, std::size_t n);
void doubleToUInt32(const double *in, uint32_t *out, std::size_t n);
void doubleToUInt64(const double *in, uint64_t *out, std::size_t n);
} // namespace numkit::builtin::detail

// Forward-declare the isnan/isinf/isfinite backend (defined in
// math/arithmetic/isfinite_{highway,portable}.cpp — numkit::math::detail after C4).
namespace numkit::math::detail {
void doubleIsNaNLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsInfLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsFiniteLoop(const double *in, uint8_t *out, std::size_t n);
} // namespace numkit::math::detail

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Implementation helpers
// ════════════════════════════════════════════════════════════════════════


// ════════════════════════════════════════════════════════════════════════
// Public API — numeric constructors
// ════════════════════════════════════════════════════════════════════════

Value toDouble(const Value &x, std::pmr::memory_resource *mr)
{
    return numericConstructor<double>(ValueType::DOUBLE, x, mr);
}

Value single(const Value &x, std::pmr::memory_resource *mr)
{
    return numericConstructor<float>(ValueType::SINGLE, x, mr);
}

Value int8(const Value &x, std::pmr::memory_resource *mr)   { return numericConstructor<int8_t>(ValueType::INT8, x, mr); }
Value int16(const Value &x, std::pmr::memory_resource *mr)  { return numericConstructor<int16_t>(ValueType::INT16, x, mr); }
Value int32(const Value &x, std::pmr::memory_resource *mr)  { return numericConstructor<int32_t>(ValueType::INT32, x, mr); }
Value int64(const Value &x, std::pmr::memory_resource *mr)  { return numericConstructor<int64_t>(ValueType::INT64, x, mr); }
Value uint8(const Value &x, std::pmr::memory_resource *mr)  { return numericConstructor<uint8_t>(ValueType::UINT8, x, mr); }
Value uint16(const Value &x, std::pmr::memory_resource *mr) { return numericConstructor<uint16_t>(ValueType::UINT16, x, mr); }
Value uint32(const Value &x, std::pmr::memory_resource *mr) { return numericConstructor<uint32_t>(ValueType::UINT32, x, mr); }
Value uint64(const Value &x, std::pmr::memory_resource *mr) { return numericConstructor<uint64_t>(ValueType::UINT64, x, mr); }

Value logical(const Value &x, std::pmr::memory_resource *mr)
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

Value isnumeric(const Value &x, std::pmr::memory_resource *mr) { return Value::logicalScalar(x.isNumeric(), mr); }
Value islogical(const Value &x, std::pmr::memory_resource *mr) { return Value::logicalScalar(x.isLogical(), mr); }
Value ischar(const Value &x, std::pmr::memory_resource *mr)    { return Value::logicalScalar(x.isChar(), mr); }
Value isstring(const Value &x, std::pmr::memory_resource *mr)  { return Value::logicalScalar(x.isString(), mr); }
Value iscell(const Value &x, std::pmr::memory_resource *mr)    { return Value::logicalScalar(x.isCell(), mr); }
Value isstruct(const Value &x, std::pmr::memory_resource *mr)  { return Value::logicalScalar(x.isStruct(), mr); }
Value isempty(const Value &x, std::pmr::memory_resource *mr)   { return Value::logicalScalar(x.isEmpty(), mr); }
Value isscalar(const Value &x, std::pmr::memory_resource *mr)  { return Value::logicalScalar(x.isScalar(), mr); }
Value isreal(const Value &x, std::pmr::memory_resource *mr)    { return Value::logicalScalar(!x.isComplex(), mr); }
Value isinteger(const Value &x, std::pmr::memory_resource *mr) { return Value::logicalScalar(isIntegerType(x.type()), mr); }
Value isfloat(const Value &x, std::pmr::memory_resource *mr)   { return Value::logicalScalar(isFloatType(x.type()), mr); }
Value issingle(const Value &x, std::pmr::memory_resource *mr)  { return Value::logicalScalar(x.type() == ValueType::SINGLE, mr); }
// numkit has no sparse-matrix storage class -- issparse always returns
// false for any input. Matches MATLAB on dense inputs (which is all
// numkit can produce). Covers the FAIL row in PROGRESS for issparse.
Value issparse(const Value & /*x*/, std::pmr::memory_resource *mr) { return Value::logicalScalar(false, mr); }

Value isnan(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isnan(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    if (x.numel() == 0) return r;
    ::numkit::math::detail::doubleIsNaNLoop(x.doubleData(), r.logicalDataMut(), x.numel());
    return r;
}

Value isinf(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isinf(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    if (x.numel() == 0) return r;
    ::numkit::math::detail::doubleIsInfLoop(x.doubleData(), r.logicalDataMut(), x.numel());
    return r;
}

Value isfinite(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isScalar())
        return Value::logicalScalar(std::isfinite(x.toScalar()), p);
    auto r = createLike(x, ValueType::LOGICAL, p);
    if (x.numel() == 0) return r;
    ::numkit::math::detail::doubleIsFiniteLoop(x.doubleData(), r.logicalDataMut(), x.numel());
    return r;
}

// ── missing-value predicates ─────────────────────────────────────────


Value ismissing(const Value &x, const Value &indicator,
                std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    auto r = createLike(x, ValueType::LOGICAL, p);
    const std::size_t n = x.numel();
    if (n == 0) return r;

    std::uint8_t *out = r.logicalDataMut();

    // Build the indicator value list (empty → standard missing only).
    const bool have_ind = (!indicator.isEmpty());
    std::vector<double> ind_vals;
    if (have_ind) {
        ind_vals.reserve(indicator.numel());
        for (std::size_t k = 0; k < indicator.numel(); ++k)
            ind_vals.push_back(indicator.elemAsDouble(k));
    }
    const bool float_class = typeHasMissing(x.type());

    if (!have_ind) {
        // Standard missing only: NaN for float, never for ints/logical.
        if (float_class) {
            if (x.type() == ValueType::DOUBLE) {
                ::numkit::math::detail::doubleIsNaNLoop(x.doubleData(), out, n);
            } else {  // SINGLE
                const float *src = x.singleData();
                for (std::size_t i = 0; i < n; ++i)
                    out[i] = std::isnan(src[i]) ? 1u : 0u;
            }
        } else {
            std::fill(out, out + n, std::uint8_t{0});
        }
        return r;
    }

    // With indicator: ONLY values listed in `indicator` are missing
    // (NaN is NOT auto-flagged when an indicator is given — MATLAB
    // behaviour, probed). NaN entries in the indicator itself match
    // NaN values in `x` (special-cased because NaN != NaN).
    for (std::size_t i = 0; i < n; ++i) {
        const double xi = elemD(x, i);
        const bool xi_nan = std::isnan(xi);
        bool tf = false;
        for (double v : ind_vals) {
            if (std::isnan(v)) {
                if (xi_nan) { tf = true; break; }
            } else if (xi == v) {
                tf = true; break;
            }
        }
        out[i] = tf ? 1u : 0u;
    }
    return r;
}

Value standardizeMissing(const Value &x, const Value &indicator,
                         std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    const std::size_t n = x.numel();
    const ValueType T = x.type();

    // Integer / logical / char: no missing concept → return a copy.
    if (!typeHasMissing(T))
        return x;

    // Build indicator list (skip NaN — won't match anything via ==).
    std::vector<double> ind_vals;
    if (!indicator.isEmpty()) {
        ind_vals.reserve(indicator.numel());
        for (std::size_t k = 0; k < indicator.numel(); ++k) {
            const double v = indicator.elemAsDouble(k);
            if (!std::isnan(v)) ind_vals.push_back(v);
        }
    }

    Value out = createLike(x, T, p);
    if (n == 0) return out;

    if (T == ValueType::DOUBLE) {
        const double *src = x.doubleData();
        double *dst       = out.doubleDataMut();
        const double nan  = std::numeric_limits<double>::quiet_NaN();
        for (std::size_t i = 0; i < n; ++i) {
            const double v = src[i];
            bool hit = false;
            for (double ind : ind_vals) {
                if (v == ind) { hit = true; break; }
            }
            dst[i] = hit ? nan : v;
        }
    } else {  // SINGLE
        const float *src = x.singleData();
        float *dst       = out.singleDataMut();
        const float nan  = std::numeric_limits<float>::quiet_NaN();
        for (std::size_t i = 0; i < n; ++i) {
            const float v = src[i];
            bool hit = false;
            for (double ind : ind_vals) {
                if (v == static_cast<float>(ind)) { hit = true; break; }
            }
            dst[i] = hit ? nan : v;
        }
    }
    return out;
}

Value anymissing(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    const std::size_t n = x.numel();
    if (n == 0) return Value::logicalScalar(false, p);
    if (!typeHasMissing(x.type()))
        return Value::logicalScalar(false, p);
    if (x.type() == ValueType::DOUBLE) {
        const double *src = x.doubleData();
        for (std::size_t i = 0; i < n; ++i)
            if (std::isnan(src[i])) return Value::logicalScalar(true, p);
    } else {
        const float *src = x.singleData();
        for (std::size_t i = 0; i < n; ++i)
            if (std::isnan(src[i])) return Value::logicalScalar(true, p);
    }
    return Value::logicalScalar(false, p);
}

// ── Shape predicates ─────────────────────────────────────────────────

Value isvector(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && (d.rows() == 1 || d.cols() == 1) && x.numel() > 0;
    return Value::logicalScalar(tf, mr);
}

Value isrow(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && d.rows() == 1;
    return Value::logicalScalar(tf, mr);
}

Value iscolumn(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    bool tf = d.ndim() <= 2 && d.cols() == 1;
    return Value::logicalScalar(tf, mr);
}

Value ismatrix(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(x.dims().ndim() <= 2, mr);
}

// ── Order predicates ─────────────────────────────────────────────────


Value issorted(const Value &x, const Value &mode, std::pmr::memory_resource *mr)
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

Value issortedrows(const Value &x, std::pmr::memory_resource *mr)
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

Value isuniform(const Value &x, std::pmr::memory_resource *mr)
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


Value flintmax(const Value &t, std::pmr::memory_resource *mr)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(ValueType::SINGLE, static_cast<float>(1u << 24), mr);
    if (name == "double")
        return Value::scalar(9007199254740992.0, mr);  // 2^53 exactly
    throw std::runtime_error("flintmax: type must be 'double' or 'single'");
}

Value intmax(const Value &t, std::pmr::memory_resource *mr)
{
    auto name = readTypeName(t, "int32");
    if (name == "int8")   return typedScalar<int8_t>(ValueType::INT8, std::numeric_limits<int8_t>::max(), mr);
    if (name == "int16")  return typedScalar<int16_t>(ValueType::INT16, std::numeric_limits<int16_t>::max(), mr);
    if (name == "int32")  return typedScalar<int32_t>(ValueType::INT32, std::numeric_limits<int32_t>::max(), mr);
    if (name == "int64")  return typedScalar<int64_t>(ValueType::INT64, std::numeric_limits<int64_t>::max(), mr);
    if (name == "uint8")  return typedScalar<uint8_t>(ValueType::UINT8, std::numeric_limits<uint8_t>::max(), mr);
    if (name == "uint16") return typedScalar<uint16_t>(ValueType::UINT16, std::numeric_limits<uint16_t>::max(), mr);
    if (name == "uint32") return typedScalar<uint32_t>(ValueType::UINT32, std::numeric_limits<uint32_t>::max(), mr);
    if (name == "uint64") return typedScalar<uint64_t>(ValueType::UINT64, std::numeric_limits<uint64_t>::max(), mr);
    throw std::runtime_error("intmax: unsupported integer class");
}

Value intmin(const Value &t, std::pmr::memory_resource *mr)
{
    auto name = readTypeName(t, "int32");
    if (name == "int8")   return typedScalar<int8_t>(ValueType::INT8, std::numeric_limits<int8_t>::min(), mr);
    if (name == "int16")  return typedScalar<int16_t>(ValueType::INT16, std::numeric_limits<int16_t>::min(), mr);
    if (name == "int32")  return typedScalar<int32_t>(ValueType::INT32, std::numeric_limits<int32_t>::min(), mr);
    if (name == "int64")  return typedScalar<int64_t>(ValueType::INT64, std::numeric_limits<int64_t>::min(), mr);
    if (name == "uint8")  return typedScalar<uint8_t>(ValueType::UINT8, static_cast<uint8_t>(0), mr);
    if (name == "uint16") return typedScalar<uint16_t>(ValueType::UINT16, static_cast<uint16_t>(0), mr);
    if (name == "uint32") return typedScalar<uint32_t>(ValueType::UINT32, static_cast<uint32_t>(0), mr);
    if (name == "uint64") return typedScalar<uint64_t>(ValueType::UINT64, static_cast<uint64_t>(0), mr);
    throw std::runtime_error("intmin: unsupported integer class");
}

Value realmax(const Value &t, std::pmr::memory_resource *mr)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(ValueType::SINGLE, std::numeric_limits<float>::max(), mr);
    if (name == "double")
        return Value::scalar(std::numeric_limits<double>::max(), mr);
    throw std::runtime_error("realmax: type must be 'double' or 'single'");
}

Value realmin(const Value &t, std::pmr::memory_resource *mr)
{
    auto name = readTypeName(t, "double");
    if (name == "single")
        return typedScalar<float>(ValueType::SINGLE, std::numeric_limits<float>::min(), mr);
    if (name == "double")
        return Value::scalar(std::numeric_limits<double>::min(), mr);
    throw std::runtime_error("realmin: type must be 'double' or 'single'");
}

// ── Whole-array float predicates ────────────────────────────────────

Value allfinite(const Value &x, std::pmr::memory_resource *mr)
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

Value anynan(const Value &x, std::pmr::memory_resource *mr)
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

Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(valuesEqual(a, b, false), mr);
}

Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(valuesEqual(a, b, true), mr);
}

Value classOf(const Value &x, std::pmr::memory_resource *mr)
{
    // OBJECT instances report their registered class name, not "object".
    if (x.isObject())
        return Value::fromString(x.objectClassName(), mr);
    return Value::fromString(mtypeName(x.type()), mr);
}

// ── Pack 36: cast + swapbytes ────────────────────────────────────────
Value cast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr)
{
    if (classname == "double")  return toDouble(x, mr);
    if (classname == "single")  return single(x, mr);
    if (classname == "int8")    return int8(x, mr);
    if (classname == "int16")   return int16(x, mr);
    if (classname == "int32")   return int32(x, mr);
    if (classname == "int64")   return int64(x, mr);
    if (classname == "uint8")   return uint8(x, mr);
    if (classname == "uint16")  return uint16(x, mr);
    if (classname == "uint32")  return uint32(x, mr);
    if (classname == "uint64")  return uint64(x, mr);
    if (classname == "logical") return logical(x, mr);
    if (classname == "char")    return toChar(x, mr);
    if (classname == "string")  return toString(x, mr);
    throw Error("cast: unsupported class '" + classname + "'",
                 0, 0, "cast", "", "numkit:cast:badClass");
}


// Map a MATLAB classname string → (ValueType, element-size-in-bytes).

Value typecast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr)
{
    TypeInfo info = typeInfoFor(classname);
    if (info.elemSize == 0)
        throw Error("typecast: unsupported class '" + classname + "'",
                     0, 0, "typecast", "", "numkit:typecast:badClass");

    const size_t srcElemSize = elemSizeOf(x.type());
    if (srcElemSize == 0)
        throw Error("typecast: input type does not have a contiguous byte buffer",
                     0, 0, "typecast", "", "numkit:typecast:badInputType");

    const size_t totalBytes = x.numel() * srcElemSize;
    if (totalBytes % info.elemSize != 0)
        throw Error("typecast: input byte count must be a multiple of the "
                    "destination element size",
                     0, 0, "typecast", "", "numkit:typecast:badSize");
    const size_t newCount = totalBytes / info.elemSize;

    // Output is always a row vector (matches MATLAB).
    Value out = Value::matrix(1, newCount, info.vt, mr);
    if (totalBytes > 0)
        std::memcpy(out.rawDataMut(), x.rawData(), totalBytes);
    return out;
}

Value swapbytes(const Value &x, std::pmr::memory_resource *mr)
{
    switch (x.type()) {
    case ValueType::INT8:    return int8(x, mr);     // copy through (1 byte = identity)
    case ValueType::UINT8:   return uint8(x, mr);
    case ValueType::LOGICAL: return logical(x, mr);
    case ValueType::INT16:   return swapBytesArray<int16_t>(x, mr);
    case ValueType::UINT16:  return swapBytesArray<uint16_t>(x, mr);
    case ValueType::INT32:   return swapBytesArray<int32_t>(x, mr);
    case ValueType::UINT32:  return swapBytesArray<uint32_t>(x, mr);
    case ValueType::INT64:   return swapBytesArray<int64_t>(x, mr);
    case ValueType::UINT64:  return swapBytesArray<uint64_t>(x, mr);
    case ValueType::SINGLE:  return swapBytesArray<float>(x, mr);
    case ValueType::DOUBLE:  return swapBytesArray<double>(x, mr);
    default:
        throw Error("swapbytes: input must be a numeric or logical array",
                     0, 0, "swapbytes", "", "numkit:swapbytes:badType");
    }
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════


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
