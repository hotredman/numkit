// libs/builtin/src/lang/arrays/matrix.cpp

#include <numkit/builtin/lang/arrays/matrix.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"
#include "lang/operators/backends/binary_ops_loops.hpp"
#include "math/elementary/backends/cumsum.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <type_traits>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Constructors ──────────────────────────────────────────────────────
Value zeros(std::pmr::memory_resource *mr, size_t rows, size_t cols, size_t pages)
{
    return createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
}

Value ones(std::pmr::memory_resource *mr, size_t rows, size_t cols, size_t pages)
{
    auto m = createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

// ND overloads: caller passes a flat dim list. For nd <= 3 these just
// route to the legacy 2D/3D ctors via createMatrixND; nd > 3 hits the
// Value::matrixND ctor and the SBO Dims storage.
Value zerosND(std::pmr::memory_resource *mr, const size_t *dims, std::size_t nDims)
{
    return createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
}

Value onesND(std::pmr::memory_resource *mr, const size_t *dims, std::size_t nDims)
{
    auto m = createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

Value eye(std::pmr::memory_resource *mr, size_t rows, size_t cols)
{
    auto m = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < std::min(rows, cols); ++i)
        m.elem(i, i) = 1.0;
    return m;
}

// ── Shape queries ────────────────────────────────────────────────────
Value size(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &dims = x.dims();
    // Output ndim: at least 2 (MATLAB convention — a row vector reports
    // [1, n], not [n]). Otherwise the actual rank, including any extra
    // dims past 3.
    const int n = std::max(2, dims.ndim());
    auto sv = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *out = sv.doubleDataMut();
    for (int i = 0; i < n; ++i)
        out[i] = static_cast<double>(dims.dim(i));
    return sv;
}

Value size(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return Value::scalar(static_cast<double>(x.dims().dimSize(dim - 1)), mr);
}

std::tuple<Value, Value> sizePair(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &dims = x.dims();
    return std::make_tuple(
        Value::scalar(static_cast<double>(dims.rows()), mr),
        Value::scalar(static_cast<double>(dims.cols()), mr));
}

Value length(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty() || x.numel() == 0)
        return Value::scalar(0.0, mr);
    const auto &dims = x.dims();
    const double len = static_cast<double>(std::max({dims.rows(), dims.cols(), dims.pages()}));
    return Value::scalar(len, mr);
}

Value numel(std::pmr::memory_resource *mr, const Value &x)
{
    return Value::scalar(static_cast<double>(x.numel()), mr);
}

Value ndims(std::pmr::memory_resource *mr, const Value &x)
{
    return Value::scalar(static_cast<double>(x.dims().ndims()), mr);
}

