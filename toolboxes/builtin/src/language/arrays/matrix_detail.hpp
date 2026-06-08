// toolboxes/.../matrix_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by matrix.cpp + matrix_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"

#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

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

namespace {

// Siamese / de la Loubère method for odd N >= 3.
// Fills positions [0..N²-1] starting at (0, N/2) and stepping
// (-1, +1) mod N; on collision step (+1, 0) instead.
void magicOdd(double *p, size_t N)
{
    size_t r = 0;
    size_t c = N / 2;
    for (size_t k = 1; k <= N * N; ++k) {
        p[r * N + c] = static_cast<double>(k);
        const size_t nr = (r == 0) ? (N - 1) : (r - 1);
        const size_t nc = (c + 1) % N;
        if (p[nr * N + nc] != 0.0) {
            r = (r + 1) % N;          // collision: drop down
        } else {
            r = nr;
            c = nc;
        }
    }
}

// Doubly-even (N ≡ 0 mod 4): start with the natural 1..N² fill and
// swap each cell whose (i mod 4, j mod 4) is on either of the two
// 4×4-block diagonals.
void magicDoublyEven(double *p, size_t N)
{
    const size_t total = N * N;
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j) {
            const size_t k = i * N + j + 1;       // 1-based natural fill
            const size_t mi = i % 4;
            const size_t mj = j % 4;
            const bool diag = (mi == mj) || (mi + mj == 3);
            p[i * N + j] = static_cast<double>(diag ? (total + 1 - k) : k);
        }
}

