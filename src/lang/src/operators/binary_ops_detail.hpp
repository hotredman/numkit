// toolboxes/.../binary_ops_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by binary_ops.cpp + binary_ops_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
#include <numkit/ops/reductions.hpp>

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

namespace numkit::lang {

using namespace ::numkit::ops;

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

} // namespace numkit::lang