// ── Shape transformations ────────────────────────────────────────────
Value reshape(std::pmr::memory_resource *mr, const Value &x, size_t rows, size_t cols, size_t pages)
{
    const size_t newNumel = rows * cols * (pages == 0 ? 1 : pages);
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    DimsArg d{rows, cols, pages};

    // CELL and STRING store element-wise, not in the raw buffer — memcpy
    // wouldn't copy Value members.
    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        const bool is3D = d.pages > 0;
        Value r = (x.type() == ValueType::CELL)
            ? (is3D ? Value::cell3D(d.rows, d.cols, d.pages)
                    : Value::cell(d.rows, d.cols))
            : (is3D ? Value::stringArray3D(d.rows, d.cols, d.pages)
                    : Value::stringArray(d.rows, d.cols));
        auto &src = x.cellDataVec();
        auto &dst = r.cellDataVec();
        for (size_t i = 0; i < src.size() && i < dst.size(); ++i)
            dst[i] = src[i];
        return r;
    }

    auto r = createMatrix(d, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

// ND reshape. Same elem-count check, then route to matrixND for nd > 3.
// CELL/STRING ND not supported yet (matches the 2D/3D behaviour: only
// CELL/STRING currently handles 2D and 3D shapes via cell3D/stringArray3D).
Value reshapeND(std::pmr::memory_resource *mr, const Value &x,
                 const size_t *dims, std::size_t nDims)
{
    size_t newNumel = 1;
    for (std::size_t i = 0; i < nDims; ++i) newNumel *= dims[i];
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        if (nDims > 3)
            throw Error("reshape: ND CELL/STRING (>3) not yet supported",
                         0, 0, "reshape", "", "m:reshape:cellND");
        // Fall through to legacy path for 2D / 3D cell.
        const size_t r = nDims > 0 ? dims[0] : 1;
        const size_t c = nDims > 1 ? dims[1] : 1;
        const size_t p = nDims > 2 ? dims[2] : 0;
        return reshape(mr, x, r, c, p);
    }

    auto r = createMatrixND(dims, nDims, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

Value transpose(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    auto r = Value::matrix(cols, rows, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            r.elem(j, i) = x(i, j);
    return r;
}

// ── pagemtimes: page-wise matrix multiply ──────────────────────────────
//
// MATLAB R2020b+ batched matmul. Treats axes 1-2 of each operand as the
// matrix (M×K, K×N) and axes ≥3 as a batch index. Output batch shape is
// the NumPy broadcast of the two batch shapes. Supports DOUBLE and
// SINGLE (mixed → SINGLE, matching MATLAB's promotion rule).
//
//   Z = pagemtimes(X, Y)                     // tx = ty = None
//   Z = pagemtimes(X, "transpose", Y, "none")
//
// 'transpose' / 'ctranspose' transpose each X (or Y) page before
// multiply. For real input the two flags are identical (no imaginary
// component to conjugate). 2D × 2D collapses to ordinary matmul. One
// operand may be 2D (broadcast across the other's batch dims).

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
    detail::matmulDoubleLoop(a, b, c, M, N, K);
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
Value pagemtimesImpl(std::pmr::memory_resource *mr,
                      const Value &x, TranspOp tx,
                      const Value &y, TranspOp ty)
{
    const auto &xd = x.dims();
    const auto &yd = y.dims();
    const int xnd = xd.ndim();
    const int ynd = yd.ndim();
    if (xnd < 2 || ynd < 2)
        throw Error("pagemtimes: each input must have at least 2 dimensions",
                     0, 0, "pagemtimes", "", "m:pagemtimes:rank");

    const size_t xRowDim = xd.dim(0), xColDim = xd.dim(1);
    const size_t yRowDim = yd.dim(0), yColDim = yd.dim(1);

    const size_t M  = (tx == TranspOp::None) ? xRowDim : xColDim;
    const size_t Kx = (tx == TranspOp::None) ? xColDim : xRowDim;
    const size_t Ky = (ty == TranspOp::None) ? yRowDim : yColDim;
    const size_t N  = (ty == TranspOp::None) ? yColDim : yRowDim;
    if (Kx != Ky)
        throw Error("pagemtimes: inner matrix dimensions must agree",
                     0, 0, "pagemtimes", "", "m:pagemtimes:innerdim");
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
                         0, 0, "pagemtimes", "", "m:pagemtimes:dimagree");
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

Value pagemtimes(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    return pagemtimes(mr, x, TranspOp::None, y, TranspOp::None);
}

Value pagemtimes(std::pmr::memory_resource *mr,
                  const Value &x, TranspOp tx,
                  const Value &y, TranspOp ty)
{
    // MATLAB type promotion: COMPLEX wins over real; SINGLE wins over
    // DOUBLE. Integer/logical/char inputs are rejected — pagemtimes
    // requires floating or complex inputs.
    auto isFloatLike = [](ValueType t) {
        return t == ValueType::DOUBLE || t == ValueType::SINGLE || t == ValueType::COMPLEX;
    };
    if (!isFloatLike(x.type()) || !isFloatLike(y.type()))
        throw Error("pagemtimes: inputs must be 'single', 'double', or complex",
                     0, 0, "pagemtimes", "", "m:pagemtimes:type");
    if (x.isComplex() || y.isComplex())
        return pagemtimesImpl<Complex>(mr, x, tx, y, ty);
    if (x.type() == ValueType::SINGLE || y.type() == ValueType::SINGLE)
        return pagemtimesImpl<float  >(mr, x, tx, y, ty);
    return     pagemtimesImpl<double >(mr, x, tx, y, ty);
}

Value diag(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.dims().isVector()) {
        const size_t n = x.numel();
        auto r = Value::matrix(n, n, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < n; ++i)
            r.elem(i, i) = x.doubleData()[i];
        return r;
    }
    const size_t n = std::min(x.dims().rows(), x.dims().cols());
    auto r = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = x(i, i);
    return r;
}

// ── Sort / find ──────────────────────────────────────────────────────
std::tuple<Value, Value> sort(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isScalar())
        return std::make_tuple(x, Value::scalar(1.0, mr));

    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const int sortDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
    const size_t N = (sortDim == 0) ? R : (sortDim == 1) ? C : P;

    auto r = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                             : Value::matrix(R, C, ValueType::DOUBLE, mr);
    auto idx = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                               : Value::matrix(R, C, ValueType::DOUBLE, mr);

    const size_t slice0 = (sortDim == 0) ? 1 : R;
    const size_t slice1 = (sortDim == 1) ? 1 : C;
    const size_t slice2 = (sortDim == 2) ? 1 : P;
    ScratchArena scratch(mr);
    ScratchVec<std::pair<double, size_t>> buf(N, &scratch);

    for (size_t pp = 0; pp < slice2; ++pp)
        for (size_t c = 0; c < slice1; ++c)
            for (size_t rr = 0; rr < slice0; ++rr) {
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    buf[k] = {x.doubleData()[pIdx * R * C + cIdx * R + rIdx], k};
                }
                std::sort(buf.begin(), buf.end(),
                          [](const auto &a, const auto &b) { return a.first < b.first; });
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    const size_t lin = pIdx * R * C + cIdx * R + rIdx;
                    r.doubleDataMut()[lin] = buf[k].first;
                    idx.doubleDataMut()[lin] = static_cast<double>(buf[k].second + 1);
                }
            }
    return std::make_tuple(std::move(r), std::move(idx));
}

