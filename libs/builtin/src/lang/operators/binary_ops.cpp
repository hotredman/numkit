// libs/builtin/src/lang/operators/binary_ops.cpp

#include <numkit/builtin/lang/operators/binary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "backends/binary_ops_loops.hpp"
#include "backends/compare.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory_resource>
#include <string>

namespace {

// Fast-path predicate: both inputs are non-scalar, dimensions match
// exactly (includes 3D same-shape — memory is contiguous so the flat
// SIMD loop works unchanged). Other shapes (broadcasting) still fall
// through to elementwiseDouble() in helpers.hpp.
inline bool sameShapeDoubleFastPath(const numkit::Value &a,
                                    const numkit::Value &b)
{
    return !a.isScalar() && !b.isScalar() && a.dims() == b.dims();
}

} // namespace

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — binary operators
// ════════════════════════════════════════════════════════════════════════

// ── Arithmetic ──────────────────────────────────────────────────────────

Value plus(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::plus<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            detail::plusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::plus<double>{}, p);
    }
    if (a.isChar() && b.isChar())
        return Value::fromString(a.toString() + b.toString(), p);
    if (a.isChar() && b.type() == ValueType::DOUBLE) {
        auto ca = createLike(a, ValueType::DOUBLE, p);
        const char *cd = a.charData();
        double *dd = ca.doubleDataMut();
        for (size_t i = 0; i < a.numel(); ++i)
            dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
        return elementwiseDouble(ca, b, std::plus<double>{}, p);
    }
    if (a.type() == ValueType::DOUBLE && b.isChar()) {
        auto cb = createLike(b, ValueType::DOUBLE, p);
        const char *cd = b.charData();
        double *dd = cb.doubleDataMut();
        for (size_t i = 0; i < b.numel(); ++i)
            dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
        return elementwiseDouble(a, cb, std::plus<double>{}, p);
    }
    if (a.isString() && b.isString())
        return Value::stringScalar(a.toString() + b.toString(), p);
    if (a.isString() && b.isChar())
        return Value::stringScalar(a.toString() + b.toString(), p);
    if (a.isChar() && b.isString())
        return Value::stringScalar(a.toString() + b.toString(), p);
    if (a.isString() && b.isNumeric())
        return Value::stringScalar(a.toString() + std::to_string(b.toScalar()), p);
    if (a.isNumeric() && b.isString())
        return Value::stringScalar(std::to_string(a.toScalar()) + b.toString(), p);
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateAdd(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for +", 0, 0, "plus", "", "m:plus:unsupportedTypes");
}

Value minus(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::minus<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            detail::minusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::minus<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateSub(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for -", 0, 0, "minus", "", "m:minus:unsupportedTypes");
}

Value times(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            detail::timesLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::multiplies<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for .*", 0, 0, "times", "", "m:times:unsupportedTypes");
}

Value mtimes(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;

    // Matrix-multiply is undefined for N-D arrays (N > 2) except the
    // scalar * NDArray degenerate form, which is just an elementwise
    // scale and is handled further down. Match MATLAB's error here —
    // the pre-split code silently treated pages as 1 and produced
    // garbage, which is worse than failing loudly.
    if ((a.dims().is3D() || b.dims().is3D()) && !a.isScalar() && !b.isScalar())
        throw Error("MTIMES is not supported for N-D arrays",
                     0, 0, "mtimes", "", "m:mtimes:notSupportedND");

    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        if (ca.isScalar() || cb.isScalar())
            return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
        size_t M = ca.dims().rows(), K = ca.dims().cols(), N = cb.dims().cols();
        if (K != cb.dims().rows())
            throw Error("Inner matrix dimensions must agree", 0, 0, "mtimes", "",
                         "m:innerdim");
        auto r = Value::complexMatrix(M, N, p);
        for (size_t i = 0; i < M; ++i)
            for (size_t j = 0; j < N; ++j) {
                Complex s(0, 0);
                for (size_t k = 0; k < K; ++k)
                    s += ca.complexElem(i, k) * cb.complexElem(k, j);
                r.complexDataMut()[j * M + i] = s;
            }
        return r;
    }

    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    if (a.isScalar() || b.isScalar())
        return elementwiseDouble(a, b, std::multiplies<double>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        size_t M = a.dims().rows(), K = a.dims().cols(), N = b.dims().cols();
        if (K != b.dims().rows())
            throw Error("Inner matrix dimensions must agree", 0, 0, "mtimes", "",
                         "m:innerdim");
        auto r = Value::matrix(M, N, ValueType::DOUBLE, p);
        detail::matmulDoubleLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(),
                                 M, N, K);
        return r;
    }
    throw Error("Unsupported types for *", 0, 0, "mtimes", "", "m:mtimes:unsupportedTypes");
}

