#pragma once

#include "MLabEngine.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mlab {

// ============================================================
// Helper: promote pair to complex if needed
// ============================================================
inline std::pair<MValue, MValue> promoteToComplex(const MValue &a, const MValue &b, Allocator *alloc)
{
    MValue ca = a, cb = b;
    if (a.isComplex() && !b.isComplex())
        cb.promoteToComplex(alloc);
    else if (!a.isComplex() && b.isComplex())
        ca.promoteToComplex(alloc);
    return {ca, cb};
}

// ============================================================
// Elementwise binary op on double arrays
// ============================================================
template<typename Op>
MValue elementwiseDouble(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    // MATLAB: empty propagation — any op with [] returns []
    if (a.isEmpty() && b.isEmpty())
        return MValue::empty();
    if (a.isEmpty() || b.isEmpty()) {
        // [] + scalar or scalar + [] → []
        if (a.isScalar() || b.isScalar())
            return MValue::empty();
        throw std::runtime_error("Matrix dimensions must agree");
    }
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(op(a.toScalar(), b.toScalar()), alloc);
    if (a.isScalar()) {
        auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::DOUBLE, alloc);
        double av = a.toScalar();
        for (size_t i = 0; i < b.numel(); ++i)
            r.doubleDataMut()[i] = op(av, b.doubleData()[i]);
        return r;
    }
    if (b.isScalar()) {
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
        double bv = b.toScalar();
        for (size_t i = 0; i < a.numel(); ++i)
            r.doubleDataMut()[i] = op(a.doubleData()[i], bv);
        return r;
    }
    if (a.dims() != b.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i], b.doubleData()[i]);
    return r;
}

// ============================================================
// Elementwise binary op on complex arrays
// ============================================================
template<typename Op>
MValue elementwiseComplex(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    // MATLAB: empty propagation
    if (a.isEmpty() || b.isEmpty())
        return MValue::empty();
    auto [ca, cb] = promoteToComplex(a, b, alloc);
    if (ca.isScalar() && cb.isScalar())
        return MValue::complexScalar(op(ca.toComplex(), cb.toComplex()), alloc);
    if (ca.isScalar()) {
        auto r = MValue::complexMatrix(cb.dims().rows(), cb.dims().cols(), alloc);
        Complex av = ca.toComplex();
        for (size_t i = 0; i < cb.numel(); ++i)
            r.complexDataMut()[i] = op(av, cb.complexData()[i]);
        return r;
    }
    if (cb.isScalar()) {
        auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
        Complex bv = cb.toComplex();
        for (size_t i = 0; i < ca.numel(); ++i)
            r.complexDataMut()[i] = op(ca.complexData()[i], bv);
        return r;
    }
    if (ca.dims() != cb.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
    for (size_t i = 0; i < ca.numel(); ++i)
        r.complexDataMut()[i] = op(ca.complexData()[i], cb.complexData()[i]);
    return r;
}

// ============================================================
// Elementwise unary on double
// ============================================================
template<typename Op>
MValue unaryDouble(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::scalar(op(a.toScalar()), alloc);
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i]);
    return r;
}

// ============================================================
// Elementwise unary on complex
// ============================================================
template<typename Op>
MValue unaryComplex(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::complexScalar(op(a.toComplex()), alloc);
    auto r = MValue::complexMatrix(a.dims().rows(), a.dims().cols(), alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.complexDataMut()[i] = op(a.complexData()[i]);
    return r;
}

// ============================================================
// Saturating arithmetic for integer types
// ============================================================

template <typename T>
inline T saturateAdd(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>) {
        return a + b;
    } else if constexpr (std::is_unsigned_v<T>) {
        T r = a + b;
        return (r < a) ? std::numeric_limits<T>::max() : r;
    } else {
        using W = int64_t;
        W r = static_cast<W>(a) + static_cast<W>(b);
        if (r > std::numeric_limits<T>::max()) return std::numeric_limits<T>::max();
        if (r < std::numeric_limits<T>::min()) return std::numeric_limits<T>::min();
        return static_cast<T>(r);
    }
}

