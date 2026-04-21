// libs/builtin/src/MStdBinaryOps.cpp

#include <numkit/m/builtin/MStdBinaryOps.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "backends/BinaryOpsLoops.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

// Fast-path predicate: both inputs are non-scalar, dimensions match
// exactly (includes 3D same-shape — memory is contiguous so the flat
// SIMD loop works unchanged). Other shapes (broadcasting) still fall
// through to elementwiseDouble() in MStdHelpers.hpp.
inline bool sameShapeDoubleFastPath(const numkit::m::MValue &a,
                                    const numkit::m::MValue &b)
{
    return !a.isScalar() && !b.isScalar() && a.dims() == b.dims();
}

} // namespace

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — binary operators
// ════════════════════════════════════════════════════════════════════════

// ── Arithmetic ──────────────────────────────────────────────────────────

MValue plus(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::plus<Complex>{}, p);
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, MType::DOUBLE, p);
            detail::plusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::plus<double>{}, p);
    }
    if (a.isChar() && b.isChar())
        return MValue::fromString(a.toString() + b.toString(), p);
    if (a.isChar() && b.type() == MType::DOUBLE) {
        auto ca = createLike(a, MType::DOUBLE, p);
        const char *cd = a.charData();
        double *dd = ca.doubleDataMut();
        for (size_t i = 0; i < a.numel(); ++i)
            dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
        return elementwiseDouble(ca, b, std::plus<double>{}, p);
    }
    if (a.type() == MType::DOUBLE && b.isChar()) {
        auto cb = createLike(b, MType::DOUBLE, p);
        const char *cd = b.charData();
        double *dd = cb.doubleDataMut();
        for (size_t i = 0; i < b.numel(); ++i)
            dd[i] = static_cast<double>(static_cast<unsigned char>(cd[i]));
        return elementwiseDouble(a, cb, std::plus<double>{}, p);
    }
    if (a.isString() && b.isString())
        return MValue::stringScalar(a.toString() + b.toString(), p);
    if (a.isString() && b.isChar())
        return MValue::stringScalar(a.toString() + b.toString(), p);
    if (a.isChar() && b.isString())
        return MValue::stringScalar(a.toString() + b.toString(), p);
    if (a.isString() && b.isNumeric())
        return MValue::stringScalar(a.toString() + std::to_string(b.toScalar()), p);
    if (a.isNumeric() && b.isString())
        return MValue::stringScalar(std::to_string(a.toScalar()) + b.toString(), p);
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateAdd(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw MError("Unsupported types for +", 0, 0, "plus", "", "m:plus:unsupportedTypes");
}

MValue minus(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::minus<Complex>{}, p);
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, MType::DOUBLE, p);
            detail::minusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::minus<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateSub(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw MError("Unsupported types for -", 0, 0, "minus", "", "m:minus:unsupportedTypes");
}

MValue times(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, MType::DOUBLE, p);
            detail::timesLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::multiplies<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw MError("Unsupported types for .*", 0, 0, "times", "", "m:times:unsupportedTypes");
}

MValue mtimes(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;

    // Matrix-multiply is undefined for N-D arrays (N > 2) except the
    // scalar * NDArray degenerate form, which is just an elementwise
    // scale and is handled further down. Match MATLAB's error here —
    // the pre-split code silently treated pages as 1 and produced
    // garbage, which is worse than failing loudly.
    if ((a.dims().is3D() || b.dims().is3D()) && !a.isScalar() && !b.isScalar())
        throw MError("MTIMES is not supported for N-D arrays",
                     0, 0, "mtimes", "", "m:mtimes:notSupportedND");

    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        if (ca.isScalar() || cb.isScalar())
            return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
        size_t M = ca.dims().rows(), K = ca.dims().cols(), N = cb.dims().cols();
        if (K != cb.dims().rows())
            throw MError("Inner matrix dimensions must agree", 0, 0, "mtimes", "",
                         "m:innerdim");
        auto r = MValue::complexMatrix(M, N, p);
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
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
        size_t M = a.dims().rows(), K = a.dims().cols(), N = b.dims().cols();
        if (K != b.dims().rows())
            throw MError("Inner matrix dimensions must agree", 0, 0, "mtimes", "",
                         "m:innerdim");
        auto r = MValue::matrix(M, N, MType::DOUBLE, p);
        detail::matmulDoubleLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(),
                                 M, N, K);
        return r;
    }
    throw MError("Unsupported types for *", 0, 0, "mtimes", "", "m:mtimes:unsupportedTypes");
}

