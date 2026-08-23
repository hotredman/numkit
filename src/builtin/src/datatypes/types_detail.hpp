// toolboxes/.../types_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by types.cpp + types_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/reductions.hpp>  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace numkit::builtin::detail {
// SIMD/portable cast + isfinite backend (defs in casts_{highway,portable}.cpp /
// isfinite_{highway,portable}.cpp) — needed by the numericConstructor template below.
void doubleToInt8 (const double *in, int8_t   *out, std::size_t n);
void doubleToInt16(const double *in, int16_t  *out, std::size_t n);
void doubleToInt32(const double *in, int32_t  *out, std::size_t n);
void doubleToInt64(const double *in, int64_t  *out, std::size_t n);
void doubleToUInt8 (const double *in, uint8_t  *out, std::size_t n);
void doubleToUInt16(const double *in, uint16_t *out, std::size_t n);
void doubleToUInt32(const double *in, uint32_t *out, std::size_t n);
void doubleToUInt64(const double *in, uint64_t *out, std::size_t n);
} // namespace numkit::builtin::detail

// isnan/isinf/isfinite backend lives in arithmetic → numkit::builtin::detail (C4).
namespace numkit::builtin::detail {
void doubleIsNaNLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsInfLoop(const double *in, uint8_t *out, std::size_t n);
void doubleIsFiniteLoop(const double *in, uint8_t *out, std::size_t n);
} // namespace numkit::builtin::detail

namespace numkit::builtin {

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