// ── sortrows ─────────────────────────────────────────────────────────
namespace {

// Promote to a 2D DOUBLE matrix for row-tuple ops. Returns a copy if the
// type or shape differs; for already-2D-DOUBLE input returns by value
// (cheap COW in the engine).
Value toDoubleMatrix2D(std::pmr::memory_resource *mr, const Value &x, const char *fn)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": input must be 2D",
                     0, 0, fn, "", std::string("m:") + fn + ":bad2D");
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

std::tuple<Value, Value>
sortRowsImpl(std::pmr::memory_resource *mr, const Value &x,
             const int *cols, std::size_t nCols)
{
    auto m = toDoubleMatrix2D(mr, x, "sortrows");
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
                             0, 0, "sortrows", "", "m:sortrows:badCol");
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

std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x)
{
    return sortRowsImpl(mr, x, nullptr, 0);
}

std::tuple<Value, Value> sortrows(std::pmr::memory_resource *mr, const Value &x,
                                    const int *cols, std::size_t nCols)
{
    return sortRowsImpl(mr, x, cols, nCols);
}

Value find(std::pmr::memory_resource *mr, const Value &x)
{
    ScratchArena scratch(mr);
    auto indices = ScratchVec<double>(&scratch);
    if (x.isLogical()) {
        const uint8_t *ld = x.logicalData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (ld[i])
                indices.push_back(static_cast<double>(i + 1));
    } else {
        const double *dd = x.doubleData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (dd[i] != 0.0)
                indices.push_back(static_cast<double>(i + 1));
    }
    const bool rowResult = !x.dims().is3D() && x.dims().rows() == 1;
    auto r = rowResult ? Value::matrix(1, indices.size(), ValueType::DOUBLE, mr)
                       : Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    if (!indices.empty())
        std::memcpy(r.doubleDataMut(), indices.data(), indices.size() * sizeof(double));
    return r;
}

// ── nnz / nonzeros ───────────────────────────────────────────────────
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
                     0, 0, "nnz", "", "m:nnz:badType");
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
Value collectTypedNonzeros(std::pmr::memory_resource *mr, const Value &x,
                            ValueType outType, Reader read)
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

Value nnz(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.numel() == 0)
        return Value::scalar(0.0, mr);
    size_t count = 0;
    forEachNonzero(x, [&](size_t) { ++count; });
    return Value::scalar(static_cast<double>(count), mr);
}