MValue rdivide(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::divides<Complex>{}, p);
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, MType::DOUBLE, p);
            detail::rdivideLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw MError("Unsupported types for ./", 0, 0, "rdivide", "",
                 "m:rdivide:unsupportedTypes");
}

MValue mrdivide(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::divides<Complex>{}, p);
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    if (a.type() == MType::DOUBLE && b.isScalar())
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(a.toScalar() / b.toScalar(), p);
    throw MError("Matrix right division not implemented", 0, 0, "mrdivide", "",
                 "m:mrdivide:notImplemented");
}

MValue mldivide(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(b.toScalar() / a.toScalar(), p);
    throw MError("Matrix left division not implemented", 0, 0, "mldivide", "",
                 "m:mldivide:notImplemented");
}

MValue power(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        return MValue::complexScalar(std::pow(ca.toComplex(), cb.toComplex()), p);
    }
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(std::pow(a.toScalar(), b.toScalar()), p);
    throw MError("Matrix power not implemented", 0, 0, "power", "",
                 "m:power:notImplemented");
}

MValue elementPower(Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.isComplex() || b.isComplex()) {
        return elementwiseComplex(
            a, b, [](const Complex &x, const Complex &y) { return std::pow(x, y); }, p);
    }
    if (a.type() == MType::DOUBLE && b.type() == MType::DOUBLE) {
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
    throw MError("Unsupported types for .^", 0, 0, "elementPower", "",
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

MValue compareImpl(Cmp c, const MValue &a, const MValue &b)
{
    // Char/char fast paths for == and ~=
    if (a.isChar() && b.isChar()) {
        if (c == Cmp::EQ)
            return MValue::logicalScalar(a.toString() == b.toString(), nullptr);
        if (c == Cmp::NE)
            return MValue::logicalScalar(a.toString() != b.toString(), nullptr);
    }

    // String comparisons
    if (a.isString() || b.isString()) {
        auto toStr = [](const MValue &v) -> std::string {
            if (v.isString() || v.isChar())
                return v.toString();
            throw MError("Comparison between string and non-string is not supported",
                         0, 0, "compare", "", "m:compare:stringType");
        };
        std::string sa = toStr(a), sb = toStr(b);
        switch (c) {
        case Cmp::EQ: return MValue::logicalScalar(sa == sb, nullptr);
        case Cmp::NE: return MValue::logicalScalar(sa != sb, nullptr);
        case Cmp::LT: return MValue::logicalScalar(sa <  sb, nullptr);
        case Cmp::GT: return MValue::logicalScalar(sa >  sb, nullptr);
        case Cmp::LE: return MValue::logicalScalar(sa <= sb, nullptr);
        case Cmp::GE: return MValue::logicalScalar(sa >= sb, nullptr);
        }
    }

    // Complex: only == and ~= are defined
    if (a.isComplex() || b.isComplex()) {
        if (c != Cmp::EQ && c != Cmp::NE)
            throw MError(std::string("Operator '") + cmpOpName(c)
                             + "' is not supported for complex operands",
                         0, 0, "compare", "", "m:compare:complexOrder");
        const bool isEq = (c == Cmp::EQ);
        auto ceq = [](Complex x, Complex y) {
            return x.real() == y.real() && x.imag() == y.imag();
        };
        auto getC = [](const MValue &v, size_t i) -> Complex {
            if (v.isComplex())
                return v.complexData()[i];
            return Complex(v.type() == MType::LOGICAL
                               ? static_cast<double>(v.logicalData()[i])
                               : v.doubleData()[i],
                           0.0);
        };
        if (a.isScalar() && b.isScalar()) {
            Complex ca = a.isComplex() ? a.toComplex() : Complex(a.toScalar(), 0.0);
            Complex cb = b.isComplex() ? b.toComplex() : Complex(b.toScalar(), 0.0);
            return MValue::logicalScalar(isEq ? ceq(ca, cb) : !ceq(ca, cb), nullptr);
        }
        if (a.isScalar()) {
            Complex ca = a.isComplex() ? a.toComplex() : Complex(a.toScalar(), 0.0);
            auto r = createLike(b, MType::LOGICAL, nullptr);
            for (size_t i = 0; i < b.numel(); ++i)
                r.logicalDataMut()[i] =
                    (isEq ? ceq(ca, getC(b, i)) : !ceq(ca, getC(b, i))) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            Complex cb = b.isComplex() ? b.toComplex() : Complex(b.toScalar(), 0.0);
            auto r = createLike(a, MType::LOGICAL, nullptr);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] =
                    (isEq ? ceq(getC(a, i), cb) : !ceq(getC(a, i), cb)) ? 1 : 0;
            return r;
        }
        if (a.dims() != b.dims())
            throw MError("Matrix dimensions must agree for comparison",
                         0, 0, "compare", "", "m:dimagree");
        auto r = createLike(a, MType::LOGICAL, nullptr);
        for (size_t i = 0; i < a.numel(); ++i)
            r.logicalDataMut()[i] =
                (isEq ? ceq(getC(a, i), getC(b, i)) : !ceq(getC(a, i), getC(b, i)))
                    ? 1 : 0;
        return r;
    }

    // Numeric — double/logical/integer/single with broadcasting
    auto getD = [](const MValue &v, size_t r, size_t col) -> double {
        size_t idx = col * v.dims().rows() + r;
        if (v.isLogical()) return static_cast<double>(v.logicalData()[idx]);
        if (isIntegerType(v.type()) || v.type() == MType::SINGLE) return v.toScalar();
        return v.doubleData()[idx];
    };
    auto getDScalar = [](const MValue &v) -> double {
        if (v.isLogical()) return v.toBool() ? 1.0 : 0.0;
        return v.toScalar();
    };

    if (a.isScalar() && b.isScalar())
        return MValue::logicalScalar(applyCmp(c, getDScalar(a), getDScalar(b)), nullptr);

    if (a.dims().is3D() || b.dims().is3D()) {
        auto elemD = [](const MValue &v, size_t i) -> double {
            if (v.isLogical()) return v.logicalData()[i];
            if (v.type() == MType::DOUBLE) return v.doubleData()[i];
            return v.elemAsDouble(i);
        };
        if (a.isScalar()) {
            auto r = createLike(b, MType::LOGICAL, nullptr);
            double s = getDScalar(a);
            for (size_t i = 0; i < b.numel(); ++i)
                r.logicalDataMut()[i] = applyCmp(c, s, elemD(b, i)) ? 1 : 0;
            return r;
        }
        if (b.isScalar()) {
            auto r = createLike(a, MType::LOGICAL, nullptr);
            double s = getDScalar(b);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] = applyCmp(c, elemD(a, i), s) ? 1 : 0;
            return r;
        }
        if (a.dims() != b.dims())
            throw MError("3D broadcasting not supported — dimensions must match",
                         0, 0, "compare", "", "m:dimagree");
        auto r = createLike(a, MType::LOGICAL, nullptr);
        for (size_t i = 0; i < a.numel(); ++i)
            r.logicalDataMut()[i] = applyCmp(c, elemD(a, i), elemD(b, i)) ? 1 : 0;
        return r;
    }

    size_t ar = a.dims().rows(), ac = a.dims().cols();
    size_t br = b.dims().rows(), bc = b.dims().cols();
    size_t outR, outC;
    if (!broadcastDims(ar, ac, br, bc, outR, outC))
        throw MError("Matrix dimensions must agree for comparison",
                     0, 0, "compare", "", "m:dimagree");

    auto r = MValue::matrix(outR, outC, MType::LOGICAL, nullptr);
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

MValue eq(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::EQ, a, b); }
MValue ne(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::NE, a, b); }
MValue lt(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::LT, a, b); }
MValue gt(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::GT, a, b); }
MValue le(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::LE, a, b); }
MValue ge(Allocator &, const MValue &a, const MValue &b) { return compareImpl(Cmp::GE, a, b); }