    // SIMD fast path: double → integer target. Skips the per-element
    // virtual dispatch through elemAsDouble. Only kicks in when source
    // is DOUBLE and there's actual data; falls through for other source
    // types (single, int*, logical, char) where the bulk of the cost
    // is type-conversion not iteration.
    if constexpr (std::is_integral_v<T>) {
        if (x.type() == ValueType::DOUBLE && n > 0) {
            const double *src = x.doubleData();
            if constexpr (std::is_same_v<T, int8_t>)        ::numkit::builtin::detail::doubleToInt8 (src, dst, n);
            else if constexpr (std::is_same_v<T, int16_t>)  ::numkit::builtin::detail::doubleToInt16(src, dst, n);
            else if constexpr (std::is_same_v<T, int32_t>)  ::numkit::builtin::detail::doubleToInt32(src, dst, n);
            else if constexpr (std::is_same_v<T, int64_t>)  ::numkit::builtin::detail::doubleToInt64(src, dst, n);
            else if constexpr (std::is_same_v<T, uint8_t>)  ::numkit::builtin::detail::doubleToUInt8 (src, dst, n);
            else if constexpr (std::is_same_v<T, uint16_t>) ::numkit::builtin::detail::doubleToUInt16(src, dst, n);
            else if constexpr (std::is_same_v<T, uint32_t>) ::numkit::builtin::detail::doubleToUInt32(src, dst, n);
            else if constexpr (std::is_same_v<T, uint64_t>) ::numkit::builtin::detail::doubleToUInt64(src, dst, n);
            return r;
        }
    }

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
namespace {

// True iff this element-type can hold "standard" missing (i.e. NaN).
inline bool typeHasMissing(ValueType t)
{
    return t == ValueType::DOUBLE || t == ValueType::SINGLE;
}

// Read element i as double for the comparison path; works for any
// numeric class (integer types convert losslessly; LOGICAL → 0/1).
inline double elemD(const Value &x, std::size_t i) { return x.elemAsDouble(i); }

} // anonymous
namespace {
enum class SortMode { Ascend, Descend, Monotonic, StrictAscend, StrictDescend };

inline bool readSortMode(const Value &m, SortMode &out)
{
    out = SortMode::Ascend;
    if (m.isEmpty()) return true;
    if (!m.isChar() && !m.isString()) return false;
    auto s = m.toString();
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
namespace {
inline std::string readTypeName(const Value &t, const char *def)
{
    if (t.isEmpty()) return def;
    if (!t.isChar() && !t.isString())
        throw std::runtime_error("numeric-limit: type argument must be a string");
    auto s = t.toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

template <typename T>
inline Value typedScalar(ValueType vt, T v, std::pmr::memory_resource *mr)
{
    auto r = Value::matrix(1, 1, vt, mr);
    *static_cast<T *>(r.rawDataMut()) = v;
    return r;
}
} // anon
namespace {

// Byte-swap helper. 1-byte types pass through; 2/4/8-byte types
// re-interpret as the matching unsigned int and reverse.
template <typename T>
T swapBytesScalar(T v)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if constexpr (sizeof(T) == 1) {
        return v;
    } else if constexpr (sizeof(T) == 2) {
        uint16_t u; std::memcpy(&u, &v, 2);
        u = static_cast<uint16_t>((u << 8) | (u >> 8));
        T r; std::memcpy(&r, &u, 2); return r;
    } else if constexpr (sizeof(T) == 4) {
        uint32_t u; std::memcpy(&u, &v, 4);
        u = ((u & 0xFF000000U) >> 24) | ((u & 0x00FF0000U) >> 8)
          | ((u & 0x0000FF00U) << 8)  | ((u & 0x000000FFU) << 24);
        T r; std::memcpy(&r, &u, 4); return r;
    } else {  // 8
        uint64_t u; std::memcpy(&u, &v, 8);
        u = ((u & 0xFF00000000000000ULL) >> 56) | ((u & 0x00FF000000000000ULL) >> 40)
          | ((u & 0x0000FF0000000000ULL) >> 24) | ((u & 0x000000FF00000000ULL) >> 8)
          | ((u & 0x00000000FF000000ULL) << 8)  | ((u & 0x0000000000FF0000ULL) << 24)
          | ((u & 0x000000000000FF00ULL) << 40) | ((u & 0x00000000000000FFULL) << 56);
        T r; std::memcpy(&r, &u, 8); return r;
    }
}

template <typename T>
Value swapBytesArray(const Value &x, std::pmr::memory_resource *mr)
{
    Value r = createLike(x, x.type(), mr);
    const T *src = static_cast<const T *>(x.rawData());
    T *dst = static_cast<T *>(r.rawDataMut());
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = swapBytesScalar(src[i]);
    return r;
}

} // namespace
namespace {

struct TypeInfo { ValueType vt; size_t elemSize; };

TypeInfo typeInfoFor(const std::string &classname)
{
    if (classname == "double")  return { ValueType::DOUBLE,  sizeof(double)   };
    if (classname == "single")  return { ValueType::SINGLE,  sizeof(float)    };
    if (classname == "int8")    return { ValueType::INT8,    sizeof(int8_t)   };
    if (classname == "int16")   return { ValueType::INT16,   sizeof(int16_t)  };
    if (classname == "int32")   return { ValueType::INT32,   sizeof(int32_t)  };
    if (classname == "int64")   return { ValueType::INT64,   sizeof(int64_t)  };
    if (classname == "uint8")   return { ValueType::UINT8,   sizeof(uint8_t)  };
    if (classname == "uint16")  return { ValueType::UINT16,  sizeof(uint16_t) };
    if (classname == "uint32")  return { ValueType::UINT32,  sizeof(uint32_t) };
    if (classname == "uint64")  return { ValueType::UINT64,  sizeof(uint64_t) };
    if (classname == "logical") return { ValueType::LOGICAL, sizeof(uint8_t)  };
    if (classname == "char")    return { ValueType::CHAR,    sizeof(char)     };
    return { ValueType::DOUBLE, 0u };  // sentinel: 0 size = unknown
}

size_t elemSizeOf(ValueType t)
{
    switch (t) {
    case ValueType::DOUBLE:  return sizeof(double);
    case ValueType::SINGLE:  return sizeof(float);
    case ValueType::COMPLEX: return sizeof(double) * 2;
    case ValueType::INT8:
    case ValueType::UINT8:
    case ValueType::LOGICAL:
    case ValueType::CHAR:    return 1;
    case ValueType::INT16:
    case ValueType::UINT16:  return 2;
    case ValueType::INT32:
    case ValueType::UINT32:  return 4;
    case ValueType::INT64:
    case ValueType::UINT64:  return 8;
    default:                 return 0;
    }
}

} // namespace

} // namespace numkit::builtin