// Singly-even (N ≡ 2 mod 4, N >= 6) via Strachey's method.
// This mirrors MATLAB R2025b's magic.m verbatim:
//   p = N/2;  K = (N-2)/4;
//   M = [Q  Q+2p²; Q+3p²  Q+p²]   where Q = magic(p)
//   For columns j in {0..K-1, N-K+1..N-1}:
//     swap rows r ∈ {0..p-1} with rows r+p (column-by-column).
//   Then for row mid = K (0-indexed = (p-1)/2):
//     swap (mid, 0) ↔ (mid+p, 0)   -- undo the previous swap on this cell
//     swap (mid, K) ↔ (mid+p, K)   -- and apply the strached swap instead
void magicSinglyEven(double *p, size_t N)
{
    const size_t P = N / 2;          // odd
    const size_t S = P * P;
    const size_t K = (N - 2) / 4;    // num "full" left/right cols to swap

    // Build one (P×P) odd-magic and tile into four quadrants.
    std::vector<double> sq(P * P, 0.0);
    magicOdd(sq.data(), P);

    for (size_t i = 0; i < P; ++i) {
        for (size_t j = 0; j < P; ++j) {
            const double s = sq[i * P + j];
            p[(i)     * N + (j)]     = s;                    // A (top-left)
            p[(i)     * N + (j + P)] = s + 2.0 * S;          // C (top-right)
            p[(i + P) * N + (j)]     = s + 3.0 * S;          // D (bottom-left)
            p[(i + P) * N + (j + P)] = s + 1.0 * S;          // B (bottom-right)
        }
    }

    // Bulk column swaps: leftmost K and rightmost K-1 columns get
    // top-half / bottom-half rows swapped. (For N=6, K=1 → swap col 0
    // only; right side has K-1=0 cols, none.)
    auto swapRowsAtCol = [&](size_t col) {
        for (size_t r = 0; r < P; ++r)
            std::swap(p[r * N + col], p[(r + P) * N + col]);
    };
    for (size_t c = 0; c < K; ++c)
        swapRowsAtCol(c);
    for (size_t c = N - K + 1; c < N && K > 0; ++c)
        swapRowsAtCol(c);

    // Middle-row fix: for row mid = K (0-indexed), undo column-0 swap
    // and apply column-K swap instead. (MATLAB: i=k+1, j=[1, i].)
    const size_t mid = K;
    if (mid < P) {
        std::swap(p[mid * N + 0], p[(mid + P) * N + 0]);  // undo
        std::swap(p[mid * N + K], p[(mid + P) * N + K]);  // apply
    }
}

} // anonymous namespace
namespace {

// Extract Value as a flat double buffer in linear element order via
// elemAsDouble (handles every numeric/logical type). Returns the
// buffer; live for the lifetime of `scratch`.
ScratchVec<double> valueToScratchDoubles(const Value &v, ScratchArena &scratch)
{
    const std::size_t n = v.numel();
    ScratchVec<double> out(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // namespace
namespace {

// Per-page transpose helper. `conjugate` flips the sign of imaginary
// parts when input is COMPLEX. For DOUBLE / SINGLE inputs the flag is
// ignored at the element level (no-op).
template <typename T>
Value pageTransposeT(const Value &x, ValueType ty, bool conjugate, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const size_t M = d.rows(), N = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1u;

    auto out = (P == 1u)
        ? Value::matrix(N, M, ty, mr)
        : Value::matrix3d(N, M, P, ty, mr);

    const T *src = static_cast<const T *>(x.rawData());
    T *dst       = static_cast<T *>(out.rawDataMut());
    const size_t pageInElems  = M * N;
    const size_t pageOutElems = N * M;

    for (size_t p = 0; p < P; ++p) {
        const T *sp = src + p * pageInElems;
        T *dp       = dst + p * pageOutElems;
        for (size_t j = 0; j < N; ++j) {
            for (size_t i = 0; i < M; ++i) {
                if constexpr (std::is_same_v<T, Complex>) {
                    Complex v = sp[j * M + i];
                    dp[i * N + j] = conjugate ? std::conj(v) : v;
                } else {
                    (void)conjugate;
                    dp[i * N + j] = sp[j * M + i];
                }
            }
        }
    }
    return out;
}

Value pageTransposeAny(const Value &x, bool conjugate, std::pmr::memory_resource *mr)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return pageTransposeT<double>(x, ValueType::DOUBLE, conjugate, mr);
    case ValueType::SINGLE:  return pageTransposeT<float>(x, ValueType::SINGLE, conjugate, mr);
    case ValueType::COMPLEX: return pageTransposeT<Complex>(x, ValueType::COMPLEX, conjugate, mr);
    case ValueType::INT8:    return pageTransposeT<int8_t>(x, ValueType::INT8, conjugate, mr);
    case ValueType::INT16:   return pageTransposeT<int16_t>(x, ValueType::INT16, conjugate, mr);
    case ValueType::INT32:   return pageTransposeT<int32_t>(x, ValueType::INT32, conjugate, mr);
    case ValueType::INT64:   return pageTransposeT<int64_t>(x, ValueType::INT64, conjugate, mr);
    case ValueType::UINT8:   return pageTransposeT<uint8_t>(x, ValueType::UINT8, conjugate, mr);
    case ValueType::UINT16:  return pageTransposeT<uint16_t>(x, ValueType::UINT16, conjugate, mr);
    case ValueType::UINT32:  return pageTransposeT<uint32_t>(x, ValueType::UINT32, conjugate, mr);
    case ValueType::UINT64:  return pageTransposeT<uint64_t>(x, ValueType::UINT64, conjugate, mr);
    case ValueType::LOGICAL: return pageTransposeT<uint8_t>(x, ValueType::LOGICAL, conjugate, mr);
    default:
        throw Error("pagetranspose: unsupported input type",
                     0, 0, "pagetranspose", "", "numkit:pagetranspose:badType");
    }
}

} // namespace
namespace {

// Per-page matmul kernel, parameterised by element type. The DOUBLE
// specialisation hands off to the SIMD-aware matmulDoubleLoop in the
// backend; the SINGLE one uses the same (j, k, i) ordering as a
// portable inline loop.
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

// Read element i of `src` as T. For T = Complex, real-typed sources
// upgrade to (real, 0); for T ∈ {double, float}, complex sources are
// rejected upstream so we never reach the if-branch with COMPLEX input.
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

// Conjugate a value if T is Complex; identity for real T.
template <typename T>
inline T conjIfComplex(T v)
{
    if constexpr (std::is_same_v<T, Complex>) return std::conj(v);
    else return v;
}

// Materialise one page from `src` into typed scratch `dst`, optionally
// transposing (and conjugating, for ctranspose on Complex). Direct copy
// (no per-element conversion) when src already holds the target type
// AND no transpose is needed.
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
    // Transpose: dst is colDim × rowDim col-major;
    // dst[r * colDim + c] = src[c * rowDim + r] (then conjugate if ctranspose+Complex).
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

