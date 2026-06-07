// libs/builtin/src/lang/operators/binary_ops.cpp

#include <numkit/builtin/language/operators/binary_ops.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

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

// MATLAB auto-coerces logical to double in arithmetic ops. Cast a
// LOGICAL Value into a fresh DOUBLE Value of the same shape; pass
// through any other type unchanged. See BUGS.md #24.
inline numkit::Value coerceLogicalToDouble(const numkit::Value &v,
                                           std::pmr::memory_resource *mr)
{
    using namespace numkit;
    if (v.type() != ValueType::LOGICAL) return v;
    Value out = numkit::createLike(v, ValueType::DOUBLE, mr);
    const uint8_t *src = v.logicalData();
    double *dst = out.doubleDataMut();
    const size_t n = v.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<double>(src[i]);
    return out;
}

} // namespace

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
namespace {

// Read an N-element column-major Value into a fresh row-by-row buffer.
void copyColumnMajorDouble(const Value &v, double *dst)
{
    const std::size_t n = v.numel();
    if (v.type() == ValueType::DOUBLE) {
        const double *src = v.doubleData();
        std::copy(src, src + n, dst);
    } else {
        for (std::size_t i = 0; i < n; ++i) dst[i] = v.elemAsDouble(i);
    }
}

// Solve A·X = B via la_solve, packed up as Value math. A is m×n, B is m×k.
// Output is n×k. Uses scratch arena; final result is placed on `mr`.
Value matrixSolve(const Value &A, const Value &B, const char *opname, std::pmr::memory_resource *mr)
{
    const std::size_t m  = A.dims().rows();
    const std::size_t n  = A.dims().cols();
    const std::size_t bm = B.dims().rows();
    const std::size_t k  = B.dims().cols();
    if (bm != m)
        throw Error(std::string(opname) + ": matrix dimensions must agree",
                    0, 0, opname, "", std::string("numkit:") + opname + ":dim");
    if (m < n)
        throw Error(std::string(opname)
                    + ": underdetermined (wide A, m<n) not yet supported",
                    0, 0, opname, "",
                    std::string("numkit:") + opname + ":wide");

    ScratchArena arena(mr);
    ScratchVec<double> A_buf(m * n, &arena);
    ScratchVec<double> B_buf(m * k, &arena);
    copyColumnMajorDouble(A, A_buf.data());
    copyColumnMajorDouble(B, B_buf.data());

    Value X = Value::matrix(n, k, ValueType::DOUBLE, mr);
    double *Xd = X.doubleDataMut();
    if (!numkit::ops::la_solve(A_buf.data(), m, n, B_buf.data(), k, Xd, &arena))
        throw Error(std::string(opname)
                    + ": matrix is singular or rank-deficient",
                    0, 0, opname, "",
                    std::string("numkit:") + opname + ":singular");
    return X;
}

// Compute the transpose of an m×n DOUBLE Value (column-major) into a
// new n×m DOUBLE Value on `mr`.
Value transposeDouble(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t m = A.dims().rows();
    const std::size_t n = A.dims().cols();
    Value T = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *Td = T.doubleDataMut();
    if (A.type() == ValueType::DOUBLE) {
        const double *Ad = A.doubleData();
        for (std::size_t j = 0; j < n; ++j)
            for (std::size_t i = 0; i < m; ++i)
                Td[j + i * n] = Ad[i + j * m];
    } else {
        for (std::size_t j = 0; j < n; ++j)
            for (std::size_t i = 0; i < m; ++i)
                Td[j + i * n] = A.elemAsDouble(i + j * m);
    }
    return T;
}

} // namespace

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
                         0, 0, "compare", "", "numkit:compare:stringType");
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
                         0, 0, "compare", "", "numkit:compare:complexOrder");
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
                         0, 0, "compare", "", "numkit:dimagree");
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
        case Cmp::EQ: r = ops::eqFast(a, b); break;
        case Cmp::NE: r = ops::neFast(a, b); break;
        case Cmp::LT: r = ops::ltFast(a, b); break;
        case Cmp::GT: r = ops::gtFast(a, b); break;
        case Cmp::LE: r = ops::leFast(a, b); break;
        case Cmp::GE: r = ops::geFast(a, b); break;
        }
        if (!r.isUnset()) return r;
    }

    // Numeric — double/logical/integer/single with broadcasting
    auto getD = [](const Value &v, size_t r, size_t col) -> double {
        size_t idx = col * v.dims().rows() + r;
        if (v.isLogical()) return static_cast<double>(v.logicalData()[idx]);
        // Index element idx for ANY numeric type. (Previously this called
        // toScalar() for integer/single, which threw on non-scalar arrays —
        // breaking e.g. `uint8Image > 0`.)
        if (isIntegerType(v.type()) || v.type() == ValueType::SINGLE) return v.elemAsDouble(idx);
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
                         0, 0, "compare", "", "numkit:dimagree");
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
                         0, 0, "compare", "", "numkit:dimagree");

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
                     0, 0, "compare", "", "numkit:dimagree");

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

