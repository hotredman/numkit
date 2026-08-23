// src/builtin/src/ops/binary_ops_detail.hpp
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
#include <numkit/ops/reductions.hpp>
#include <numkit/ops/compare.hpp>

#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/shape_ops.hpp>

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

namespace numkit::builtin {

using namespace ::numkit::ops;

namespace {

inline bool sameShapeDoubleFastPath(const numkit::Value &a,
                                    const numkit::Value &b)
{
    return !a.isScalar() && !b.isScalar() && a.dims() == b.dims();
}

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

inline Value narrowIfReal(const Value &val, std::pmr::memory_resource *mr) {
    if (!val.isComplex()) return val;
    const std::complex<double> *cd = val.complexData();
    const std::size_t num = val.numel();
    for (std::size_t i = 0; i < num; ++i) {
        if (std::abs(cd[i].imag()) > 1e-14 * (1.0 + std::abs(cd[i].real())))
            return val;
    }
    const std::size_t rows = val.dims().rows();
    const std::size_t cols = val.dims().cols();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < num; ++i) od[i] = cd[i].real();
    return out;
}

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

    if (A.isComplex() || B.isComplex()) {
        using Complex = std::complex<double>;
        ScratchVec<Complex> A_buf(m * n, &arena);
        ScratchVec<Complex> B_buf(m * k, &arena);
        if (A.isComplex()) {
            std::copy(A.complexData(), A.complexData() + m * n, A_buf.begin());
        } else {
            const double *ad = A.doubleData();
            for (std::size_t i = 0; i < m * n; ++i) A_buf[i] = Complex(ad[i], 0.0);
        }
        if (B.isComplex()) {
            std::copy(B.complexData(), B.complexData() + m * k, B_buf.begin());
        } else {
            const double *bd = B.doubleData();
            for (std::size_t i = 0; i < m * k; ++i) B_buf[i] = Complex(bd[i], 0.0);
        }
        Value X = Value::complexMatrix(n, k, mr);
        if (!numkit::ops::la_solve(A_buf.data(), m, n, B_buf.data(), k, X.complexDataMut(), &arena))
            throw Error(std::string(opname)
                        + ": matrix is singular or rank-deficient",
                        0, 0, opname, "",
                        std::string("numkit:") + opname + ":singular");
        return narrowIfReal(X, mr);
    }

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
    if (a.isChar() && b.isChar()) {
        if (c == Cmp::EQ)
            return Value::logicalScalar(a.toString() == b.toString(), nullptr);
        if (c == Cmp::NE)
            return Value::logicalScalar(a.toString() != b.toString(), nullptr);
    }

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
            throw Error("Matrix dimensions must agree for comparison. a=[" + std::to_string(a.dims().rows()) + "x" + std::to_string(a.dims().cols()) + "], b=[" + std::to_string(b.dims().rows()) + "x" + std::to_string(b.dims().cols()) + "]",
                        0, 0, "compareImpl", "", "numkit:compare:dimMismatch");
        auto r = createLike(a, ValueType::LOGICAL, nullptr);
        for (size_t i = 0; i < a.numel(); ++i)
            r.logicalDataMut()[i] =
                (isEq ? ceq(getC(a, i), getC(b, i)) : !ceq(getC(a, i), getC(b, i)))
                    ? 1 : 0;
        return r;
    }

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

    auto getD = [](const Value &v, size_t r, size_t col) -> double {
        size_t idx = col * v.dims().rows() + r;
        if (v.isLogical()) return static_cast<double>(v.logicalData()[idx]);
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
        throw Error("Matrix dimensions must agree for comparison. a=[" + std::to_string(ar) + "x" + std::to_string(ac) + "], b=[" + std::to_string(br) + "x" + std::to_string(bc) + "]",
                    0, 0, "compareString", "", "numkit:compare:dimMismatch");

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
        for (size_t i = 0; i < v.numel(); ++i)
            r[i] = (v.elemAsDouble(i) != 0.0) ? 1 : 0;
    }
    return r;
}

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
        throw Error(std::string("Matrix dimensions must agree for ") + opName + ". a=[" + std::to_string(a.dims().rows()) + "x" + std::to_string(a.dims().cols()) + "], b=[" + std::to_string(b.dims().rows()) + "x" + std::to_string(b.dims().cols()) + "]",
                    0, 0, opName, "", "numkit:binary_ops:dimMismatch");
    auto aa = toBoolArray(a, &scratch);
    auto bb = toBoolArray(b, &scratch);
    auto r = createLike(a, ValueType::LOGICAL, mr);
    uint8_t *dst = r.logicalDataMut();
    for (size_t i = 0; i < aa.size(); ++i)
        dst[i] = op(static_cast<bool>(aa[i]), static_cast<bool>(bb[i])) ? 1 : 0;
    return r;
}