Value nonzeros(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.numel() == 0) {
        // Empty input → 0×1 column of the source type (DOUBLE if unknown).
        const ValueType outT = (x.type() == ValueType::EMPTY) ? ValueType::DOUBLE : x.type();
        return Value::matrix(0, 1, outT, mr);
    }
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        return collectTypedNonzeros<uint8_t>(mr, x, ValueType::LOGICAL,
            [&](size_t i) -> uint8_t { return p[i]; });
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        return collectTypedNonzeros<double>(mr, x, ValueType::DOUBLE,
            [&](size_t i) -> double { return p[i]; });
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        return collectTypedNonzeros<float>(mr, x, ValueType::SINGLE,
            [&](size_t i) -> float { return p[i]; });
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        return collectTypedNonzeros<Complex>(mr, x, ValueType::COMPLEX,
            [&](size_t i) -> Complex { return p[i]; });
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        return collectTypedNonzeros<int8_t>(mr, x, ValueType::INT8,
            [&](size_t i) -> int8_t { return p[i]; });
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        return collectTypedNonzeros<int16_t>(mr, x, ValueType::INT16,
            [&](size_t i) -> int16_t { return p[i]; });
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        return collectTypedNonzeros<int32_t>(mr, x, ValueType::INT32,
            [&](size_t i) -> int32_t { return p[i]; });
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        return collectTypedNonzeros<int64_t>(mr, x, ValueType::INT64,
            [&](size_t i) -> int64_t { return p[i]; });
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        return collectTypedNonzeros<uint8_t>(mr, x, ValueType::UINT8,
            [&](size_t i) -> uint8_t { return p[i]; });
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        return collectTypedNonzeros<uint16_t>(mr, x, ValueType::UINT16,
            [&](size_t i) -> uint16_t { return p[i]; });
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        return collectTypedNonzeros<uint32_t>(mr, x, ValueType::UINT32,
            [&](size_t i) -> uint32_t { return p[i]; });
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        return collectTypedNonzeros<uint64_t>(mr, x, ValueType::UINT64,
            [&](size_t i) -> uint64_t { return p[i]; });
    }
    default:
        throw Error("nonzeros: unsupported element type",
                     0, 0, "nonzeros", "", "m:nonzeros:badType");
    }
}

// ── Concatenation ────────────────────────────────────────────────────
Value horzcat(std::pmr::memory_resource *mr, const Value *values, size_t count)
{
    if (count == 0)
        return Value::empty();
    return Value::horzcat(values, count, mr);
}

Value vertcat(std::pmr::memory_resource *mr, const Value *values, size_t count)
{
    if (count == 0)
        return Value::empty();
    return Value::vertcat(values, count, mr);
}

// ── Grids ────────────────────────────────────────────────────────────
std::tuple<Value, Value> meshgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    const size_t nx = x.numel(), ny = y.numel();
    auto X = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < ny; ++r)
        for (size_t c = 0; c < nx; ++c) {
            X.elem(r, c) = x.doubleData()[c];
            Y.elem(r, c) = y.doubleData()[r];
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

// ── ndgrid ──────────────────────────────────────────────────────────
std::tuple<Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    const size_t nx = x.numel(), ny = y.numel();
    // Output shape: [nx, ny] — first arg is row dim (axes-major).
    auto X = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < nx; ++r)
        for (size_t c = 0; c < ny; ++c) {
            X.elem(r, c) = x.elemAsDouble(r);
            Y.elem(r, c) = y.elemAsDouble(c);
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

std::tuple<Value, Value, Value>
ndgrid(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &z)
{
    const size_t nx = x.numel(), ny = y.numel(), nz = z.numel();
    auto X = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Y = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Z = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    for (size_t p = 0; p < nz; ++p)
        for (size_t c = 0; c < ny; ++c)
            for (size_t r = 0; r < nx; ++r) {
                X.elem(r, c, p) = x.elemAsDouble(r);
                Y.elem(r, c, p) = y.elemAsDouble(c);
                Z.elem(r, c, p) = z.elemAsDouble(p);
            }
    return std::make_tuple(std::move(X), std::move(Y), std::move(Z));
}

// ── kron ────────────────────────────────────────────────────────────
Value kron(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.type() == ValueType::COMPLEX || b.type() == ValueType::COMPLEX)
        throw Error("kron: complex inputs are not supported",
                     0, 0, "kron", "", "m:kron:complex");
    if (a.dims().is3D() || a.dims().ndim() > 2
        || b.dims().is3D() || b.dims().ndim() > 2)
        throw Error("kron: inputs must be 2D",
                     0, 0, "kron", "", "m:kron:rank");

    const size_t rA = a.dims().rows(), cA = a.dims().cols();
    const size_t rB = b.dims().rows(), cB = b.dims().cols();
    const size_t rOut = rA * rB, cOut = cA * cB;

    auto out = Value::matrix(rOut, cOut, ValueType::DOUBLE, mr);
    if (rOut == 0 || cOut == 0) return out;

    double *dst = out.doubleDataMut();
    for (size_t ja = 0; ja < cA; ++ja)
        for (size_t ia = 0; ia < rA; ++ia) {
            const double av = a.elemAsDouble(ia + ja * rA);
            for (size_t jb = 0; jb < cB; ++jb) {
                const size_t jOut = ja * cB + jb;
                for (size_t ib = 0; ib < rB; ++ib) {
                    const size_t iOut = ia * rB + ib;
                    const double bv = b.elemAsDouble(ib + jb * rB);
                    dst[jOut * rOut + iOut] = av * bv;
                }
            }
        }
    return out;
}

