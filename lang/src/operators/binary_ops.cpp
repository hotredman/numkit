// toolboxes/builtin/src/lang/operators/binary_ops.cpp

#include <numkit/lang/operators/binary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
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

namespace numkit::lang {

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

    // Matrix-multiply is undefined for N-D arrays (N > 2) except the
    // scalar * NDArray degenerate form, which is just an elementwise
    // scale and is handled further down. Match MATLAB's error here —
    // the pre-split code silently treated pages as 1 and produced
    // garbage, which is worse than failing loudly.
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

// Internal helper: solve A·X = B via square LU or tall QR. Returns the
// X column (n×nrhs) on success; throws on size mismatch / singular /
// rank-deficient / wide-system input. Caller is mldivide; mrdivide
// composes via the standard transpose trick X = A/B = (B'\A')'.

Value mrdivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return mrdivide(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
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
    if (a.isScalar() && !b.isScalar()) {
        // Per MATLAB R2025b: `2 / [1 2; 3 4]` errors with "Matrix
        // dimensions must agree". Match that behavior — do NOT silently
        // expand to scalar·inv(B).
        throw Error("mrdivide: matrix dimensions must agree",
                    0, 0, "mrdivide", "", "numkit:mrdivide:dim");
    }
    // Matrix right division: X = A / B  ↔  X · B = A.
    // Standard identity: X = (B' \ A')'. Same LU/QR primitives as mldivide.
    {
        Value Bt = transposeDouble(b, p);
        Value At = transposeDouble(a, p);
        Value Y  = matrixSolve(Bt, At, "mrdivide", p);
        return transposeDouble(Y, p);
    }
}

Value mldivide(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (a.isEmpty() || b.isEmpty())
        return emptyArithResult(a, b, p);
    if (a.type() == ValueType::LOGICAL || b.type() == ValueType::LOGICAL)
        return mldivide(coerceLogicalToDouble(a, p), coerceLogicalToDouble(b, p), p);
    if (a.isComplex() || b.isComplex())
        throw Error("mldivide: complex matrix systems not yet supported",
                    0, 0, "mldivide", "", "numkit:mldivide:complex");
    if (a.isScalar() && b.isScalar())
        return Value::scalar(b.toScalar() / a.toScalar(), p);
    if (a.isScalar() && !b.isScalar()) {
        // Scalar A: X = B / A elementwise.
        return elementwiseDouble(b, a, std::divides<double>{}, p);
    }
    // Matrix left division: A·X = B.  Square → LU; tall → QR (LSQ).
    return matrixSolve(a, b, "mldivide", p);
}

// MATLAB raises a negative real base to a non-integer exponent to a COMPLEX
// result (e.g. (-8)^(1/3) == 1+1.732i); an integer exponent stays real
// ((-8)^3 == -512). Returns true if any element-pair (base, exp) would be
// complex. Handles scalar / array.^scalar / scalar.^array / same-shape
// precisely; other broadcasts keep the real path (pre-existing).
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
        if (base < 0.0 && e != std::floor(e)) // negative base, non-integer exp -> complex
            return Value::complexScalar(std::pow(Complex(base, 0.0), e), p);
        return Value::scalar(std::pow(base, e), p);
    }
    // Matrix power A^n: when a is a square numeric matrix and b is an
    // integer scalar exponent, compute the matrix product chain
    // A·A·…·A (n times). Non-integer or non-square fall through to
    // the not-implemented error (eigendecomposition route is BACKLOG).
    if (b.isScalar() && !a.isScalar()) {
        const double bs = b.toScalar();
        const long n = (long)bs;
        if ((double)n == bs && n >= 0
            && a.dims().rows() == a.dims().cols()
            && a.dims().ndim() == 2) {
            const std::size_t R = a.dims().rows();
            // n=0 → identity matrix.
            if (n == 0) {
                auto I = Value::matrix(R, R, ValueType::DOUBLE, p);
                double *id = I.doubleDataMut();
                std::memset(id, 0, sizeof(double) * R * R);
                for (std::size_t i = 0; i < R; ++i) id[i * R + i] = 1.0;
                return I;
            }
            // Repeat-multiply via existing mtimes.
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
        if (powNeedsComplex(a, b)) { // any negative base ^ non-integer exp -> complex
            // Both operands are real here, so promoteToComplex (which only
            // promotes when the OTHER is already complex) is a no-op; force
            // both to complex before the elementwise complex pow.
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

// ── Comparisons ──────────────────────────────────────────────────────────


Value eq(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::EQ, a, b); }
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::NE, a, b); }
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LT, a, b); }
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GT, a, b); }
Value le(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LE, a, b); }
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GE, a, b); }

// ── Logical ──────────────────────────────────────────────────────────────


Value logicalAnd(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return logicalBinary("&", [](bool x, bool y) { return x && y; }, mr, a, b);
}

Value logicalOr(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return logicalBinary("|", [](bool x, bool y) { return x || y; }, mr, a, b);
}

// ── Named-function adapters for the binary operator set ──────────────
// Pack 11: lift the "works as op only" ⚠️ entries to ✅ by exposing
// each binary operator under its MATLAB function name. Each adapter is
// a thin wrapper over the existing public API.

} // namespace numkit::lang

// ════════════════════════════════════════════════════════════════════════
// Registration — forward BinaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const Value &a, const Value &b) { return numkit::lang::plus(a, b, engine.resource()); });
    engine.registerBinaryOp("-",  [&engine](const Value &a, const Value &b) { return numkit::lang::minus(a, b, engine.resource()); });
    engine.registerBinaryOp(".*", [&engine](const Value &a, const Value &b) { return numkit::lang::times(a, b, engine.resource()); });
    engine.registerBinaryOp("*",  [&engine](const Value &a, const Value &b) { return numkit::lang::mtimes(a, b, engine.resource()); });
    engine.registerBinaryOp("./", [&engine](const Value &a, const Value &b) { return numkit::lang::rdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("/",  [&engine](const Value &a, const Value &b) { return numkit::lang::mrdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("\\", [&engine](const Value &a, const Value &b) { return numkit::lang::mldivide(a, b, engine.resource()); });
    engine.registerBinaryOp("^",  [&engine](const Value &a, const Value &b) { return numkit::lang::power(a, b, engine.resource()); });
    engine.registerBinaryOp(".^", [&engine](const Value &a, const Value &b) { return numkit::lang::elementPower(a, b, engine.resource()); });

    engine.registerBinaryOp("==", [&engine](const Value &a, const Value &b) { return numkit::lang::eq(a, b, engine.resource()); });
    engine.registerBinaryOp("~=", [&engine](const Value &a, const Value &b) { return numkit::lang::ne(a, b, engine.resource()); });
    engine.registerBinaryOp("<",  [&engine](const Value &a, const Value &b) { return numkit::lang::lt(a, b, engine.resource()); });
    engine.registerBinaryOp(">",  [&engine](const Value &a, const Value &b) { return numkit::lang::gt(a, b, engine.resource()); });
    engine.registerBinaryOp("<=", [&engine](const Value &a, const Value &b) { return numkit::lang::le(a, b, engine.resource()); });
    engine.registerBinaryOp(">=", [&engine](const Value &a, const Value &b) { return numkit::lang::ge(a, b, engine.resource()); });

    engine.registerBinaryOp("&",  [&engine](const Value &a, const Value &b) { return numkit::lang::logicalAnd(a, b, engine.resource()); });
    engine.registerBinaryOp("|",  [&engine](const Value &a, const Value &b) { return numkit::lang::logicalOr(a, b, engine.resource()); });
}

} // namespace numkit