    // Direct-pass when source already matches T and no transpose is
    // needed; otherwise materialise into typed scratch (one per call,
    // reused across all batch pages).
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
namespace {

// Promote to a 2D DOUBLE matrix for row-tuple ops. Returns a copy if the
// type or shape differs; for already-2D-DOUBLE input returns by value
// (cheap COW in the engine).
Value toDoubleMatrix2D(const Value &x, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": input must be 2D",
                     0, 0, fn, "", std::string("numkit:") + fn + ":bad2D");
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    if (x.type() == ValueType::DOUBLE) {
        // Return a fresh DOUBLE matrix identical to x — cheap, avoids
        // touching the input through a shared buffer later.
        auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
        if (x.numel() > 0)
            std::memcpy(r.doubleDataMut(), x.doubleData(),
                        x.numel() * sizeof(double));
        return r;
    }
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < x.numel(); ++i)
        dst[i] = x.elemAsDouble(i);
    return r;
}

// Complex sortrows (MATLAB rule): rows are ordered lexicographically by the
// requested columns; each column compares complex entries by magnitude |z|
// then phase angle arg(z) ascending (a negative column index sorts that
// column descending). A NaN component sorts last. Must NOT drop the
// imaginary part (toDoubleMatrix2D does) — that silently returns wrong rows.
inline bool cxRowLess(Complex a, Complex b)
{
    const double am = std::abs(a), bm = std::abs(b);
    const bool an = std::isnan(am), bn = std::isnan(bm);
    if (an || bn) { if (an && bn) return false; return bn; }   // non-NaN < NaN
    if (am != bm) return am < bm;
    return std::arg(a) < std::arg(b);
}

std::tuple<Value, Value>
sortRowsComplex(const Value &x, const int *cols, std::size_t nCols,
                std::pmr::memory_resource *mr)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error("sortrows: input must be 2D",
                     0, 0, "sortrows", "", "numkit:sortrows:bad2D");
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    if (R == 0)
        return std::make_tuple(x, Value::matrix(0, 1, ValueType::DOUBLE, mr));

    ScratchArena scratch(mr);
    ScratchVec<int> sortKeys(&scratch);
    if (nCols == 0) {
        sortKeys.reserve(C);
        for (size_t c = 1; c <= C; ++c) sortKeys.push_back(static_cast<int>(c));
    } else {
        sortKeys.assign(cols, cols + nCols);
        for (int rawCol : sortKeys) {
            const int absC = (rawCol < 0) ? -rawCol : rawCol;
            if (rawCol == 0 || static_cast<size_t>(absC) > C)
                throw Error("sortrows: column index out of range",
                             0, 0, "sortrows", "", "numkit:sortrows:badCol");
        }
    }

    const Complex *src = x.complexData();
    auto perm = ScratchVec<size_t>(R, &scratch);
    for (size_t i = 0; i < R; ++i) perm[i] = i;
    std::stable_sort(perm.begin(), perm.end(),
        [&](size_t a, size_t b) {
            for (int key : sortKeys) {
                const bool desc = key < 0;
                const size_t c = static_cast<size_t>((desc ? -key : key) - 1);
                const Complex va = src[c * R + a], vb = src[c * R + b];
                if (cxRowLess(va, vb)) return !desc;
                if (cxRowLess(vb, va)) return desc;
            }
            return false;   // all keys equal → stable
        });

    auto out = Value::matrix(R, C, ValueType::COMPLEX, mr);
    Complex *dst = out.complexDataMut();
    for (size_t i = 0; i < R; ++i)
        for (size_t c = 0; c < C; ++c)
            dst[c * R + i] = src[c * R + perm[i]];
    auto idx = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *idxP = idx.doubleDataMut();
    for (size_t i = 0; i < R; ++i)
        idxP[i] = static_cast<double>(perm[i] + 1);
    return std::make_tuple(std::move(out), std::move(idx));
}

