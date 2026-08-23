// src/builtin/src/ops/binary_ops.cpp

#include <numkit/builtin/ops.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include <numkit/ops/la_solve.hpp>
#include <numkit/ops/binary_ops.hpp>
#include <numkit/ops/compare.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory_resource>
#include <string>

#include "binary_ops_detail.hpp"

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API — binary operators
// ════════════════════════════════════════════════════════════════════════

// ── Arithmetic ──────────────────────────────────────────────────────────

Value plus(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    // MATLAB auto-coerces logical to double for arithmetic. See BUGS.md #24.
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return plus(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::plus<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            ops::detail::plusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
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
    throw Error("Unsupported types for +", 0, 0, "plus", "", "numkit:plus:unsupportedTypes");
}

Value minus(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return minus(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::minus<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            ops::detail::minusLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::minus<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateSub(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for -", 0, 0, "minus", "", "numkit:minus:unsupportedTypes");
}

Value times(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return times(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            ops::detail::timesLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::multiplies<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateMul(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for .*", 0, 0, "times", "", "numkit:times:unsupportedTypes");
}

Value mtimes(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;

    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return mtimes(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);

    if ((a.dims().is3D() || b.dims().is3D()) && !a.isScalar() && !b.isScalar())
        throw Error("MTIMES is not supported for N-D arrays",
                     0, 0, "mtimes", "", "numkit:mtimes:notSupportedND");

    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        if (ca.isScalar() || cb.isScalar())
            return elementwiseComplex(a, b, std::multiplies<Complex>{}, p);
        size_t M = ca.dims().rows(), K = ca.dims().cols(), N = cb.dims().cols();
        if (K != cb.dims().rows())
            throw Error("Inner matrix dimensions must agree", 0, 0, "mtimes", "",
                         "numkit:innerdim");
        auto r = Value::complexMatrix(M, N, p);
        for (size_t i = 0; i < M; ++i)
            for (size_t j = 0; j < N; ++j) {
                Complex s(0, 0);
                for (size_t k = 0; k < K; ++k)
                    s += ca.complexElem(i, k) * cb.complexElem(k, j);
                r.complexDataMut()[j * M + i] = s;
            }
        return narrowComplex(r, p);
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
                         "numkit:innerdim");
        auto r = Value::matrix(M, N, ValueType::DOUBLE, p);
        ops::detail::matmulDoubleLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(),
                                 M, N, K);
        return r;
    }
    throw Error("Unsupported types for *", 0, 0, "mtimes", "", "numkit:mtimes:unsupportedTypes");
}

Value rdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return rdivide(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex())
        return elementwiseComplex(a, b, std::divides<Complex>{}, p);
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (sameShapeDoubleFastPath(a, b)) {
            auto r = createLike(a, ValueType::DOUBLE, p);
            ops::detail::rdivideLoop(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    }
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    throw Error("Unsupported types for ./", 0, 0, "rdivide", "",
                 "numkit:rdivide:unsupportedTypes");
}

Value ldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return rdivide(b, a, mr);
}

Value mrdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return mrdivide(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    {
        auto r = dispatchIntegerBinaryOp(a, b, [](auto x, auto y) { return saturateDiv(x, y); }, p);
        if (!r.isUnset()) return r;
    }
    if (b.isScalar()) {
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(a, b, std::divides<Complex>{}, p);
        return elementwiseDouble(a, b, std::divides<double>{}, p);
    }
    if (a.isScalar() && !b.isScalar()) {
        throw Error("mrdivide: matrix dimensions must agree",
                    0, 0, "mrdivide", "", "numkit:mrdivide:dim");
    }
    Value Bt = ctranspose(b, p);
    Value At = ctranspose(a, p);
    Value Y  = mldivide(Bt, At, p);
    return ctranspose(Y, p);
}

Value mldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return mldivide(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isScalar() && b.isScalar()) {
        if (a.isComplex() || b.isComplex()) {
            Complex ca = a.isComplex() ? a.complexData()[0] : Complex(a.doubleData()[0], 0.0);
            Complex cb = b.isComplex() ? b.complexData()[0] : Complex(b.doubleData()[0], 0.0);
            Complex res = cb / ca;
            return narrowIfReal(Value::complexScalar(res.real(), res.imag(), p), p);
        }
        return Value::scalar(b.toScalar() / a.toScalar(), p);
    }
    if (a.isScalar() && !b.isScalar()) {
        if (a.isComplex() || b.isComplex())
            return elementwiseComplex(b, a, std::divides<Complex>{}, p);
        return elementwiseDouble(b, a, std::divides<double>{}, p);
    }
    return matrixSolve(a, b, "mldivide", p);
}