template <typename T>
inline T saturateSub(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>) {
        return a - b;
    } else if constexpr (std::is_unsigned_v<T>) {
        return (b > a) ? T(0) : a - b;
    } else {
        using W = int64_t;
        W r = static_cast<W>(a) - static_cast<W>(b);
        if (r > std::numeric_limits<T>::max()) return std::numeric_limits<T>::max();
        if (r < std::numeric_limits<T>::min()) return std::numeric_limits<T>::min();
        return static_cast<T>(r);
    }
}

template <typename T>
inline T saturateMul(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>) {
        return a * b;
    } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
        double r = static_cast<double>(a) * static_cast<double>(b);
        if (r > static_cast<double>(std::numeric_limits<T>::max())) return std::numeric_limits<T>::max();
        if (r < static_cast<double>(std::numeric_limits<T>::min())) return std::numeric_limits<T>::min();
        return a * b;
    } else {
        using W = int64_t;
        W r = static_cast<W>(a) * static_cast<W>(b);
        if (r > std::numeric_limits<T>::max()) return std::numeric_limits<T>::max();
        if (r < std::numeric_limits<T>::min()) return std::numeric_limits<T>::min();
        return static_cast<T>(r);
    }
}

template <typename T>
inline T saturateDiv(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>) {
        return a / b;
    } else {
        if (b == 0) {
            if constexpr (std::is_unsigned_v<T>)
                return (a == 0) ? T(0) : std::numeric_limits<T>::max();
            else
                return (a > 0) ? std::numeric_limits<T>::max()
                     : (a < 0) ? std::numeric_limits<T>::min()
                               : T(0);
        }
        if constexpr (!std::is_unsigned_v<T>) {
            if (a == std::numeric_limits<T>::min() && b == T(-1))
                return std::numeric_limits<T>::max();
        }
        // MATLAB integer division: round result
        double r = static_cast<double>(a) / static_cast<double>(b);
        r = std::round(r);
        if (r > static_cast<double>(std::numeric_limits<T>::max())) return std::numeric_limits<T>::max();
        if (r < static_cast<double>(std::numeric_limits<T>::min())) return std::numeric_limits<T>::min();
        return static_cast<T>(r);
    }
}

template <typename T>
inline T saturateNeg(T a)
{
    if constexpr (std::is_floating_point_v<T>) {
        return -a;
    } else if constexpr (std::is_unsigned_v<T>) {
        return T(0);
    } else {
        if (a == std::numeric_limits<T>::min()) return std::numeric_limits<T>::max();
        return -a;
    }
}

// ============================================================
// Elementwise binary op on typed arrays (integer/single)
// ============================================================

template <typename T, typename Op>
MValue elementwiseTyped(const MValue &a, const MValue &b, MType targetType, Op op, Allocator *alloc)
{
    auto readElem = [](const MValue &v, MType tgt, size_t i) -> T {
        if (v.type() == tgt) return static_cast<const T *>(v.rawData())[i];
        // Convert from double (or other numeric) to target type
        double d = v.toScalar();
        if (v.numel() > 1) d = v.doubleData()[i];
        if constexpr (std::is_integral_v<T>)
            return static_cast<T>(std::clamp(std::round(d),
                static_cast<double>(std::numeric_limits<T>::min()),
                static_cast<double>(std::numeric_limits<T>::max())));
        else
            return static_cast<T>(d);
    };
    auto readA = [&](size_t i) -> T { return readElem(a, targetType, i); };
    auto readB = [&](size_t i) -> T { return readElem(b, targetType, i); };

    if (a.isScalar() && b.isScalar()) {
        auto r = MValue::matrix(1, 1, targetType, alloc);
        *static_cast<T *>(r.rawDataMut()) = op(readA(0), readB(0));
        return r;
    }
    if (a.isScalar()) {
        auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), targetType, alloc);
        T av = readA(0);
        T *dst = static_cast<T *>(r.rawDataMut());
        for (size_t i = 0; i < b.numel(); ++i)
            dst[i] = op(av, readB(i));
        return r;
    }
    if (b.isScalar()) {
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), targetType, alloc);
        T bv = readB(0);
        T *dst = static_cast<T *>(r.rawDataMut());
        for (size_t i = 0; i < a.numel(); ++i)
            dst[i] = op(readA(i), bv);
        return r;
    }
    if (a.dims() != b.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), targetType, alloc);
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t i = 0; i < a.numel(); ++i)
        dst[i] = op(readA(i), readB(i));
    return r;
}