std::tuple<Value, Value>
sortRowsImpl(const Value &x, const int *cols, std::size_t nCols, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        return sortRowsComplex(x, cols, nCols, mr);
    auto m = toDoubleMatrix2D(x, "sortrows", mr);
    const size_t R = m.dims().rows();
    const size_t C = m.dims().cols();

    if (R == 0) {
        // Empty rows — return as-is and an empty 0×1 idx column.
        auto idx = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return std::make_tuple(std::move(m), std::move(idx));
    }

    ScratchArena scratch(mr);

    // Validate cols list. nCols==0 ⇒ all columns ascending in order.
    ScratchVec<int> sortKeys(&scratch);
    if (nCols == 0) {
        sortKeys.reserve(C);
        for (size_t c = 1; c <= C; ++c)
            sortKeys.push_back(static_cast<int>(c));
    } else {
        sortKeys.assign(cols, cols + nCols);
        for (int rawCol : sortKeys) {
            const int absC = (rawCol < 0) ? -rawCol : rawCol;
            if (rawCol == 0 || static_cast<size_t>(absC) > C)
                throw Error("sortrows: column index out of range",
                             0, 0, "sortrows", "", "numkit:sortrows:badCol");
        }
    }

    auto perm = ScratchVec<size_t>(R, &scratch);
    for (size_t i = 0; i < R; ++i) perm[i] = i;

    const double *src = m.doubleData();
    std::stable_sort(perm.begin(), perm.end(),
        [&](size_t a, size_t b) {
            return detail::rowLexCmpByCols(src, C, R, a, b,
                                            sortKeys.data(), sortKeys.size()) < 0;
        });

    auto sorted = detail::collectRowsByIndex(mr, m, perm.data(), perm.size());
    auto idx = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *idxP = idx.doubleDataMut();
    for (size_t i = 0; i < R; ++i)
        idxP[i] = static_cast<double>(perm[i] + 1);
    return std::make_tuple(std::move(sorted), std::move(idx));
}

} // namespace
namespace {

// Type-aware predicate: element at linear index i non-zero?
// NaN counts as non-zero (NaN != 0). For COMPLEX both parts checked.
template <typename T>
inline bool isNonzeroElemT(const T *p, size_t i) { return p[i] != T{0}; }

inline bool isNonzeroComplex(const Complex *p, size_t i)
{
    return p[i].real() != 0.0 || p[i].imag() != 0.0;
}

template <typename Visit>
void forEachNonzero(const Value &x, Visit visit)
{
    const size_t n = x.numel();
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        for (size_t i = 0; i < n; ++i) if (p[i]) visit(i);
        break;
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroComplex(p, i)) visit(i);
        break;
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    default:
        throw Error("nnz/nonzeros: unsupported element type",
                     0, 0, "nnz", "", "numkit:nnz:badType");
    }
}

template <typename T>
T *typedDstFor(Value &r, ValueType outType)
{
    switch (outType) {
    case ValueType::LOGICAL: return reinterpret_cast<T *>(r.logicalDataMut());
    case ValueType::DOUBLE:  return reinterpret_cast<T *>(r.doubleDataMut());
    case ValueType::SINGLE:  return reinterpret_cast<T *>(r.singleDataMut());
    case ValueType::COMPLEX: return reinterpret_cast<T *>(r.complexDataMut());
    case ValueType::INT8:    return reinterpret_cast<T *>(r.int8DataMut());
    case ValueType::INT16:   return reinterpret_cast<T *>(r.int16DataMut());
    case ValueType::INT32:   return reinterpret_cast<T *>(r.int32DataMut());
    case ValueType::INT64:   return reinterpret_cast<T *>(r.int64DataMut());
    case ValueType::UINT8:   return reinterpret_cast<T *>(r.uint8DataMut());
    case ValueType::UINT16:  return reinterpret_cast<T *>(r.uint16DataMut());
    case ValueType::UINT32:  return reinterpret_cast<T *>(r.uint32DataMut());
    case ValueType::UINT64:  return reinterpret_cast<T *>(r.uint64DataMut());
    default: return nullptr;
    }
}

template <typename T, typename Reader>
Value collectTypedNonzeros(const Value &x, ValueType outType, Reader read, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<T> vals(&scratch);
    forEachNonzero(x, [&](size_t i) { vals.push_back(read(i)); });
    auto r = Value::matrix(vals.size(), 1, outType, mr);
    if (!vals.empty()) {
        T *dst = typedDstFor<T>(r, outType);
        std::memcpy(dst, vals.data(), vals.size() * sizeof(T));
    }
    return r;
}

} // namespace
namespace {

// Integer cumsum / cumprod. MATLAB keeps the integer class and accumulates
// NATIVELY with saturation at each step — the saturated running value is
// carried forward, so cumsum(int8([100 100 -100]))=[100 127 27] int8 and
// cumprod(int8([5 10 10]))=[5 50 127] int8. Generic strided scan over the
// chosen dimension d (column-major: stride = prod(dims[0..d-2]),
// len = dims[d-1]). Linear iteration is valid for any dim because element i
// depends only on i-stride (< i). NOTE: int64/uint64 above 2^53 lose
// precision (accumulated through double) — the same limitation as the rest
// of numkit's numeric core; int8/16/32 and uint8/16/32 are exact.
template <typename T>
void cumIntegerScanInto(const Value &x, T *dst, size_t strideD, size_t lenD,
                        bool isProd)
{
    const double lo = static_cast<double>(std::numeric_limits<T>::min());
    const double hi = static_cast<double>(std::numeric_limits<T>::max());
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        const size_t coord = (i / strideD) % lenD;
        const double cur = x.elemAsDouble(i);
        double v = cur;
        if (coord != 0) {
            const double prev = static_cast<double>(dst[i - strideD]);
            v = isProd ? prev * cur : prev + cur;
        }
        if (v < lo) v = lo;
        else if (v > hi) v = hi;        // saturate to the class range
        dst[i] = static_cast<T>(v);
    }
}