// ── Reductions and products ──────────────────────────────────────────
Value cumsum(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        cumsumScan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }
    const size_t R = x.dims().rows(), C = x.dims().cols();
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    // Per-column inclusive scan — column data is contiguous.
    for (size_t c = 0; c < C; ++c)
        cumsumScan(src + c * R, dst + c * R, R);
    return r;
}

// cumsum along an explicit dim. Output shape equals input shape (this is
// not a reduction). Vector / scalar input ignores dim and walks linearly.
Value cumsum(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    if (dim <= 0) return cumsum(mr, x);
    if (x.dims().isVector() || x.isScalar()) return cumsum(mr, x);

    const int d = detail::resolveDim(x, dim, "cumsum");
    const auto &dd = x.dims();

    // ND fallback for rank ≥ 4: per-slice scan along axis d-1. Inner
    // block size B = prod(dim[0..d-2]); outer count O = prod(dim[d..]).
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error("cumsum: rank exceeds 32",
                         0, 0, "cumsum", "", "m:cumsum:tooManyDims");
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
                cumsumScan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc += src[base + k * B];
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
        // dim=1: scan down each column. Column data is contiguous so
        // route through the SIMD prefix-sum kernel.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                cumsumScan(src + base, dst + base, R);
            }
    } else if (d == 2) {
        // Walk across columns for each (row, page). Stride = R.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                const size_t pageBase = pp * R * C;
                for (size_t c = 0; c < C; ++c) {
                    s += src[pageBase + c * R + rr];
                    dst[pageBase + c * R + rr] = s;
                }
            }
    } else if (d == 3) {
        // Walk through pages for each (row, col). Stride = R*C.
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                for (size_t pp = 0; pp < P; ++pp) {
                    s += src[pp * R * C + c * R + rr];
                    dst[pp * R * C + c * R + rr] = s;
                }
            }
    }
    return r;
}

// ── Generic cumulative kernel for cumprod / cummax / cummin ─────────
//
// Op is a binary functor (double, double) -> double. Init is the value
// the running accumulator starts at; for cumulative ops we instead seed
// with the first element of the slice, but seeding behavior is still
// captured by Op (e.g. cumprod could just use init=1.0 multiplicative).
// We use the seed-with-first style so cummax / cummin work without
// needing to know +/- infinity for the initial value.
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
Value cumImpl(std::pmr::memory_resource *mr, const Value &x, int dim,
               Op op, const char *fn)
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

// cumprod / cummax / cummin: SIMD prefix-op kernels in
// backends/MStdCumSum_{simd,portable}.cpp handle vector input and the
// dim=1 (column) path where access is contiguous. For dim=2/3 the
// strided access pattern doesn't benefit from SIMD; cumImpl's scalar
// cumKernel still handles those (with the same Op as before).
namespace {

using ScanFn = void (*)(const double *, double *, std::size_t);

template <typename Op>
Value cumScanDispatch(std::pmr::memory_resource *mr, const Value &x, int dim,
                       ScanFn scan, Op scalarOp, const char *fn)
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
                         0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");
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

Value cumprod(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return cumScanDispatch(mr, x, dim, cumprodScan,
                           [](double a, double b) { return a * b; }, "cumprod");
}

Value cummax(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    // NaN propagation: MATLAB cummax skips NaN if 'omitnan' is passed
    // and propagates otherwise. Default = 'omitnan' since R2018a; we
    // skip NaN here, treating them as identity.
    return cumScanDispatch(mr, x, dim, cummaxScan,
                           [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::max(a, b);
                           }, "cummax");
}

Value cummin(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return cumScanDispatch(mr, x, dim, cumminScan,
                           [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::min(a, b);
                           }, "cummin");
}

// ── diff: discrete difference ────────────────────────────────────────
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

Value makeDiffOutput(std::pmr::memory_resource *mr, const Dims &srcDims, int d, size_t step)
{
    const int nd = srcDims.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("diff: rank exceeds 32",
                     0, 0, "diff", "", "m:diff:tooManyDims");
    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = srcDims.dim(i);
    outDims[d - 1] = (outDims[d - 1] >= step) ? outDims[d - 1] - step : 0;
    return Value::matrixND(outDims, nd, ValueType::DOUBLE, mr);
}

Value copyToDouble(std::pmr::memory_resource *mr, const Value &x)
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

} // namespace