// ============================================================
// Dispatch integer/single binary op by MType
// ============================================================

// Resolve the target integer type from a pair.
// MATLAB rules: intN + double → intN, intN + intN → intN, intN + intM → error
inline MType resolveIntegerPairType(const MValue &a, const MValue &b)
{
    MType ta = a.type(), tb = b.type();
    bool ai = isIntegerType(ta), bi = isIntegerType(tb);
    bool af = isFloatType(ta),  bf = isFloatType(tb);
    bool aS = (ta == MType::SINGLE), bS = (tb == MType::SINGLE);

    if (ai && bi) {
        if (ta != tb)
            throw std::runtime_error("Integers can only be combined with integers of the same class");
        return ta;
    }
    if (ai && (bf || tb == MType::LOGICAL)) return ta;
    if (bi && (af || ta == MType::LOGICAL)) return tb;
    if (aS || bS) return MType::SINGLE;
    return MType::EMPTY; // not an integer/single case
}

template <typename Op>
MValue dispatchIntegerBinaryOp(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    MType target = resolveIntegerPairType(a, b);
    if (target == MType::EMPTY) return MValue(); // signal: not handled

    switch (target) {
    case MType::INT8:   return elementwiseTyped<int8_t>(a, b, target, [&](int8_t x, int8_t y) { return op(x, y); }, alloc);
    case MType::INT16:  return elementwiseTyped<int16_t>(a, b, target, [&](int16_t x, int16_t y) { return op(x, y); }, alloc);
    case MType::INT32:  return elementwiseTyped<int32_t>(a, b, target, [&](int32_t x, int32_t y) { return op(x, y); }, alloc);
    case MType::INT64:  return elementwiseTyped<int64_t>(a, b, target, [&](int64_t x, int64_t y) { return op(x, y); }, alloc);
    case MType::UINT8:  return elementwiseTyped<uint8_t>(a, b, target, [&](uint8_t x, uint8_t y) { return op(x, y); }, alloc);
    case MType::UINT16: return elementwiseTyped<uint16_t>(a, b, target, [&](uint16_t x, uint16_t y) { return op(x, y); }, alloc);
    case MType::UINT32: return elementwiseTyped<uint32_t>(a, b, target, [&](uint32_t x, uint32_t y) { return op(x, y); }, alloc);
    case MType::UINT64: return elementwiseTyped<uint64_t>(a, b, target, [&](uint64_t x, uint64_t y) { return op(x, y); }, alloc);
    case MType::SINGLE: return elementwiseTyped<float>(a, b, target, [&](float x, float y) { return op(x, y); }, alloc);
    default: return MValue();
    }
}

// ============================================================
// Unary typed op
// ============================================================

template <typename T, typename Op>
MValue unaryTyped(const MValue &a, MType targetType, Op op, Allocator *alloc)
{
    auto r = MValue::matrix(a.dims().rows(), std::max(a.dims().cols(), size_t(1)), targetType, alloc);
    const T *src = static_cast<const T *>(a.rawData());
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t i = 0; i < a.numel(); ++i)
        dst[i] = op(src[i]);
    return r;
}

} // namespace mlab