Value cumIntegerNative(const Value &x, int dim, bool isProd,
                       std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    int d;
    if (dim > 0) {
        d = detail::resolveDim(x, dim, isProd ? "cumprod" : "cumsum");
    } else {
        d = 1;                          // first non-singleton dim (MATLAB default)
        for (int k = 0; k < nd; ++k)
            if (dd.dim(k) > 1) { d = k + 1; break; }
    }
    size_t strideD = 1;
    for (int k = 0; k < d - 1; ++k) strideD *= dd.dim(k);
    const size_t lenD = (d - 1 < nd) ? dd.dim(d - 1) : 1;

    size_t outDims[Dims::kMaxRank];
    for (int k = 0; k < nd; ++k) outDims[k] = dd.dim(k);
    Value r = Value::matrixND(outDims, nd, x.type(), mr);
    if (x.numel() == 0 || lenD == 0 || strideD == 0) return r;

    switch (x.type()) {
    case ValueType::INT8:   cumIntegerScanInto<int8_t>  (x, r.int8DataMut(),   strideD, lenD, isProd); break;
    case ValueType::INT16:  cumIntegerScanInto<int16_t> (x, r.int16DataMut(),  strideD, lenD, isProd); break;
    case ValueType::INT32:  cumIntegerScanInto<int32_t> (x, r.int32DataMut(),  strideD, lenD, isProd); break;
    case ValueType::INT64:  cumIntegerScanInto<int64_t> (x, r.int64DataMut(),  strideD, lenD, isProd); break;
    case ValueType::UINT8:  cumIntegerScanInto<uint8_t> (x, r.uint8DataMut(),  strideD, lenD, isProd); break;
    case ValueType::UINT16: cumIntegerScanInto<uint16_t>(x, r.uint16DataMut(), strideD, lenD, isProd); break;
    case ValueType::UINT32: cumIntegerScanInto<uint32_t>(x, r.uint32DataMut(), strideD, lenD, isProd); break;
    case ValueType::UINT64: cumIntegerScanInto<uint64_t>(x, r.uint64DataMut(), strideD, lenD, isProd); break;
    default: break;
    }
    return r;
}

// Inclusive cumulative scan of a COMPLEX array along the 1-based dim d (sum
// when isProd=false, product when isProd=true). Output shape == input shape.
// Uniform inner-block (B) / outer-count (O) strides cover vector/2D/3D/ND.
// (Complex isn't perf-critical here — correctness over a SIMD prefix scan.)
Value cumComplexAlongDim(const Value &x, int d, bool isProd,
                         std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("cumsum: rank exceeds 32",
                     0, 0, "cumsum", "", "numkit:cumsum:tooManyDims");
    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = dd.dim(i);
    auto r = Value::matrixND(outDims, nd, ValueType::COMPLEX, mr);
    if (x.numel() == 0) return r;
    const size_t sliceLen = (d >= 1 && d <= nd) ? dd.dim(d - 1) : 1;
    if (sliceLen == 0) return r;
    size_t B = 1; for (int i = 0; i < d - 1 && i < nd; ++i) B *= dd.dim(i);
    size_t O = 1; for (int i = d; i < nd; ++i) O *= dd.dim(i);
    const Complex *src = x.complexData();
    Complex *dst = r.complexDataMut();
    for (size_t o = 0; o < O; ++o)
        for (size_t b = 0; b < B; ++b) {
            const size_t base = o * sliceLen * B + b;
            Complex acc = src[base];
            dst[base] = acc;
            for (size_t k = 1; k < sliceLen; ++k) {
                const Complex v = src[base + k * B];
                acc = isProd ? acc * v : acc + v;
                dst[base + k * B] = acc;
            }
        }
    return r;
}