Value rdivide(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::divides<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            detail::rdivideLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for ./", 0, 0, "rdivide", "",
                 "m:rdivide:unsupportedTypes");
}

Value mrdivide(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::divides<Complex>{}, p);
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    if (a.type() == ValueType::DOUBLE && b.isScalar())
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    if (a.isScalar() && b.isScalar())
        return Value::scalar(a.toScalar() / b.toScalar(), p);
    throw Error("Matrix right division not implemented", 0, 0, "mrdivide", "",
                 "m:mrdivide:notImplemented");
}

Value mldivide(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isScalar() && b.isScalar())
        return Value::scalar(b.toScalar() / a.toScalar(), p);
    throw Error("Matrix left division not implemented", 0, 0, "mldivide", "",
                 "m:mldivide:notImplemented");
}

Value power(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        return Value::complexScalar(std::pow(ca.toComplex(), cb.toComplex()), p);
    }
    if (a.isScalar() && b.isScalar())
        return Value::scalar(std::pow(a.toScalar(), b.toScalar()), p);
    throw Error("Matrix power not implemented", 0, 0, "power", "",
                 "m:power:notImplemented");
}

Value elementPower(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex()) {
        return elementwiseComplex(
            a, b, [](const Complex &x, const Complex &y) { return std::pow(x, y); }, p);
    }
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        return elementwiseDouble(a, b, [](double x, double y) { return std::pow(x, y); }, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) -> decltype(x) {
            double r = std::pow(static_cast<double>(x), static_cast<double>(y));
            if constexpr (std::is_integral_v<decltype(x)>) {
                r = std::round(r);
                if (r > static_cast<double>(std::numeric_limits<decltype(x)>::max()))
                    return std::numeric_limits<decltype(x)>::max();
                if (r < static_cast<double>(std::numeric_limits<decltype(x)>::min()))
                    return std::numeric_limits<decltype(x)>::min();
                return static_cast<decltype(x)>(r);
            } else {
                return static_cast<decltype(x)>(r);
            }
        }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for .^", 0, 0, "elementPower", "",
                 "m:elementPower:unsupportedTypes");
}

// ── Comparisons ──────────────────────────────────────────────────────────