Value diff(std::pmr::memory_resource *mr, const Value &x, int n, int dim)
{
    if (n < 0)
        throw Error("diff: order n must be non-negative",
                     0, 0, "diff", "", "m:diff:badOrder");

    if (n == 0) {
        // Identity copy preserving DOUBLE shape.
        return copyToDouble(mr, x);
    }

    // Scalar: MATLAB returns 1×0 empty.
    if (x.isScalar())
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    const int d = detail::resolveDim(x, dim, "diff");
    const auto &dd = x.dims();
    const size_t sliceLen = (d >= 1 && d <= dd.ndim()) ? dd.dim(d - 1) : 1;

    // If n collapses or exceeds the dim, return correctly-shaped empty.
    if (sliceLen <= static_cast<size_t>(n))
        return makeDiffOutput(mr, dd, d, sliceLen);

    // Promote integer/logical to DOUBLE first (consistent with cumsum).
    Value cur = copyToDouble(mr, x);

    for (int pass = 0; pass < n; ++pass) {
        const auto &curDims = cur.dims();
        auto out = makeDiffOutput(mr, curDims, d, 1);
        diffOnceDouble(cur.doubleData(), out.doubleDataMut(), curDims, d);
        cur = std::move(out);
    }
    return cur;
}

// ── any / all moved to backends/MStdLogicalReductions_{simd,portable}.cpp
//
// MATLAB's any(X) returns true if ANY element is non-zero (NaN counts
// as true since NaN != 0). all(X) returns true if ALL elements are
// non-zero. Empty: any → false, all → true (vacuously). The SIMD
// backend scans LOGICAL bytes and DOUBLE lanes directly with early
// exit (Phase P1 of project_perf_optimization_plan.md).

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

// ── xor (elementwise logical) ────────────────────────────────────────
Value xorOf(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    auto ad = promoteToDouble(a, mr);
    auto bd = promoteToDouble(b, mr);
    auto d = elementwiseDouble(ad, bd,
        [](double aa, double bb) {
            return ((aa != 0.0) != (bb != 0.0)) ? 1.0 : 0.0;
        }, mr);
    if (d.isScalar()) return Value::logicalScalar(d.toScalar() != 0.0, mr);
    auto r = createLike(d, ValueType::LOGICAL, mr);
    for (size_t i = 0; i < d.numel(); ++i)
        r.logicalDataMut()[i] = (d.doubleData()[i] != 0.0) ? 1 : 0;
    return r;
}

Value cross(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.numel() != 3 || b.numel() != 3)
        throw Error("cross requires 3-element vectors",
                     0, 0, "cross", "", "m:cross:badSize");
    auto r = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    r.doubleDataMut()[0] = a.doubleData()[1] * b.doubleData()[2] - a.doubleData()[2] * b.doubleData()[1];
    r.doubleDataMut()[1] = a.doubleData()[2] * b.doubleData()[0] - a.doubleData()[0] * b.doubleData()[2];
    r.doubleDataMut()[2] = a.doubleData()[0] * b.doubleData()[1] - a.doubleData()[1] * b.doubleData()[0];
    return r;
}

Value dot(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.numel() != b.numel())
        throw Error("dot: vectors must have same length",
                     0, 0, "dot", "", "m:dot:lengthMismatch");
    double s = 0;
    for (size_t i = 0; i < a.numel(); ++i)
        s += a.doubleData()[i] * b.doubleData()[i];
    return Value::scalar(s, mr);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void zeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    outs[0] = zerosND(mr, d.data(), d.size());
}

void ones_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    outs[0] = onesND(mr, d.data(), d.size());
}

void eye_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto d = parseDimsArgs(args);
    outs[0] = eye(ctx.engine->resource(), d.rows, d.cols);
}