static bool powNeedsComplex(const Value &a, const Value &b)
{
    if (a.type() != ValueType::DOUBLE || b.type() != ValueType::DOUBLE)
        return false;
    auto negBase = [](double x) { return x < 0.0; };
    auto fracExp = [](double y) { return y != std::floor(y); };
    if (a.isScalar() && b.isScalar())
        return negBase(a.toScalar()) && fracExp(b.toScalar());
    if (b.isScalar()) {
        if (!fracExp(b.toScalar())) return false;
        const double *da = a.doubleData();
        for (std::size_t i = 0; i < a.numel(); ++i)
            if (negBase(da[i])) return true;
        return false;
    }
    if (a.isScalar()) {
        if (!negBase(a.toScalar())) return false;
        const double *db = b.doubleData();
        for (std::size_t i = 0; i < b.numel(); ++i)
            if (fracExp(db[i])) return true;
        return false;
    }
    if (a.dims() == b.dims()) {
        const double *da = a.doubleData(), *db = b.doubleData();
        for (std::size_t i = 0; i < a.numel(); ++i)
            if (negBase(da[i]) && fracExp(db[i])) return true;
    }
    return false;
}

Value power(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return power(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex()) {
        auto [ca, cb] = promoteToComplex(a, b, p);
        return Value::complexScalar(std::pow(ca.toComplex(), cb.toComplex()), p);
    }
    if (a.isScalar() && b.isScalar()) {
        const double base = a.toScalar(), e = b.toScalar();
        if (base < 0.0 && e != std::floor(e))
            return Value::complexScalar(std::pow(Complex(base, 0.0), e), p);
        return Value::scalar(std::pow(base, e), p);
    }
    if (b.isScalar() && !a.isScalar()) {
        const double bs = b.toScalar();
        const long n = (long)bs;
        if ((double)n == bs && n >= 0
            && a.dims().rows() == a.dims().cols()
            && a.dims().ndim() == 2) {
            const std::size_t R = a.dims().rows();
            if (n == 0) {
                auto I = Value::matrix(R, R, ValueType::DOUBLE, p);
                double *id = I.doubleDataMut();
                std::memset(id, 0, sizeof(double) * R * R);
                for (std::size_t i = 0; i < R; ++i) id[i * R + i] = 1.0;
                return I;
            }
            Value acc = a;
            for (long k = 1; k < n; ++k) acc = mtimes(acc, a, p);
            return acc;
        }
    }
    throw Error("Matrix power not implemented", 0, 0, "power", "",
                 "numkit:power:notImplemented");
}

Value elementPower(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return elementPower(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex()) {
        return elementwiseComplex(
            a, b, [](const Complex &x, const Complex &y) { return std::pow(x, y); }, p);
    }
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE) {
        if (b.isScalar() && b.toScalar() == 2.0)
            return times(a, a, p);
        if (powNeedsComplex(a, b)) {
            Value ca = a, cb = b;
            ca.promoteToComplex(p);
            cb.promoteToComplex(p);
            return elementwiseComplex(
                ca, cb, [](const Complex &x, const Complex &y) { return std::pow(x, y); }, p);
        }
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
                 "numkit:elementPower:unsupportedTypes");
}

Value mpower(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return power(a, b, mr);
}

// ── Comparisons ──────────────────────────────────────────────────────────

Value eq(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::EQ, a, b); }
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::NE, a, b); }
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LT, a, b); }
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GT, a, b); }
Value le(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LE, a, b); }
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GE, a, b); }

// ── Logical ──────────────────────────────────────────────────────────────

Value logical_and(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return logicalBinary("&", [](bool x, bool y) { return x && y; }, mr, a, b);
}

Value logical_or(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return logicalBinary("|", [](bool x, bool y) { return x || y; }, mr, a, b);
}

Value logical_xor(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return logicalBinary("xor", [](bool x, bool y) { return (x || y) && !(x && y); }, mr, a, b);
}

} // namespace numkit::builtin