// First non-singleton dimension (1-based), default 1 — MATLAB's default op dim.
int firstNonSingletonDim(const Value &x)
{
    const auto &dd = x.dims();
    for (int k = 0; k < dd.ndim(); ++k)
        if (dd.dim(k) > 1) return k + 1;
    return 1;
}

} // namespace
namespace {

template <typename Op>
void cumKernel(const Value &x, int d, Op op, double *dst)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const double *src = x.doubleData();

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                if (R == 0) continue;
                double acc = src[base];
                dst[base] = acc;
                for (size_t rr = 1; rr < R; ++rr) {
                    acc = op(acc, src[base + rr]);
                    dst[base + rr] = acc;
                }
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                const size_t pageBase = pp * R * C;
                if (C == 0) continue;
                double acc = src[pageBase + rr];
                dst[pageBase + rr] = acc;
                for (size_t c = 1; c < C; ++c) {
                    acc = op(acc, src[pageBase + c * R + rr]);
                    dst[pageBase + c * R + rr] = acc;
                }
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                if (P == 0) continue;
                double acc = src[c * R + rr];
                dst[c * R + rr] = acc;
                for (size_t pp = 1; pp < P; ++pp) {
                    acc = op(acc, src[pp * R * C + c * R + rr]);
                    dst[pp * R * C + c * R + rr] = acc;
                }
            }
    }
}