void size_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("Not enough input arguments",
                     0, 0, "size", "", "m:size:nargin");
    auto *mr = ctx.engine->resource();

    if (args.size() >= 2) {
        outs[0] = size(mr, args[0], static_cast<int>(args[1].toScalar()));
        return;
    }

    if (nargout > 1) {
        const auto &dims = args[0].dims();
        // Multi-output form: [r, c] = size(A) or [r, c, p, ...] = size(A).
        // For ND tensors, dims past nargout-1 are gathered into the last
        // requested output (MATLAB behaviour: extra-dim sizes multiplied
        // into the trailing slot). For dims past actual ndim, return 1.
        for (size_t i = 0; i < nargout && i < outs.size(); ++i) {
            double v;
            if (i + 1 < nargout) {
                v = static_cast<double>(dims.dim(static_cast<int>(i)));
            } else {
                // Last requested output: multiply remaining dims (if any).
                size_t prod = 1;
                for (int j = static_cast<int>(i); j < dims.ndim(); ++j)
                    prod *= dims.dim(j);
                if (dims.ndim() <= static_cast<int>(i)) prod = 1;
                v = static_cast<double>(prod);
            }
            outs[i] = Value::scalar(v, mr);
        }
        return;
    }

    outs[0] = size(mr, args[0]);
}

void length_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("length: requires 1 argument",
                     0, 0, "length", "", "m:length:nargin");
    outs[0] = length(ctx.engine->resource(), args[0]);
}

void numel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("numel: requires 1 argument",
                     0, 0, "numel", "", "m:numel:nargin");
    outs[0] = numel(ctx.engine->resource(), args[0]);
}

void ndims_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ndims: requires 1 argument",
                     0, 0, "ndims", "", "m:ndims:nargin");
    outs[0] = ndims(ctx.engine->resource(), args[0]);
}

void reshape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("reshape: requires at least 2 arguments",
                     0, 0, "reshape", "", "m:reshape:nargin");

    const auto &x = args[0];
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> dims(&scratch);

    // Dims-vector form: reshape(A, [m n p ...]). No [] inference here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        dims = parseDimsArgsND(&scratch, args.subspan(1));
    } else {
        // Scalar-args form: reshape(A, m, n, ...). One [] allowed for
        // dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        dims.assign(dimCount, 1);
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount; ++i) {
            if (args[i + 1].isEmpty()) {
                if (inferPos >= 0)
                    throw Error("reshape: only one dimension may be inferred via []",
                                 0, 0, "reshape", "", "m:reshape:tooManyInferred");
                inferPos = static_cast<int>(i);
            } else {
                dims[i] = static_cast<size_t>(args[i + 1].toScalar());
                knownProd *= dims[i];
            }
        }
        if (inferPos >= 0) {
            if (knownProd == 0 || x.numel() % knownProd != 0)
                throw Error("reshape: size of array must be divisible by product of known dims",
                             0, 0, "reshape", "", "m:reshape:indivisible");
            dims[inferPos] = x.numel() / knownProd;
        }
    }

    // Strip trailing 1s past the 2nd dim (MATLAB convention).
    stripTrailingOnes(dims);
    outs[0] = reshapeND(mr, x, dims.data(), dims.size());
}

void transpose_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("transpose: requires 1 argument",
                     0, 0, "transpose", "", "m:transpose:nargin");
    outs[0] = transpose(ctx.engine->resource(), args[0]);
}

void pagemtimes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto parseFlag = [](const Value &v) -> TranspOp {
        if (!v.isChar() && !v.isString())
            throw Error("pagemtimes: transpose flag must be a string",
                         0, 0, "pagemtimes", "", "m:pagemtimes:flagType");
        const std::string s = v.toString();
        if (s == "none")       return TranspOp::None;
        if (s == "transpose")  return TranspOp::Transpose;
        if (s == "ctranspose") return TranspOp::CTranspose;
        throw Error("pagemtimes: invalid transpose flag '" + s
                     + "' (expected 'none', 'transpose', or 'ctranspose')",
                     0, 0, "pagemtimes", "", "m:pagemtimes:invalidFlag");
    };
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = pagemtimes(mr, args[0], args[1]);
        return;
    }
    if (args.size() == 4) {
        outs[0] = pagemtimes(mr,
                             args[0], parseFlag(args[1]),
                             args[2], parseFlag(args[3]));
        return;
    }
    throw Error("pagemtimes: expected 2 or 4 arguments",
                 0, 0, "pagemtimes", "", "m:pagemtimes:nargin");
}

void diag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diag: requires 1 argument",
                     0, 0, "diag", "", "m:diag:nargin");
    outs[0] = diag(ctx.engine->resource(), args[0]);
}