namespace {

// cmpOp encodes the comparison as a numeric op id to avoid repeated string
// compares inside tight loops.
enum class Cmp { EQ, NE, LT, GT, LE, GE };

inline const char *cmpOpName(Cmp c)
{
    switch (c) {
    case Cmp::EQ: return "==";
    case Cmp::NE: return "~=";
    case Cmp::LT: return "<";
    case Cmp::GT: return ">";
    case Cmp::LE: return "<=";
    case Cmp::GE: return ">=";
    }
    return "";
}

inline bool applyCmp(Cmp c, double x, double y)
{
    switch (c) {
    case Cmp::EQ: return x == y;
    case Cmp::NE: return x != y;
    case Cmp::LT: return x <  y;
    case Cmp::GT: return x >  y;
    case Cmp::LE: return x <= y;
    case Cmp::GE: return x >= y;
    }
    return false;
}

Value compareImpl(Cmp c, const Value &a, const Value &b)
{
    // Char/char fast paths for == and ~=
    if (a.isChar() && b.isChar()) {
        if (c == Cmp::EQ)
            return Value::logicalScalar(a.toString() == b.toString(), nullptr);
        if (c == Cmp::NE)
            return Value::logicalScalar(a.toString() != b.toString(), nullptr);
    }

    // String comparisons
    if (a.isString() || b.isString()) {
        auto toStr = [](const Value &v) -> std::string {
            if (v.isString() || v.isChar())
                return v.toString();
            throw Error("Comparison between string and non-string is not supported",
                         0, 0, "compare", "", "m:compare:stringType");
        };
        std::string sa = toStr(a), sb = toStr(b);
        switch (c) {
        case Cmp::EQ: return Value::logicalScalar(sa == sb, nullptr);
        case Cmp::NE: return Value::logicalScalar(sa != sb, nullptr);
        case Cmp::LT: return Value::logicalScalar(sa <  sb, nullptr);
        case Cmp::GT: return Value::logicalScalar(sa >  sb, nullptr);
        case Cmp::LE: return Value::logicalScalar(sa <= sb, nullptr);
        case Cmp::GE: return Value::logicalScalar(sa >= sb, nullptr);
        }
    }

    // Complex: only == and ~= are defined
    if (a.isComplex() || b.isComplex()) {
        if (c != Cmp::EQ && c != Cmp::NE)
            throw Error(std::string("Operator '") + cmpOpName(c)
                             + "' is not supported for complex operands",
                         0, 0, "compare", "", "m:compare:complexOrder");
        const bool isEq = (c == Cmp::EQ);
        auto ceq = [](Complex x, Complex y) {
            return x.real() == y.real() && x.imag() == y.imag();
        };
        auto getC = [](const Value &v, size_t i) -> Complex {
            if (v.isComplex())
                return v.complexData()[i];
            return Complex(v.type() == ValueType::LOGICAL
                               ? static_cast<double>(v.logicalData()[i])
                               : v.doubleData()[i],
                           0.0);
        };
        if (a.isScalar() && b.isScalar()) {
            Complex ca = a.isComplex() ? a.toComplex() : Complex(a.toScalar(), 0.0);
            Complex cb = b.isComplex() ? b.toComplex() : Complex(b.toScalar(), 0.0);
            return Value::logicalScalar(isEq ? ceq(ca, cb) : !ceq(ca, cb), nullptr);
        }
        if (a.isScalar()) {
            Complex ca = a.isComplex() ? a.toComplex() : Complex(a.toScalar(), 0.0);
            auto r = createLike(b, ValueType::LOGICAL, nullptr);
            for (size_t i = 0; i < b.numel(); ++i)
                r.logicalDataMut()[i] =
                    (isEq ? ceq(ca, getC(b, i)) : !ceq(ca, getC(b, i))) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            Complex cb = b.isComplex() ? b.toComplex() : Complex(b.toScalar(), 0.0);
            auto r = createLike(a, ValueType::LOGICAL, nullptr);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] =
                    (isEq ? ceq(getC(a, i), cb) : !ceq(getC(a, i), cb)) ? 1 : 0;
            return r;
        }
        if (a.dims() != b.dims())
            throw Error("Matrix dimensions must agree for comparison",
                         0, 0, "compare", "", "m:dimagree");
        auto r = createLike(a, ValueType::LOGICAL, nullptr);
        for (size_t i = 0; i < a.numel(); ++i)
            r.logicalDataMut()[i] =
                (isEq ? ceq(getC(a, i), getC(b, i)) : !ceq(getC(a, i), getC(b, i)))
                    ? 1 : 0;
        return r;
    }

    // SIMD fast path — pure DOUBLE × DOUBLE (or DOUBLE scalar broadcast).
    // Returns unset Value if it can't handle the case (logical/integer/
    // complex operand, broadcast across mismatched non-scalar dims, both
    // operands scalar) — scalar dispatch below picks up the leftovers.
    {
        Value r;
        switch (c) {
        case Cmp::EQ: r = eqFast(a, b); break;
        case Cmp::NE: r = neFast(a, b); break;
        case Cmp::LT: r = ltFast(a, b); break;
        case Cmp::GT: r = gtFast(a, b); break;
        case Cmp::LE: r = leFast(a, b); break;
        case Cmp::GE: r = geFast(a, b); break;
        }
        if (!r.isUnset()) return r;
    }

    // Numeric — double/logical/integer/single with broadcasting
    auto getD = [](const Value &v, size_t r, size_t col) -> double {
        size_t idx = col * v.dims().rows() + r;
        if (v.isLogical()) return static_cast<double>(v.logicalData()[idx]);
        if (isIntegerType(v.type()) || v.type() == ValueType::SINGLE) return v.toScalar();
        return v.doubleData()[idx];
    };
    auto getDScalar = [](const Value &v) -> double {
        if (v.isLogical()) return v.toBool() ? 1.0 : 0.0;
        return v.toScalar();
    };

    if (a.isScalar() && b.isScalar())
        return Value::logicalScalar(applyCmp(c, getDScalar(a), getDScalar(b)), nullptr);

    auto elemD = [](const Value &v, size_t i) -> double {
        if (v.isLogical()) return v.logicalData()[i];
        if (v.type() == ValueType::DOUBLE) return v.doubleData()[i];
        return v.elemAsDouble(i);
    };