// ── Logical ──────────────────────────────────────────────────────────────

namespace {

std::vector<uint8_t> toBoolArray(const MValue &v)
{
    std::vector<uint8_t> r(v.numel());
    if (v.isLogical()) {
        const uint8_t *d = v.logicalData();
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = d[i] ? 1 : 0;
    } else if (v.type() == MType::DOUBLE) {
        const double *d = v.doubleData();
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = (d[i] != 0.0) ? 1 : 0;
    } else {
        r[0] = v.toBool() ? 1 : 0;
    }
    return r;
}

template <typename Op>
MValue logicalBinary(const char *opName, Op op,
                     Allocator &alloc, const MValue &a, const MValue &b)
{
    Allocator *p = &alloc;
    if (a.isScalar() && b.isScalar())
        return MValue::logicalScalar(op(a.toBool(), b.toBool()), p);
    if (a.isScalar()) {
        bool av = a.toBool();
        auto bb = toBoolArray(b);
        auto r = createLike(b, MType::LOGICAL, p);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < bb.size(); ++i)
            dst[i] = op(av, static_cast<bool>(bb[i])) ? 1 : 0;
        return r;
    }
    if (b.isScalar()) {
        bool bv = b.toBool();
        auto aa = toBoolArray(a);
        auto r = createLike(a, MType::LOGICAL, p);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = op(static_cast<bool>(aa[i]), bv) ? 1 : 0;
        return r;
    }
    if (a.numel() != b.numel())
        throw MError(std::string("Matrix dimensions must agree for ") + opName,
                     0, 0, opName, "", "m:dimagree");
    auto aa = toBoolArray(a);
    auto bb = toBoolArray(b);
    auto r = createLike(a, MType::LOGICAL, p);
    uint8_t *dst = r.logicalDataMut();
    for (size_t i = 0; i < aa.size(); ++i)
        dst[i] = op(static_cast<bool>(aa[i]), static_cast<bool>(bb[i])) ? 1 : 0;
    return r;
}

} // namespace