void sort_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sort: requires 1 argument",
                     0, 0, "sort", "", "m:sort:nargin");
    auto [sorted, idx] = sort(ctx.engine->resource(), args[0]);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void sortrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sortrows: requires at least 1 argument",
                     0, 0, "sortrows", "", "m:sortrows:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto cols = ScratchVec<int>(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const auto &c = args[1];
        if (c.type() == ValueType::CHAR || c.type() == ValueType::STRING)
            throw Error("sortrows: column spec must be numeric",
                         0, 0, "sortrows", "", "m:sortrows:badColType");
        cols.reserve(c.numel());
        for (size_t i = 0; i < c.numel(); ++i) {
            const double v = c.elemAsDouble(i);
            if (v != std::floor(v))
                throw Error("sortrows: column index must be an integer",
                             0, 0, "sortrows", "", "m:sortrows:badCol");
            cols.push_back(static_cast<int>(v));
        }
    }
    auto [sorted, idx] = sortrows(mr, args[0], cols.data(), cols.size());
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void find_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("find: requires 1 argument",
                     0, 0, "find", "", "m:find:nargin");
    outs[0] = find(ctx.engine->resource(), args[0]);
}

void nnz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nnz: requires 1 argument",
                     0, 0, "nnz", "", "m:nnz:nargin");
    outs[0] = nnz(ctx.engine->resource(), args[0]);
}

void nonzeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nonzeros: requires 1 argument",
                     0, 0, "nonzeros", "", "m:nonzeros:nargin");
    outs[0] = nonzeros(ctx.engine->resource(), args[0]);
}

void horzcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = horzcat(ctx.engine->resource(), args.data(), args.size());
}

void vertcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = vertcat(ctx.engine->resource(), args.data(), args.size());
}

void meshgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("meshgrid: requires 2 arguments",
                     0, 0, "meshgrid", "", "m:meshgrid:nargin");
    auto [X, Y] = meshgrid(ctx.engine->resource(), args[0], args[1]);
    outs[0] = std::move(X);
    if (nargout > 1)
        outs[1] = std::move(Y);
}

void ndgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ndgrid: requires at least 2 arguments",
                     0, 0, "ndgrid", "", "m:ndgrid:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        auto [X, Y] = ndgrid(mr, args[0], args[1]);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = ndgrid(mr, args[0], args[1], args[2]);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("ndgrid: 4+ inputs are not yet supported",
                 0, 0, "ndgrid", "", "m:ndgrid:tooManyInputs");
}

void kron_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kron: requires 2 arguments",
                     0, 0, "kron", "", "m:kron:nargin");
    outs[0] = kron(ctx.engine->resource(), args[0], args[1]);
}

void cumsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumsum: requires at least 1 argument",
                     0, 0, "cumsum", "", "m:cumsum:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    outs[0] = (dim > 0) ? cumsum(ctx.engine->resource(), args[0], dim)
                        : cumsum(ctx.engine->resource(), args[0]);
}

#define NK_CUM_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = name(ctx.engine->resource(), args[0], dim);                 \
    }

NK_CUM_REG(cumprod)
NK_CUM_REG(cummax)
NK_CUM_REG(cummin)

void diff_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diff: requires at least 1 argument",
                     0, 0, "diff", "", "m:diff:nargin");
    int n = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double nv = args[1].toScalar();
        if (nv != std::floor(nv) || nv < 0)
            throw Error("diff: order n must be a non-negative integer",
                         0, 0, "diff", "", "m:diff:badOrder");
        n = static_cast<int>(nv);
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = diff(ctx.engine->resource(), args[0], n, dim);
}

#undef NK_CUM_REG

#define NK_LOGICAL_RED_REG(name, fn)                                           \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = fn(ctx.engine->resource(), args[0], dim);                   \
    }

NK_LOGICAL_RED_REG(any, anyOf)
NK_LOGICAL_RED_REG(all, allOf)

#undef NK_LOGICAL_RED_REG

void xor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xor: requires 2 arguments",
                     0, 0, "xor", "", "m:xor:nargin");
    outs[0] = xorOf(ctx.engine->resource(), args[0], args[1]);
}

void cross_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cross: requires 2 arguments",
                     0, 0, "cross", "", "m:cross:nargin");
    outs[0] = cross(ctx.engine->resource(), args[0], args[1]);
}

void dot_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dot: requires 2 arguments",
                     0, 0, "dot", "", "m:dot:nargin");
    outs[0] = dot(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