    // ND fallback (rank ≥ 4)
    if (a.dims().ndim() >= 4 || b.dims().ndim() >= 4) {
        if (a.isScalar()) {
            auto r = createLike(b, ValueType::LOGICAL, nullptr);
            double s = getDScalar(a);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < b.numel(); ++i)
                dst[i] = applyCmp(c, s, elemD(b, i)) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            auto r = createLike(a, ValueType::LOGICAL, nullptr);
            double s = getDScalar(b);
            uint8_t *dst = r.logicalDataMut();
            for (size_t i = 0; i < a.numel(); ++i)
                dst[i] = applyCmp(c, elemD(a, i), s) ? 1 : 0;
            return r;
        }
        Dims outD;
        if (!broadcastDimsND(a.dims(), b.dims(), outD))
            throw Error("ND dimensions must broadcast for comparison: each axis must match or be 1",
                         0, 0, "compare", "", "m:dimagree");
        auto r = createForDims(outD, ValueType::LOGICAL, nullptr);
        uint8_t *dst = r.logicalDataMut();
        forEachNDPair(a.dims(), b.dims(), outD,
            [&](size_t outIdx, size_t aOff, size_t bOff) {
                dst[outIdx] = applyCmp(c, elemD(a, aOff), elemD(b, bOff)) ? 1 : 0;
            });
        return r;
    }

    if (a.dims().is3D() || b.dims().is3D()) {
        if (a.isScalar()) {
            auto r = createLike(b, ValueType::LOGICAL, nullptr);
            double s = getDScalar(a);
            for (size_t i = 0; i < b.numel(); ++i)
                r.logicalDataMut()[i] = applyCmp(c, s, elemD(b, i)) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            auto r = createLike(a, ValueType::LOGICAL, nullptr);
            double s = getDScalar(b);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] = applyCmp(c, elemD(a, i), s) ? 1 : 0;
            return r;
        }
        const size_t aR = a.dims().rows(), aC = a.dims().cols();
        const size_t aP = a.dims().is3D() ? a.dims().pages() : 1;
        const size_t bR = b.dims().rows(), bC = b.dims().cols();
        const size_t bP = b.dims().is3D() ? b.dims().pages() : 1;
        size_t outR, outC, outP;
        if (!broadcastDims3D(aR, aC, aP, bR, bC, bP, outR, outC, outP))
            throw Error("3D dimensions must broadcast for comparison: each axis must match or be 1",
                         0, 0, "compare", "", "m:dimagree");

        if (aR == bR && aC == bC && aP == bP) {
            auto r = createLike(a, ValueType::LOGICAL, nullptr);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] = applyCmp(c, elemD(a, i), elemD(b, i)) ? 1 : 0;
            return r;
        }
        auto r = (outP > 1) ? Value::matrix3d(outR, outC, outP, ValueType::LOGICAL, nullptr)
                            : Value::matrix(outR, outC, ValueType::LOGICAL, nullptr);
        uint8_t *dst = r.logicalDataMut();
        for (size_t pp = 0; pp < outP; ++pp)
            for (size_t cc = 0; cc < outC; ++cc)
                for (size_t rr = 0; rr < outR; ++rr) {
                    const size_t aOff = broadcastOffset3D(rr, cc, pp, aR, aC, aP);
                    const size_t bOff = broadcastOffset3D(rr, cc, pp, bR, bC, bP);
                    dst[pp * outR * outC + cc * outR + rr] =
                        applyCmp(c, elemD(a, aOff), elemD(b, bOff)) ? 1 : 0;
                }
        return r;
    }

    size_t ar = a.dims().rows(), ac = a.dims().cols();
    size_t br = b.dims().rows(), bc = b.dims().cols();
    size_t outR, outC;
    if (!broadcastDims(ar, ac, br, bc, outR, outC))
        throw Error("Matrix dimensions must agree for comparison",
                     0, 0, "compare", "", "m:dimagree");

    auto r = Value::matrix(outR, outC, ValueType::LOGICAL, nullptr);
    uint8_t *dst = r.logicalDataMut();
    for (size_t col = 0; col < outC; ++col) {
        size_t ca = (ac == 1) ? 0 : col;
        size_t cb = (bc == 1) ? 0 : col;
        for (size_t row = 0; row < outR; ++row) {
            size_t ra = (ar == 1) ? 0 : row;
            size_t rb = (br == 1) ? 0 : row;
            dst[col * outR + row] =
                applyCmp(c, getD(a, ra, ca), getD(b, rb, cb)) ? 1 : 0;
        }
    }
    return r;
}

} // namespace

Value eq(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::EQ, a, b); }
Value ne(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::NE, a, b); }
Value lt(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::LT, a, b); }
Value gt(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::GT, a, b); }
Value le(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::LE, a, b); }
Value ge(std::pmr::memory_resource *, const Value &a, const Value &b) { return compareImpl(Cmp::GE, a, b); }

// ── Logical ──────────────────────────────────────────────────────────────

