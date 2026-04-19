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
// Implicit expansion (broadcasting) check
// Returns true if dims are compatible, sets outR/outC to result size.
// Rule: each dim must match or one of them must be 1.
// ============================================================
inline bool broadcastDims(size_t ar, size_t ac, size_t br, size_t bc,
                          size_t &outR, size_t &outC)
{
    if (ar != br && ar != 1 && br != 1) return false;
    if (ac != bc && ac != 1 && bc != 1) return false;
    outR = std::max(ar, br);
    outC = std::max(ac, bc);
    return true;
}

// ============================================================
// Elementwise binary op on double arrays (with implicit expansion)
// ============================================================
template<typename Op>
MValue elementwiseDouble(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    if (a.isEmpty() || b.isEmpty())
        return emptyResultForBinary(a, b, MType::DOUBLE, alloc);
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(op(a.toScalar(), b.toScalar()), alloc);

    // 3D paths — same-shape elementwise and scalar broadcasting only.
    // General 3D broadcasting is not yet supported.
    if (a.dims().is3D() || b.dims().is3D()) {
        if (a.isScalar()) {
            auto r = createLike(b, MType::DOUBLE, alloc);
            double s = a.toScalar();
            for (size_t i = 0; i < b.numel(); ++i)
                r.doubleDataMut()[i] = op(s, b.doubleData()[i]);
            return r;
        }
        if (b.isScalar()) {
            auto r = createLike(a, MType::DOUBLE, alloc);
            double s = b.toScalar();
            for (size_t i = 0; i < a.numel(); ++i)
                r.doubleDataMut()[i] = op(a.doubleData()[i], s);
            return r;
        }
        if (a.dims() != b.dims())
            throw std::runtime_error(
                "3D broadcasting not supported — dimensions must match");
        auto r = createLike(a, MType::DOUBLE, alloc);
        for (size_t i = 0; i < a.numel(); ++i)
            r.doubleDataMut()[i] = op(a.doubleData()[i], b.doubleData()[i]);
        return r;
    }

    size_t ar = a.dims().rows(), ac = a.dims().cols();
    size_t br = b.dims().rows(), bc = b.dims().cols();
    size_t outR, outC;
    if (!broadcastDims(ar, ac, br, bc, outR, outC))
        throw std::runtime_error("Matrix dimensions must agree");

    // Fast path: same dimensions, no broadcasting needed
    if (ar == br && ac == bc) {
        auto r = MValue::matrix(outR, outC, MType::DOUBLE, alloc);
        for (size_t i = 0; i < a.numel(); ++i)
            r.doubleDataMut()[i] = op(a.doubleData()[i], b.doubleData()[i]);
        return r;
    }

    // General broadcasting: column-major indexing
    auto r = MValue::matrix(outR, outC, MType::DOUBLE, alloc);
    double *dst = r.doubleDataMut();
    const double *da = a.doubleData(), *db = b.doubleData();
    for (size_t c = 0; c < outC; ++c) {
        size_t ca = (ac == 1) ? 0 : c;
        size_t cb = (bc == 1) ? 0 : c;
        for (size_t row = 0; row < outR; ++row) {
            size_t ra = (ar == 1) ? 0 : row;
            size_t rb = (br == 1) ? 0 : row;
            dst[c * outR + row] = op(da[ca * ar + ra], db[cb * br + rb]);
        }
    }
    return r;
}