template <typename Op>
Value cumImpl(const Value &x, int dim, Op op, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    if (x.dims().isVector() || x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(),
                                ValueType::DOUBLE, mr);
        if (x.numel() == 0) return r;
        double acc = x.doubleData()[0];
        r.doubleDataMut()[0] = acc;
        for (size_t i = 1; i < x.numel(); ++i) {
            acc = op(acc, x.doubleData()[i]);
            r.doubleDataMut()[i] = acc;
        }
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    auto r = dd.is3D() ? Value::matrix3d(dd.rows(), dd.cols(), dd.pages(),
                                          ValueType::DOUBLE, mr)
                       : Value::matrix(dd.rows(), dd.cols(),
                                        ValueType::DOUBLE, mr);
    cumKernel(x, d, op, r.doubleDataMut());
    return r;
}

} // namespace
namespace {

using ScanFn = void (*)(const double *, double *, std::size_t);

template <typename Op>
Value cumScanDispatch(const Value &x, int dim, ScanFn scan, Op scalarOp, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        scan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();

    // ND fallback (rank ≥ 4): per-slice scan along axis d-1.
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error(std::string(fn) + ": rank exceeds 32",
                         0, 0, fn, "", std::string("numkit:") + fn + ":tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, dd.ndim(), ValueType::DOUBLE, mr);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                scan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc = scalarOp(acc, src[base + k * B]);
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                       : Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // Per-column scan — column data is contiguous, route through SIMD.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                scan(src + base, dst + base, R);
            }
    } else {
        // dim=2/3: strided access; reuse the existing scalar cumKernel.
        cumKernel(x, d, scalarOp, dst);
    }
    return r;
}

} // namespace
namespace {
// cummax / cummin PRESERVE the logical class (unlike cumsum / cumprod, which
// promote logical → double). Run the double kernel on a promoted copy, then
// narrow the 0/1 result back to LOGICAL — mirrors xorOf's double→logical step.
Value logicalizeCumResult(const Value &d, std::pmr::memory_resource *mr)
{
    if (d.isScalar()) return Value::logicalScalar(d.toScalar() != 0.0, mr);
    Value r = createLike(d, ValueType::LOGICAL, mr);
    uint8_t *dst = r.logicalDataMut();
    const double *src = d.doubleData();
    const size_t n = d.numel();
    for (size_t i = 0; i < n; ++i) dst[i] = (src[i] != 0.0) ? 1 : 0;
    return r;
}

// Narrow a sorted DOUBLE result (exact char code points) back to a CHAR Value
// of the SAME shape — used by the char branch of sort_reg (MATLAB sorts char
// by code point, preserving the char class). Cannot reuse toChar(): it routes
// through fromString and FLATTENS a matrix to a row. createLike preserves dims
// (2-D / N-D); the column-major layout matches the double buffer 1:1.
Value charizeSortResult(const Value &d, std::pmr::memory_resource *mr)
{
    Value r = createLike(d, ValueType::CHAR, mr);
    char *dst = r.charDataMut();
    const double *src = d.doubleData();
    const size_t n = d.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<char>(static_cast<int>(src[i]));
    return r;
}
} // namespace
namespace {

// One pass of forward differences along axis `d` (1-based). Source has
// dim[d-1] = sliceLen; output has dim[d-1] = sliceLen - 1. Column-major
// strides (innerStride = prod(dim[0..d-2])).
void diffOnceDouble(const double *src, double *dst,
                    const Dims &srcDims, int d)
{
    const int nd = srcDims.ndim();
    const size_t sliceLen = srcDims.dim(d - 1);
    if (sliceLen < 2) return;  // out has zero elements

    size_t innerStride = 1;
    for (int i = 0; i < d - 1; ++i) innerStride *= srcDims.dim(i);
    size_t outerCount = 1;
    for (int i = d; i < nd; ++i) outerCount *= srcDims.dim(i);
    const size_t outSliceLen = sliceLen - 1;

    if (innerStride == 1) {
        // Contiguous along the diff axis — simple linear pass per outer block.
        for (size_t o = 0; o < outerCount; ++o) {
            const double *s = src + o * sliceLen;
            double *t = dst + o * outSliceLen;
            for (size_t k = 0; k < outSliceLen; ++k)
                t[k] = s[k + 1] - s[k];
        }
    } else {
        for (size_t o = 0; o < outerCount; ++o)
            for (size_t b = 0; b < innerStride; ++b) {
                const size_t srcBase = o * innerStride * sliceLen + b;
                const size_t dstBase = o * innerStride * outSliceLen + b;
                for (size_t k = 0; k < outSliceLen; ++k)
                    dst[dstBase + k * innerStride] =
                        src[srcBase + (k + 1) * innerStride] -
                        src[srcBase + k * innerStride];
            }
    }
}

Value makeDiffOutput(const Dims &srcDims, int d, size_t step, ValueType vt,
                     std::pmr::memory_resource *mr)
{
    const int nd = srcDims.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("diff: rank exceeds 32",
                     0, 0, "diff", "", "numkit:diff:tooManyDims");
    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = srcDims.dim(i);
    outDims[d - 1] = (outDims[d - 1] >= step) ? outDims[d - 1] - step : 0;
    return Value::matrixND(outDims, nd, vt, mr);
}

// Complex counterpart of diffOnceDouble — successive differences over Complex
// storage (real and imaginary differenced together), same strided layout.
void diffOnceComplex(const Complex *src, Complex *dst, const Dims &srcDims, int d)
{
    const int nd = srcDims.ndim();
    const size_t sliceLen = srcDims.dim(d - 1);
    if (sliceLen < 2) return;
    size_t innerStride = 1;
    for (int i = 0; i < d - 1; ++i) innerStride *= srcDims.dim(i);
    size_t outerCount = 1;
    for (int i = d; i < nd; ++i) outerCount *= srcDims.dim(i);
    const size_t outSliceLen = sliceLen - 1;
    if (innerStride == 1) {
        for (size_t o = 0; o < outerCount; ++o) {
            const Complex *s = src + o * sliceLen;
            Complex *t = dst + o * outSliceLen;
            for (size_t k = 0; k < outSliceLen; ++k) t[k] = s[k + 1] - s[k];
        }
    } else {
        for (size_t o = 0; o < outerCount; ++o)
            for (size_t b = 0; b < innerStride; ++b) {
                const size_t srcBase = o * innerStride * sliceLen + b;
                const size_t dstBase = o * innerStride * outSliceLen + b;
                for (size_t k = 0; k < outSliceLen; ++k)
                    dst[dstBase + k * innerStride] =
                        src[srcBase + (k + 1) * innerStride] -
                        src[srcBase + k * innerStride];
            }
    }
}

// Identity copy of a COMPLEX value (preserves shape + both parts).
Value copyComplexSameShape(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    size_t dims[Dims::kMaxRank];
    for (int i = 0; i < nd; ++i) dims[i] = dd.dim(i);
    auto r = Value::matrixND(dims, nd, ValueType::COMPLEX, mr);
    if (x.numel() > 0)
        std::memcpy(r.complexDataMut(), x.complexData(), x.numel() * sizeof(Complex));
    return r;
}

Value copyToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    size_t dims[kMaxNd];
    for (int i = 0; i < nd; ++i) dims[i] = dd.dim(i);
    auto r = Value::matrixND(dims, nd, ValueType::DOUBLE, mr);
    if (x.type() == ValueType::DOUBLE) {
        std::memcpy(r.doubleDataMut(), x.doubleData(),
                    x.numel() * sizeof(double));
    } else {
        double *dst = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = x.elemAsDouble(i);
    }
    return r;
}