Value eq(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::EQ, a, b); }
Value ne(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::NE, a, b); }
Value lt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LT, a, b); }
Value gt(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GT, a, b); }
Value le(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::LE, a, b); }
Value ge(const Value &a, const Value &b, std::pmr::memory_resource *) { return compareImpl(Cmp::GE, a, b); }

// ── Logical ──────────────────────────────────────────────────────────────

namespace {

ScratchVec<uint8_t> toBoolArray(const Value &v, std::pmr::memory_resource *mr)
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
    } else if (v.isComplex()) {
        const Complex *d = v.complexData();
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = (d[i].real() != 0.0 || d[i].imag() != 0.0) ? 1 : 0;
    } else {
        // single / int* / char — element-wise nonzero. (Previously this
        // called toBool(), which threw on a non-scalar integer/single array
        // and only ever set r[0].)
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = (v.elemAsDouble(i) != 0.0) ? 1 : 0;
    }
    return r;
}

// Truthiness of a SCALAR operand for &/|. Works for any numeric/logical
// type (toBool() only handles double/logical/complex scalars).
bool scalarTruth(const Value &v)
{
    if (v.isComplex()) {
        Complex c = v.toComplex();
        return c.real() != 0.0 || c.imag() != 0.0;
    }
    return v.elemAsDouble(0) != 0.0;
}

template <typename Op>
Value logicalBinary(const char *opName, Op op,
                     std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.isScalar() && b.isScalar())
        return Value::logicalScalar(op(scalarTruth(a), scalarTruth(b)), mr);
    ScratchArena scratch(mr);
    if (a.isScalar()) {
        bool av = scalarTruth(a);
        auto bb = toBoolArray(b, &scratch);
        auto r = createLike(b, ValueType::LOGICAL, mr);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < bb.size(); ++i)
            dst[i] = op(av, static_cast<bool>(bb[i])) ? 1 : 0;
        return r;
    }
    if (b.isScalar()) {
        bool bv = scalarTruth(b);
        auto aa = toBoolArray(a, &scratch);
        auto r = createLike(a, ValueType::LOGICAL, mr);
        uint8_t *dst = r.logicalDataMut();
        for (size_t i = 0; i < aa.size(); ++i)
            dst[i] = op(static_cast<bool>(aa[i]), bv) ? 1 : 0;
        return r;
    }
    if (a.numel() != b.numel())
        throw Error(std::string("Matrix dimensions must agree for ") + opName,
                     0, 0, opName, "", "numkit:dimagree");
    auto aa = toBoolArray(a, &scratch);
    auto bb = toBoolArray(b, &scratch);
    auto r = createLike(a, ValueType::LOGICAL, mr);
    uint8_t *dst = r.logicalDataMut();
    for (size_t i = 0; i < aa.size(); ++i)
        dst[i] = op(static_cast<bool>(aa[i]), static_cast<bool>(bb[i])) ? 1 : 0;
    return r;
}

} // namespace

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
namespace detail {

#define NK_BINOP_REG(MATLAB_NAME, CXX_FN)                                            \
    void MATLAB_NAME##_reg(Span<const Value> args, size_t /*nargout*/,              \
                           Span<Value> outs, CallContext &ctx)                       \
    {                                                                                 \
        if (args.size() < 2)                                                          \
            throw Error(#MATLAB_NAME ": requires 2 arguments",                       \
                         0, 0, #MATLAB_NAME, "", "numkit:" #MATLAB_NAME ":nargin");        \
        outs[0] = CXX_FN(args[0], args[1], ctx.engine->resource());                  \
    }

NK_BINOP_REG(plus,     plus)
NK_BINOP_REG(minus,    minus)
NK_BINOP_REG(times,    times)
NK_BINOP_REG(mtimes,   mtimes)
NK_BINOP_REG(rdivide,  rdivide)
NK_BINOP_REG(mrdivide, mrdivide)
NK_BINOP_REG(mldivide, mldivide)
NK_BINOP_REG(power,    elementPower)   // MATLAB power(a,b) = a.^b → C++ elementPower
NK_BINOP_REG(mpower,   power)          // MATLAB mpower(a,b) = a^b  → C++ power
NK_BINOP_REG(eq,       eq)
NK_BINOP_REG(ne,       ne)
NK_BINOP_REG(lt,       lt)
NK_BINOP_REG(le,       le)
NK_BINOP_REG(gt,       gt)
NK_BINOP_REG(ge,       ge)

#undef NK_BINOP_REG

// ldivide(a, b) = b ./ a (MATLAB convention).
void ldivide_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ldivide: requires 2 arguments",
                     0, 0, "ldivide", "", "numkit:ldivide:nargin");
    outs[0] = rdivide(args[1], args[0], ctx.engine->resource());
}