// ============================================================
// Elementwise binary op on complex arrays
// ============================================================
template<typename Op>
MValue elementwiseComplex(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    if (a.isEmpty() || b.isEmpty())
        return emptyResultForBinary(a, b, MType::COMPLEX, alloc);
    auto [ca, cb] = promoteToComplex(a, b, alloc);
    if (ca.isScalar() && cb.isScalar())
        return MValue::complexScalar(op(ca.toComplex(), cb.toComplex()), alloc);

    if (ca.dims().is3D() || cb.dims().is3D()) {
        if (ca.isScalar()) {
            auto r = createLike(cb, MType::COMPLEX, alloc);
            Complex s = ca.toComplex();
            for (size_t i = 0; i < cb.numel(); ++i)
                r.complexDataMut()[i] = op(s, cb.complexData()[i]);
            return r;
        }
        if (cb.isScalar()) {
            auto r = createLike(ca, MType::COMPLEX, alloc);
            Complex s = cb.toComplex();
            for (size_t i = 0; i < ca.numel(); ++i)
                r.complexDataMut()[i] = op(ca.complexData()[i], s);
            return r;
        }
        if (ca.dims() != cb.dims())
            throw std::runtime_error(
                "3D broadcasting not supported — dimensions must match");
        auto r = createLike(ca, MType::COMPLEX, alloc);
        for (size_t i = 0; i < ca.numel(); ++i)
            r.complexDataMut()[i] = op(ca.complexData()[i], cb.complexData()[i]);
        return r;
    }

    size_t ar = ca.dims().rows(), ac = ca.dims().cols();
    size_t br = cb.dims().rows(), bc = cb.dims().cols();
    size_t outR, outC;
    if (!broadcastDims(ar, ac, br, bc, outR, outC))
        throw std::runtime_error("Matrix dimensions must agree");

    if (ar == br && ac == bc) {
        auto r = MValue::complexMatrix(outR, outC, alloc);
        for (size_t i = 0; i < ca.numel(); ++i)
            r.complexDataMut()[i] = op(ca.complexData()[i], cb.complexData()[i]);
        return r;
    }

    auto r = MValue::complexMatrix(outR, outC, alloc);
    Complex *dst = r.complexDataMut();
    const Complex *da = ca.complexData(), *db = cb.complexData();
    for (size_t c = 0; c < outC; ++c) {
        size_t cca = (ac == 1) ? 0 : c;
        size_t ccb = (bc == 1) ? 0 : c;
        for (size_t row = 0; row < outR; ++row) {
            size_t ra = (ar == 1) ? 0 : row;
            size_t rb = (br == 1) ? 0 : row;
            dst[c * outR + row] = op(da[cca * ar + ra], db[ccb * br + rb]);
        }
    }
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
    auto r = createLike(a, MType::DOUBLE, alloc);
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
    auto r = createLike(a, MType::COMPLEX, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.complexDataMut()[i] = op(a.complexData()[i]);
    return r;
}

// ============================================================
// Parse dimension arguments for array creation functions
// Supports: f(n), f(m,n), f(m,n,p), f([m n]), f([m n p]), f(size(x))
// Returns {rows, cols, pages}. pages=0 means 2D.
// ============================================================

struct DimsArg { size_t rows = 1, cols = 1, pages = 0; };

inline DimsArg parseDimsArgs(Span<const MValue> args)
{
    if (args.empty())
        return {1, 1, 0};

    // Single vector argument: [m n] or [m n p]
    if (args.size() == 1 && !args[0].isScalar() && args[0].numel() >= 2) {
        const double *d = args[0].doubleData();
        size_t n = args[0].numel();
        size_t r = static_cast<size_t>(d[0]);
        size_t c = static_cast<size_t>(d[1]);
        size_t p = (n >= 3) ? static_cast<size_t>(d[2]) : 0;
        return {r, c, p};
    }

    // Single scalar: f(n) → n×n
    if (args.size() == 1) {
        size_t n = static_cast<size_t>(args[0].toScalar());
        return {n, n, 0};
    }

    // Two scalars: f(m, n)
    size_t r = static_cast<size_t>(args[0].toScalar());
    size_t c = static_cast<size_t>(args[1].toScalar());

    // Three scalars: f(m, n, p)
    if (args.size() >= 3) {
        size_t p = static_cast<size_t>(args[2].toScalar());
        return {r, c, p};
    }

    return {r, c, 0};
}

// Create a zero matrix/3D array with given dimensions
inline MValue createMatrix(DimsArg d, MType type, Allocator *alloc)
{
    if (d.pages > 0)
        return MValue::matrix3d(d.rows, d.cols, d.pages, type, alloc);
    return MValue::matrix(d.rows, d.cols, type, alloc);
}

// Allocate an MValue with the same shape (2D or 3D) as `src`, optionally
// of a different type. Required for any "elementwise" output: callers
// that write numel() elements into a 2D-only allocation silently
// corrupt the heap when src is 3D.
inline MValue createLike(const MValue &src, MType type, Allocator *alloc)
{
    return createMatrix({src.dims().rows(), src.dims().cols(),
                         src.dims().is3D() ? src.dims().pages() : 0},
                        type, alloc);
}

// Shape-preserving empty result for a binary op where at least one
// operand is empty. The non-scalar operand contributes the output
// shape; if both are non-scalar the dims must match, otherwise throw.
// `outType` is chosen by the caller based on its promotion rules.
inline MValue emptyResultForBinary(const MValue &a, const MValue &b,
                                   MType outType, Allocator *alloc)
{
    const bool aShaper = !a.isScalar();
    const bool bShaper = !b.isScalar();
    if (aShaper && bShaper && a.dims() != b.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    const MValue &shape = aShaper ? a : (bShaper ? b : a);
    return createLike(shape, outType, alloc);
}

// Pick the arithmetic output type for a pair of operands using
// MATLAB-style promotion: complex > single > integer > double.
// Char and logical operands promote to double.
inline MType arithOutType(const MValue &a, const MValue &b)
{
    if (a.isComplex() || b.isComplex()) return MType::COMPLEX;
    if (a.type() == MType::SINGLE || b.type() == MType::SINGLE) return MType::SINGLE;
    if (isIntegerType(a.type())) return a.type();
    if (isIntegerType(b.type())) return b.type();
    return MType::DOUBLE;
}

// Convenience wrapper: shape-preserving empty arithmetic result with
// type chosen by arithOutType().
inline MValue emptyArithResult(const MValue &a, const MValue &b, Allocator *alloc)
{
    return emptyResultForBinary(a, b, arithOutType(a, b), alloc);
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
    // Read element from source, converting to target type
    auto readAt = [](const MValue &v, MType tgt, size_t r, size_t c) -> T {
        size_t idx = c * v.dims().rows() + r; // column-major
        if (v.type() == tgt) return static_cast<const T *>(v.rawData())[idx];
        double d = v.doubleData()[idx];
        if constexpr (std::is_integral_v<T>)
            return static_cast<T>(std::clamp(std::round(d),
                static_cast<double>(std::numeric_limits<T>::min()),
                static_cast<double>(std::numeric_limits<T>::max())));
        else
            return static_cast<T>(d);
    };

    size_t ar = a.dims().rows(), ac = a.dims().cols();
    size_t br = b.dims().rows(), bc = b.dims().cols();
    size_t outR, outC;
    if (!broadcastDims(ar, ac, br, bc, outR, outC))
        throw std::runtime_error("Matrix dimensions must agree");

    auto r = MValue::matrix(outR, outC, targetType, alloc);
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t c = 0; c < outC; ++c) {
        size_t ca = (ac == 1) ? 0 : c;
        size_t cb = (bc == 1) ? 0 : c;
        for (size_t row = 0; row < outR; ++row) {
            size_t ra = (ar == 1) ? 0 : row;
            size_t rb = (br == 1) ? 0 : row;
            dst[c * outR + row] = op(readAt(a, targetType, ra, ca),
                                     readAt(b, targetType, rb, cb));
        }
    }
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
    auto r = createLike(a, targetType, alloc);
    const T *src = static_cast<const T *>(a.rawData());
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t i = 0; i < a.numel(); ++i)
        dst[i] = op(src[i]);
    return r;
}

} // namespace mlab