MValue logicalAnd(Allocator &alloc, const MValue &a, const MValue &b)
{
    return logicalBinary("&", [](bool x, bool y) { return x && y; }, alloc, a, b);
}

MValue logicalOr(Allocator &alloc, const MValue &a, const MValue &b)
{
    return logicalBinary("|", [](bool x, bool y) { return x || y; }, alloc, a, b);
}

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward BinaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const MValue &a, const MValue &b) { return builtin::plus(engine.allocator(), a, b); });
    engine.registerBinaryOp("-",  [&engine](const MValue &a, const MValue &b) { return builtin::minus(engine.allocator(), a, b); });
    engine.registerBinaryOp(".*", [&engine](const MValue &a, const MValue &b) { return builtin::times(engine.allocator(), a, b); });
    engine.registerBinaryOp("*",  [&engine](const MValue &a, const MValue &b) { return builtin::mtimes(engine.allocator(), a, b); });
    engine.registerBinaryOp("./", [&engine](const MValue &a, const MValue &b) { return builtin::rdivide(engine.allocator(), a, b); });
    engine.registerBinaryOp("/",  [&engine](const MValue &a, const MValue &b) { return builtin::mrdivide(engine.allocator(), a, b); });
    engine.registerBinaryOp("\\", [&engine](const MValue &a, const MValue &b) { return builtin::mldivide(engine.allocator(), a, b); });
    engine.registerBinaryOp("^",  [&engine](const MValue &a, const MValue &b) { return builtin::power(engine.allocator(), a, b); });
    engine.registerBinaryOp(".^", [&engine](const MValue &a, const MValue &b) { return builtin::elementPower(engine.allocator(), a, b); });

    engine.registerBinaryOp("==", [&engine](const MValue &a, const MValue &b) { return builtin::eq(engine.allocator(), a, b); });
    engine.registerBinaryOp("~=", [&engine](const MValue &a, const MValue &b) { return builtin::ne(engine.allocator(), a, b); });
    engine.registerBinaryOp("<",  [&engine](const MValue &a, const MValue &b) { return builtin::lt(engine.allocator(), a, b); });
    engine.registerBinaryOp(">",  [&engine](const MValue &a, const MValue &b) { return builtin::gt(engine.allocator(), a, b); });
    engine.registerBinaryOp("<=", [&engine](const MValue &a, const MValue &b) { return builtin::le(engine.allocator(), a, b); });
    engine.registerBinaryOp(">=", [&engine](const MValue &a, const MValue &b) { return builtin::ge(engine.allocator(), a, b); });

    engine.registerBinaryOp("&",  [&engine](const MValue &a, const MValue &b) { return builtin::logicalAnd(engine.allocator(), a, b); });
    engine.registerBinaryOp("|",  [&engine](const MValue &a, const MValue &b) { return builtin::logicalOr(engine.allocator(), a, b); });
}

} // namespace numkit::m