// `and` / `or` builtins map to logicalAnd / logicalOr.
void and_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("and: requires 2 arguments",
                     0, 0, "and", "", "numkit:and:nargin");
    outs[0] = logicalAnd(args[0], args[1], ctx.engine->resource());
}

void or_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("or: requires 2 arguments",
                     0, 0, "or", "", "numkit:or:nargin");
    outs[0] = logicalOr(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — forward BinaryOpFunc closures to the public API
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerBinaryOps(Engine &engine)
{
    engine.registerBinaryOp("+",  [&engine](const Value &a, const Value &b) { return builtin::plus(a, b, engine.resource()); });
    engine.registerBinaryOp("-",  [&engine](const Value &a, const Value &b) { return builtin::minus(a, b, engine.resource()); });
    engine.registerBinaryOp(".*", [&engine](const Value &a, const Value &b) { return builtin::times(a, b, engine.resource()); });
    engine.registerBinaryOp("*",  [&engine](const Value &a, const Value &b) { return builtin::mtimes(a, b, engine.resource()); });
    engine.registerBinaryOp("./", [&engine](const Value &a, const Value &b) { return builtin::rdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("/",  [&engine](const Value &a, const Value &b) { return builtin::mrdivide(a, b, engine.resource()); });
    engine.registerBinaryOp("\\", [&engine](const Value &a, const Value &b) { return builtin::mldivide(a, b, engine.resource()); });
    engine.registerBinaryOp("^",  [&engine](const Value &a, const Value &b) { return builtin::power(a, b, engine.resource()); });
    engine.registerBinaryOp(".^", [&engine](const Value &a, const Value &b) { return builtin::elementPower(a, b, engine.resource()); });

    engine.registerBinaryOp("==", [&engine](const Value &a, const Value &b) { return builtin::eq(a, b, engine.resource()); });
    engine.registerBinaryOp("~=", [&engine](const Value &a, const Value &b) { return builtin::ne(a, b, engine.resource()); });
    engine.registerBinaryOp("<",  [&engine](const Value &a, const Value &b) { return builtin::lt(a, b, engine.resource()); });
    engine.registerBinaryOp(">",  [&engine](const Value &a, const Value &b) { return builtin::gt(a, b, engine.resource()); });
    engine.registerBinaryOp("<=", [&engine](const Value &a, const Value &b) { return builtin::le(a, b, engine.resource()); });
    engine.registerBinaryOp(">=", [&engine](const Value &a, const Value &b) { return builtin::ge(a, b, engine.resource()); });

    engine.registerBinaryOp("&",  [&engine](const Value &a, const Value &b) { return builtin::logicalAnd(a, b, engine.resource()); });
    engine.registerBinaryOp("|",  [&engine](const Value &a, const Value &b) { return builtin::logicalOr(a, b, engine.resource()); });
}

} // namespace numkit