// One pass of an integer-typed difference along dim d, with NATIVE
// saturation (matches MATLAB: diff(int8([-100 100]))=127 int8). Same strided
// layout as diffOnceDouble.
template <typename T>
void diffOnceIntegerT(const T *src, T *dst, const Dims &srcDims, int d)
{
    const int nd = srcDims.ndim();
    const size_t sliceLen = srcDims.dim(d - 1);
    size_t innerStride = 1;
    for (int i = 0; i < d - 1; ++i) innerStride *= srcDims.dim(i);
    size_t outerCount = 1;
    for (int i = d; i < nd; ++i) outerCount *= srcDims.dim(i);
    const size_t outSliceLen = sliceLen - 1;
    const double lo = static_cast<double>(std::numeric_limits<T>::min());
    const double hi = static_cast<double>(std::numeric_limits<T>::max());
    auto sub = [&](T a, T b) -> T {
        double v = static_cast<double>(a) - static_cast<double>(b);
        if (v < lo) v = lo;
        else if (v > hi) v = hi;
        return static_cast<T>(v);
    };
    if (innerStride == 1) {
        for (size_t o = 0; o < outerCount; ++o) {
            const T *s = src + o * sliceLen;
            T *t = dst + o * outSliceLen;
            for (size_t k = 0; k < outSliceLen; ++k) t[k] = sub(s[k + 1], s[k]);
        }
    } else {
        for (size_t o = 0; o < outerCount; ++o)
            for (size_t b = 0; b < innerStride; ++b) {
                const size_t srcBase = o * innerStride * sliceLen + b;
                const size_t dstBase = o * innerStride * outSliceLen + b;
                for (size_t k = 0; k < outSliceLen; ++k)
                    dst[dstBase + k * innerStride] =
                        sub(src[srcBase + (k + 1) * innerStride],
                            src[srcBase + k * innerStride]);
            }
    }
}

// n-th order diff of an integer-typed Value: keeps the class and saturates at
// each pass (the saturated pass-k result feeds pass k+1). Caller guarantees
// sliceLen > n along d.
Value diffInteger(const Value &x, int n, int d, std::pmr::memory_resource *mr)
{
    const ValueType vt = x.type();
    Value cur = copyIntegerSameClass(x, mr);
    for (int pass = 0; pass < n; ++pass) {
        const auto &curDims = cur.dims();
        const int nd = curDims.ndim();
        size_t outDims[Dims::kMaxRank];
        for (int i = 0; i < nd; ++i) outDims[i] = curDims.dim(i);
        outDims[d - 1] = (outDims[d - 1] >= 1) ? outDims[d - 1] - 1 : 0;
        Value out = Value::matrixND(outDims, nd, vt, mr);
        switch (vt) {
        case ValueType::INT8:   diffOnceIntegerT<int8_t>  (cur.int8Data(),   out.int8DataMut(),   curDims, d); break;
        case ValueType::INT16:  diffOnceIntegerT<int16_t> (cur.int16Data(),  out.int16DataMut(),  curDims, d); break;
        case ValueType::INT32:  diffOnceIntegerT<int32_t> (cur.int32Data(),  out.int32DataMut(),  curDims, d); break;
        case ValueType::INT64:  diffOnceIntegerT<int64_t> (cur.int64Data(),  out.int64DataMut(),  curDims, d); break;
        case ValueType::UINT8:  diffOnceIntegerT<uint8_t> (cur.uint8Data(),  out.uint8DataMut(),  curDims, d); break;
        case ValueType::UINT16: diffOnceIntegerT<uint16_t>(cur.uint16Data(), out.uint16DataMut(), curDims, d); break;
        case ValueType::UINT32: diffOnceIntegerT<uint32_t>(cur.uint32Data(), out.uint32DataMut(), curDims, d); break;
        case ValueType::UINT64: diffOnceIntegerT<uint64_t>(cur.uint64Data(), out.uint64DataMut(), curDims, d); break;
        default: break;
        }
        cur = std::move(out);
    }
    return cur;
}

} // namespace
namespace {

// Used by xor below — small inputs, no need for a SIMD path.
Value promoteToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::DOUBLE) return x;
    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.elemAsDouble(i);
    return r;
}

} // namespace

// topkrows worker (def in matrix.cpp, external).
Value topkrows_full(const Value &A, std::size_t k,
                    const std::vector<std::size_t> &colsIn,
                    const std::vector<std::uint8_t> &descIn,
                    std::vector<std::size_t> *out_idx,
                    std::pmr::memory_resource *mr);

} // namespace numkit::builtin