namespace {

ScratchVec<uint8_t> toBoolArray(std::pmr::memory_resource *mr, const Value &v)
{
    ScratchVec<uint8_t> r(v.numel(), mr);
    if (v.isLogical()) {
        const uint8_t *d = v.logicalData();
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = d[i] ? 1 : 0;
    } else if (v.type() == ValueType::DOUBLE) {
        const double *d = v.doubleData();
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = (d[i] != 0.0) ? 1 : 0;
    } else {
        r[0] = v.toBool() ? 1 : 0;
    }
    return r;
}

template <typename Op>
Value logicalBinary(const char *opName, Op op,
                     std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.isScalar() && b.isScalar())
        return Value::logicalScalar(op(a.toBool(), b.toBool()), mr);
    ScratchArena scratch_arena(mr);
    if (a.isScalar()) {
        bool av = a.toBool();
        auto bb = toBoolArray(&scratch_arena, b);
        auto r = createLike(b, ValueType::LOGICAL, mr);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < bb.size(); ++i)
            dst[i] = op(av, static_cast<bool>(bb[i])) ? 1 : 0;
        return r;
    }
    if (b.isScalar()) {
        bool bv = b.toBool();
        auto aa = toBoolArray(&scratch_arena, a);
        auto r = createLike(a, ValueType::LOGICAL, mr);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = op(static_cast<bool>(aa[i]), bv) ? 1 : 0;
        return r;
    }
    if (a.numel() != b.numel())
        throw Error(std::string("Matrix dimensions must agree for ") + opName,
                     0, 0, opName, "", "m:dimagree");
    auto aa = toBoolArray(&scratch_arena, a);
    auto bb = toBoolArray(&scratch_arena, b);
    auto r = createLike(a, ValueType::LOGICAL, mr);
    uint8_t *dst = r.logicalDataMut();
    for (size_t i = 0; i < aa.size(); ++i)
        dst[i] = op(static_cast<bool>(aa[i]), static_cast<bool>(bb[i])) ? 1 : 0;
    return r;
}

} // namespace

Value logicalAnd(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return logicalBinary("&", [](bool x, bool y) { return x && y; }, mr, a, b);
}

Value logicalOr(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return logicalBinary("|", [](bool x, bool y) { return x || y; }, mr, a, b);
}

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward BinaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const Value &a, const Value &b) { return builtin::plus(engine.resource(), a, b); });
    engine.registerBinaryOp("-",  [&engine](const Value &a, const Value &b) { return builtin::minus(engine.resource(), a, b); });
    engine.registerBinaryOp(".*", [&engine](const Value &a, const Value &b) { return builtin::times(engine.resource(), a, b); });
    engine.registerBinaryOp("*",  [&engine](const Value &a, const Value &b) { return builtin::mtimes(engine.resource(), a, b); });
    engine.registerBinaryOp("./", [&engine](const Value &a, const Value &b) { return builtin::rdivide(engine.resource(), a, b); });
    engine.registerBinaryOp("/",  [&engine](const Value &a, const Value &b) { return builtin::mrdivide(engine.resource(), a, b); });
    engine.registerBinaryOp("\\", [&engine](const Value &a, const Value &b) { return builtin::mldivide(engine.resource(), a, b); });
    engine.registerBinaryOp("^",  [&engine](const Value &a, const Value &b) { return builtin::power(engine.resource(), a, b); });
    engine.registerBinaryOp(".^", [&engine](const Value &a, const Value &b) { return builtin::elementPower(engine.resource(), a, b); });

    engine.registerBinaryOp("==", [&engine](const Value &a, const Value &b) { return builtin::eq(engine.resource(), a, b); });
    engine.registerBinaryOp("~=", [&engine](const Value &a, const Value &b) { return builtin::ne(engine.resource(), a, b); });
    engine.registerBinaryOp("<",  [&engine](const Value &a, const Value &b) { return builtin::lt(engine.resource(), a, b); });
    engine.registerBinaryOp(">",  [&engine](const Value &a, const Value &b) { return builtin::gt(engine.resource(), a, b); });
    engine.registerBinaryOp("<=", [&engine](const Value &a, const Value &b) { return builtin::le(engine.resource(), a, b); });
    engine.registerBinaryOp(">=", [&engine](const Value &a, const Value &b) { return builtin::ge(engine.resource(), a, b); });

    engine.registerBinaryOp("&",  [&engine](const Value &a, const Value &b) { return builtin::logicalAnd(engine.resource(), a, b); });
    engine.registerBinaryOp("|",  [&engine](const Value &a, const Value &b) { return builtin::logicalOr(engine.resource(), a, b); });
}

} // namespace numkit