enum class TranspOp { None, Transpose, CTranspose };

template <typename T>
inline void runPageMatmul(const T *, const T *, T *,
                          size_t, size_t, size_t);

template <>
inline void runPageMatmul<double>(const double *a, const double *b, double *c,
                                  size_t M, size_t N, size_t K)
{
    ops::detail::matmulDoubleLoop(a, b, c, M, N, K);
}

template <>
inline void runPageMatmul<float>(const float *a, const float *b, float *c,
                                 size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        float *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = 0.0f;
        for (size_t k = 0; k < K; ++k) {
            const float bkj = b[j * K + k];
            const float *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <>
inline void runPageMatmul<Complex>(const Complex *a, const Complex *b, Complex *c,
                                   size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        Complex *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = Complex(0.0, 0.0);
        for (size_t k = 0; k < K; ++k) {
            const Complex bkj = b[j * K + k];
            const Complex *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <typename T> constexpr ValueType pagemtimesElemMType();
template <> constexpr ValueType pagemtimesElemMType<double >() { return ValueType::DOUBLE;  }
template <> constexpr ValueType pagemtimesElemMType<float  >() { return ValueType::SINGLE;  }
template <> constexpr ValueType pagemtimesElemMType<Complex>() { return ValueType::COMPLEX; }

template <typename T>
inline T readElemAsT(const Value &src, size_t i, bool typeMatches)
{
    if constexpr (std::is_same_v<T, Complex>) {
        if (typeMatches) return src.complexData()[i];
        return Complex(src.elemAsDouble(i), 0.0);
    } else {
        if (typeMatches) return static_cast<const T *>(src.rawData())[i];
        return static_cast<T>(src.elemAsDouble(i));
    }
}

template <typename T>
inline T conjIfComplex(T v)
{
    if constexpr (std::is_same_v<T, Complex>) return std::conj(v);
    else return v;
}

template <typename T>
void materialisePage(T *dst, const Value &src, size_t pageOff,
                     size_t rowDim, size_t colDim, TranspOp tr)
{
    const size_t pageElems = rowDim * colDim;
    const size_t base = pageOff * pageElems;
    const bool typeMatches = (src.type() == pagemtimesElemMType<T>());

    if (tr == TranspOp::None) {
        if (typeMatches) {
            std::memcpy(dst, static_cast<const T *>(src.rawData()) + base,
                        pageElems * sizeof(T));
        } else {
            for (size_t i = 0; i < pageElems; ++i)
                dst[i] = readElemAsT<T>(src, base + i, false);
        }
        return;
    }
    const bool needsConj = (tr == TranspOp::CTranspose);
    for (size_t r = 0; r < rowDim; ++r) {
        for (size_t c = 0; c < colDim; ++c) {
            const size_t srcOff = base + c * rowDim + r;
            T v = readElemAsT<T>(src, srcOff, typeMatches);
            if (needsConj) v = conjIfComplex<T>(v);
            dst[r * colDim + c] = v;
        }
    }
}

template <typename T>
Value pagemtimesImpl(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr)
{
    const auto &xd = x.dims();
    const auto &yd = y.dims();
    const int xnd = xd.ndim();
    const int ynd = yd.ndim();
    if (xnd < 2 || ynd < 2)
        throw Error("pagemtimes: each input must have at least 2 dimensions",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:rank");

    const size_t xRowDim = xd.dim(0), xColDim = xd.dim(1);
    const size_t yRowDim = yd.dim(0), yColDim = yd.dim(1);

    const size_t M  = (tx == TranspOp::None) ? xRowDim : xColDim;
    const size_t Kx = (tx == TranspOp::None) ? xColDim : xRowDim;
    const size_t Ky = (ty == TranspOp::None) ? yRowDim : yColDim;
    const size_t N  = (ty == TranspOp::None) ? yColDim : yRowDim;
    if (Kx != Ky)
        throw Error("pagemtimes: inner matrix dimensions must agree",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:innerdim");
    const size_t K = Kx;

    constexpr int kMaxNd = Dims::kMaxRank;
    const int xb = std::max(0, xnd - 2);
    const int yb = std::max(0, ynd - 2);
    const int outBatchNd = std::max(xb, yb);
    size_t xBatch[kMaxNd], yBatch[kMaxNd], outBatch[kMaxNd];
    for (int i = 0; i < outBatchNd; ++i) {
        xBatch[i] = (i < xb) ? xd.dim(2 + i) : 1;
        yBatch[i] = (i < yb) ? yd.dim(2 + i) : 1;
        if (xBatch[i] != yBatch[i] && xBatch[i] != 1 && yBatch[i] != 1)
            throw Error("pagemtimes: batch dimensions must broadcast "
                         "(each axis must match or be 1)",
                         0, 0, "pagemtimes", "", "numkit:pagemtimes:dimagree");
        outBatch[i] = std::max(xBatch[i], yBatch[i]);
    }

    size_t batchN = 1;
    for (int i = 0; i < outBatchNd; ++i) batchN *= outBatch[i];

    const int outNd = 2 + outBatchNd;
    size_t outDimArr[kMaxNd];
    outDimArr[0] = M;
    outDimArr[1] = N;
    for (int i = 0; i < outBatchNd; ++i) outDimArr[2 + i] = outBatch[i];
    auto z = createForDims(Dims(outDimArr, outNd), pagemtimesElemMType<T>(), mr);
    if (M == 0 || N == 0 || batchN == 0)
        return z;

    T *zData = static_cast<T *>(z.rawDataMut());
    const size_t xPageStride = xRowDim * xColDim;
    const size_t yPageStride = yRowDim * yColDim;
    const size_t zPageStride = M * N;

    const bool xDirect = (x.type() == pagemtimesElemMType<T>()) && (tx == TranspOp::None);
    const bool yDirect = (y.type() == pagemtimesElemMType<T>()) && (ty == TranspOp::None);
    ScratchArena scratch(mr);
    ScratchVec<T> scratchX(&scratch), scratchY(&scratch);
    if (!xDirect) scratchX.resize(xPageStride);
    if (!yDirect) scratchY.resize(yPageStride);

    auto getXPage = [&](size_t pageOff) -> const T * {
        if (xDirect)
            return static_cast<const T *>(x.rawData()) + pageOff * xPageStride;
        materialisePage(scratchX.data(), x, pageOff, xRowDim, xColDim, tx);
        return scratchX.data();
    };
    auto getYPage = [&](size_t pageOff) -> const T * {
        if (yDirect)
            return static_cast<const T *>(y.rawData()) + pageOff * yPageStride;
        materialisePage(scratchY.data(), y, pageOff, yRowDim, yColDim, ty);
        return scratchY.data();
    };

    if (outBatchNd == 0) {
        runPageMatmul<T>(getXPage(0), getYPage(0), zData, M, N, K);
        return z;
    }

    size_t xBatchStride[kMaxNd], yBatchStride[kMaxNd];
    {
        size_t sx = 1, sy = 1;
        for (int i = 0; i < outBatchNd; ++i) {
            xBatchStride[i] = sx;
            yBatchStride[i] = sy;
            sx *= xBatch[i];
            sy *= yBatch[i];
        }
    }

    size_t coords[kMaxNd] = {0};
    Dims outBatchDims(outBatch, outBatchNd);
    size_t pageIdx = 0;
    do {
        size_t xOff = 0, yOff = 0;
        for (int i = 0; i < outBatchNd; ++i) {
            const size_t xc = (xBatch[i] == 1) ? 0 : coords[i];
            const size_t yc = (yBatch[i] == 1) ? 0 : coords[i];
            xOff += xc * xBatchStride[i];
            yOff += yc * yBatchStride[i];
        }
        runPageMatmul<T>(getXPage(xOff), getYPage(yOff),
                         zData + pageIdx * zPageStride,
                         M, N, K);
        ++pageIdx;
    } while (incrementCoords(coords, outBatchDims));

    return z;
}

} // namespace

} // namespace numkit::builtin
